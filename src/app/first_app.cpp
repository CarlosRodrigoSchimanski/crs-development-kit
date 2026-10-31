#include "app/first_app.hpp"
#include "engine/rendering/crs_camera.hpp"
#include "engine/rendering/systems/simple_render_system.hpp"
#include "engine/rendering/systems/point_light_system.hpp"
#include "engine/rendering/systems/shadow_render_system.hpp"
#include "engine/physics/crs_physics_system.hpp"
#include "engine/rendering/crs_buffer.hpp"
#include "editor/crs_imgui.hpp"
#include "engine/rendering/crs_shadow_map.hpp"
#include "engine/platform/crs_platform.hpp"
#include "engine/scene/scene_serializer.hpp"
#include "editor/file_dialog.hpp"

#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <filesystem>
#include <iostream>
#include <chrono>
#include <thread>

namespace crsdk {
    //limite do tempo de um frame (evita saltos grandes na física)
    constexpr float MAX_FRAME_TIME = 0.05f;
    //limite de FPS do programa (<= 0 = sem limite)
    constexpr float TARGET_FPS{144.f};
    //cursor do mouse durante o Play (configuração do jogo; este usa câmera girando com o mouse)
    constexpr CursorMode PLAY_CURSOR = CursorMode::Captured;
    //meia largura da caixa de sombra do sol
    constexpr float SHADOW_HALF_SIZE = 10.f;
    //distância da câmera do sol até o centro da caixa
    constexpr float SHADOW_SUN_DISTANCE = 30.f;
    //arquivo da cena do editor
    constexpr const char *SCENE_FILE = "assets/scenes/main.json";
    //pasta onde as janelas de arquivo começam
    constexpr const char *SCENE_FOLDER = "assets/scenes";
    //pasta onde a janela de importar começa
    constexpr const char *MODEL_FOLDER = "assets/models";
    //componentes Python (os .py que o Add Component lista e o Generate Component cria)
    constexpr const char *COMPONENT_FOLDER = "assets/components";

    //vista da câmera do editor de uma cena (pessoal: fora do git): main.json -> main.editor.json
    static std::string editorViewFile(const std::string &sceneFile) {
        std::filesystem::path path{sceneFile};
        path.replace_extension(".editor.json");
        return path.string();
    }

    //posiciona a câmera do sol em volta do centro
    static void updateShadowCamera(CrsCamera &shadowCamera, glm::vec3 center, glm::vec3 lightDirection, uint32_t shadowMapSize) {
        shadowCamera.setViewDirection(center - lightDirection * SHADOW_SUN_DISTANCE, lightDirection);
        //alinha à grade de texels (a borda da sombra não treme ao mover)
        float texelWorldSize = (2.f * SHADOW_HALF_SIZE) / static_cast<float>(shadowMapSize);
        glm::vec3 centerInLightView = glm::vec3{shadowCamera.getView() * glm::vec4{center, 1.f}};
        glm::vec2 snapped = glm::floor(glm::vec2{centerInLightView} / texelWorldSize) * texelWorldSize;
        glm::vec3 snapOffset = glm::mat3{shadowCamera.getInverseView()}
            * glm::vec3{snapped - glm::vec2{centerInLightView}, 0.f};
        center += snapOffset;
        shadowCamera.setViewDirection(center - lightDirection * SHADOW_SUN_DISTANCE, lightDirection);
    }

    //desregistra o refresh callback ao sair do run()
    struct RefreshCallbackGuard {
        CrsWindow &window;
        ~RefreshCallbackGuard() { window.setRefreshCallback(nullptr); }
    };

    //espera a GPU terminar ao sair do run()
    struct DeviceIdleGuard {
        CrsDevice &device;
        //sem vkCheck de propósito (destrutor não pode lançar exceção)
        ~DeviceIdleGuard() { vkDeviceWaitIdle(device.device()); }
    };

    FirstApp::FirstApp() {
        //pool e layout dos descriptors globais
        globalPool = CrsDescriptorPool::Builder{crsDevice}
            .setMaxSets(CrsSwapChain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, CrsSwapChain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, CrsSwapChain::MAX_FRAMES_IN_FLIGHT)
            //2 storage buffers por set: luzes (binding 2) e ossos (binding 3)
            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 * CrsSwapChain::MAX_FRAMES_IN_FLIGHT)
            .build();
        globalSetLayout = CrsDescriptorSetLayout::Builder{crsDevice}
            .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
            .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
            .addBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
            .build();

        //buffers do UBO
        uboBuffers.resize(CrsSwapChain::MAX_FRAMES_IN_FLIGHT);
        for(size_t i = 0; i < uboBuffers.size(); i++) {
            uboBuffers[i] = std::make_unique<CrsBuffer>(
                crsDevice,
                sizeof(GlobalUbo),
                1,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                crsDevice.properties.limits.minUniformBufferOffsetAlignment
            );
            uboBuffers[i]->map();
        }

        //sistemas de render e UI
        VkFormat colorFormat = crsRenderer.getSwapChainImageFormat();
        VkFormat depthFormat = crsRenderer.getSwapChainDepthFormat();
        VkDescriptorSetLayout globalLayout = globalSetLayout->getDescriptorSetLayout();
        simpleRenderSystem = std::make_unique<SimpleRenderSystem>(crsDevice, colorFormat, depthFormat, globalLayout);
        pointLightSystem = std::make_unique<PointLightSystem>(crsDevice, colorFormat, depthFormat, globalLayout);
        shadowRenderSystem = std::make_unique<ShadowRenderSystem>(crsDevice, shadowMap, globalLayout);
        gizmoRenderSystem = std::make_unique<GizmoRenderSystem>(crsDevice, colorFormat, depthFormat, globalLayout);
        selectionOutlineSystem = std::make_unique<SelectionOutlineSystem>(crsDevice, colorFormat, depthFormat, globalLayout);
        skinningSystem = std::make_unique<SkinningSystem>(crsDevice, animationLibrary);
        crsImgui = std::make_unique<CrsImgui>(crsWindow, crsDevice, colorFormat, depthFormat, crsRenderer.getImageCount());

        //descriptor sets globais: ligam UBO, shadow map, luzes e ossos aos shaders
        globalDescriptorSets.resize(CrsSwapChain::MAX_FRAMES_IN_FLIGHT);
        for (size_t i = 0; i < globalDescriptorSets.size(); i++) {
            auto bufferInfo = uboBuffers[i]->descriptorInfo();
            auto lightBufferInfo = pointLightSystem->lightBufferInfo(static_cast<int>(i));
            auto boneBufferInfo = skinningSystem->boneBufferInfo(static_cast<int>(i));
            VkDescriptorImageInfo shadowMapInfo{};
            shadowMapInfo.sampler = shadowMap.getSampler();
            shadowMapInfo.imageView = shadowMap.getImageView();
            //mesmo layout da barreira de saída do passe de sombra
            shadowMapInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

            CrsDescriptorWriter(*globalSetLayout, *globalPool)
                .writeBuffer(0, &bufferInfo)
                .writeImage(1, &shadowMapInfo)
                .writeBuffer(2, &lightBufferInfo)
                .writeBuffer(3, &boneBufferInfo)
                .build(globalDescriptorSets[i]);
        }

        //projeção da câmera do sol (fixa)
        shadowCamera.setOrthographicProjection(-SHADOW_HALF_SIZE, SHADOW_HALF_SIZE, -SHADOW_HALF_SIZE, SHADOW_HALF_SIZE, 0.1f, 2.f * SHADOW_SUN_DISTANCE);

        //cena inicial: a do arquivo; sem arquivo (ou com erro) monta a de exemplo do código
        if (!std::filesystem::exists(SCENE_FILE) || !openSceneFile(SCENE_FILE)) {
            loadGameObjects();
            sceneFile = SCENE_FILE;
        }

        //o que os scripts Python enxergam pelo módulo crsdk (a cena troca no Play)
        scripting.setScene(&crsScene);
        scripting.setInput(&crsInput);
        scripting.setWindow(&crsWindow);
        scripting.addImportFolder(COMPONENT_FOLDER);
        scripting.addImportFolder("assets/scripts");
        refreshComponents();
    }

    void FirstApp::run() {
        //relógio inicial
        currentTime = std::chrono::high_resolution_clock::now();

        //guards da saída: desregistram o callback e esperam a GPU
        RefreshCallbackGuard refreshGuard{crsWindow};
        DeviceIdleGuard deviceIdleGuard{crsDevice};
        //redesenha durante o arrasto da borda (Windows)
        if (platform::needsRedrawDuringResize()) {
            crsWindow.setRefreshCallback([this]() { drawFrame(); });
        }

        while (!crsWindow.shouldClose()) {
            //eventos da janela e um frame
            glfwPollEvents();
            drawFrame();

            //limitador de FPS (<= 0 = sem limite)
            //if constexpr: o limite é constante, o compilador decide na compilação
            if constexpr (TARGET_FPS > 0.f) {
                float targetFrameDuration = 1.f / TARGET_FPS;
                //dorme 1ms por vez e termina em espera ativa (precisão)
                float elapsed = std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - currentTime).count();
                while (elapsed < targetFrameDuration) {
                    if (targetFrameDuration - elapsed > 0.002f) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                    elapsed = std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - currentTime).count();
                }
            }
        }
    }

    void FirstApp::loadGameObjects() {
        //cubo de teste (dinâmico)
        std::shared_ptr<CrsModel> crsModel = modelCache.load("assets/models/iron_cube.glb");
        auto& cube = crsScene.createGameObject();
        cube.name = "cube";
        cube.model = crsModel;
        cube.transform.translation = {0.f, 1.5f, 0.f};
        cube.transform.scale = glm::vec3{1.f, 1.f, 1.f};
        cube.rigidBody.emplace();

        //personagem: cápsula com a física (visível no Play até o visual entrar como filho)
        crsModel = modelCache.load("assets/editor/character.glb");
        auto& player = crsScene.createGameObject();
        player.name = "Player";
        player.model = crsModel;
        player.editorOnly = true;  //só a física: gizmo no editor, invisível no Play
        player.transform.translation = {3.f, 2.f, 0.f};
        player.transform.scale = {1.f, 1.f, 1.f};
        player.rigidBody.emplace();
        player.rigidBody->shape = ColliderShape::Capsule;
        player.rigidBody->radius = 0.4f;
        player.rigidBody->height = 1.74f;
        player.rigidBody->center = {0.f, 0.87f, 0.f};  //cápsula acima dos pés
        player.rigidBody->mass = 70.f;
        player.character.emplace();
        //quem decide pra onde ele anda: o componente Python (WASD relativo à câmera)
        player.scripts.push_back({"controle_jogador", "ControleJogador"});

        //visual do personagem: filho da cápsula (anda junto com ela)
        crsModel = modelCache.load("assets/models/basicFemaleTest.glb");
        auto& playerVisual = crsScene.createGameObject();
        playerVisual.name = "PlayerMesh";
        playerVisual.model = crsModel;
        playerVisual.parentId = player.getId();
        playerVisual.transform.rotation.y = glm::radians(180.f);
        playerVisual.faceMovement.emplace();
        playerVisual.animator.emplace();
        playerVisual.animator->animations = {
            "assets/animations/idle_01.glb",
            "assets/animations/walking_01.glb",
            "assets/animations/running_01.glb",
            "assets/animations/jump_01.glb",
            "assets/animations/fall_01.glb",
        };
        playerVisual.animator->clip = "idle_01";
        playerVisual.characterAnimation.emplace();
        playerVisual.characterAnimation->idle = "idle_01";
        playerVisual.characterAnimation->walk = "walking_01";
        playerVisual.characterAnimation->run = "running_01";
        playerVisual.characterAnimation->fall = "fall_01";

        //pivô da câmera (um Empty): filho do personagem, na altura da cabeça; o mouse vai girar ele
        crsModel = modelCache.load("assets/editor/empty.glb");
        auto& cameraPivot = crsScene.createGameObject();
        cameraPivot.name = "CameraPivot";
        cameraPivot.model = crsModel;
        cameraPivot.cameraRig.emplace();
        cameraPivot.editorOnly = true;  //gizmo só no editor (provisório: a engine vai gerar a cruz de eixos)
        cameraPivot.parentId = player.getId();
        cameraPivot.transform.translation = {0.f, 1.6f, 0.f};  //1,6 acima dos pés

        //câmera de jogo
        crsModel = modelCache.load("assets/editor/camera.glb");
        auto& gameCamera = crsScene.createGameObject();
        gameCamera.name = "Camera";
        gameCamera.model = crsModel;
        gameCamera.editorOnly = true;  //gizmo só no editor
        gameCamera.parentId = cameraPivot.getId();
        gameCamera.transform.translation = {0.f, 0.f, 4.f};
        gameCamera.camera.emplace();
        crsScene.setActiveCamera(gameCamera.getId());

        //chão (estático)
        crsModel = modelCache.load("assets/models/plane.glb");
        auto& floor = crsScene.createGameObject();
        floor.name = "Floor";
        floor.model = crsModel;
        floor.transform.translation = {0.f, 0.f, 0.f};
        floor.transform.scale = glm::vec3{10.f, 1.f, 10.f};
        floor.rigidBody.emplace();
        floor.rigidBody->type = BodyType::Static;
        floor.rigidBody->halfExtents = {1.f, 0.f, 1.f};
    }

    void FirstApp::startPlay() {
        //a cena do editor fica intacta
        playScene = crsScene;
        //cada Play começa com a física zerada (os corpos do Play anterior eram de outra cópia da cena)
        physicsSystem.clear();
        scripting.setScene(&*playScene);
        scripting.forgetProjectModules();
        scripting.startComponents(*playScene);
        crsWindow.setCursorMode(PLAY_CURSOR);
        crsInput.skipNextMouseDelta();
    }

    void FirstApp::stopPlay() {
        scripting.stopComponents();
        scripting.setScene(&crsScene);
        playScene.reset();
        crsWindow.setCursorMode(CursorMode::Visible);
        crsInput.skipNextMouseDelta();
        //algum .py pode ter sido editado durante o Play
        refreshComponents();
    }

    void FirstApp::refreshComponents() {
        componentCatalog = scripting.findComponents(COMPONENT_FOLDER);
        showScriptErrors();
    }

    void FirstApp::generateComponent(const std::string &className) {
        try {
            std::string module = scripting.createComponentFile(COMPONENT_FOLDER, className);
            refreshComponents();
            //já preso no objeto selecionado: é pra ele que o componente foi pedido
            if (CrsGameObject *selected = editor.getSelected(crsScene)) {
                selected->scripts.push_back({module, className});
            }
            sceneFileStatus = "Criado " + std::string{COMPONENT_FOLDER} + "/" + module + ".py";
        } catch (const std::exception &e) {
            sceneFileStatus = std::string{"Erro ao criar o componente: "} + e.what();
        }
    }

    void FirstApp::showScriptErrors() {
        for (const std::string &error : scripting.takeErrors()) {
            std::cerr << "[componente] " << error << std::endl;
            sceneFileStatus = "Erro no componente " + error;
        }
    }

    void FirstApp::updatePlayMode() {
        //não dispara enquanto digita no ImGui
        bool typing = ImGui::GetIO().WantCaptureKeyboard;
        if (!playScene && !typing && crsInput.wasPressed(GLFW_KEY_P)) {
            startPlay();
        }
        //tecla à esquerda do 1 (GRAVE_ACCENT no GLFW)
        else if (playScene && crsInput.wasPressed(GLFW_KEY_GRAVE_ACCENT)) {
            stopPlay();
        }
    }

    void FirstApp::updateSceneFile() {
        //só no editor, e não enquanto digita no ImGui
        if (playScene || ImGui::GetIO().WantCaptureKeyboard) return;
        bool ctrl = crsInput.isDown(GLFW_KEY_LEFT_CONTROL) || crsInput.isDown(GLFW_KEY_RIGHT_CONTROL);
        bool shift = crsInput.isDown(GLFW_KEY_LEFT_SHIFT) || crsInput.isDown(GLFW_KEY_RIGHT_SHIFT);
        if (!ctrl) return;
        if (crsInput.wasPressed(GLFW_KEY_S)) {
            if (shift) saveSceneAsDialog();
            else saveSceneFile();
        }
        if (crsInput.wasPressed(GLFW_KEY_O)) {
            openSceneDialog();
        }
    }

    bool FirstApp::openSceneFile(const std::string &filepath) {
        try {
            //lê numa cena nova: se der erro no meio, a do editor não é tocada
            crsScene = loadScene(filepath, [this](const std::string &path) { return modelCache.load(path); });
            sceneFile = filepath;
            sceneFileStatus = "Cena aberta de " + filepath;
            editor.clearSelection();
            editor.loadView(editorViewFile(filepath));
            return true;
        } catch (const std::exception &e) {
            sceneFileStatus = std::string{"Erro ao abrir: "} + e.what();
            return false;
        }
    }

    void FirstApp::saveSceneFile() {
        try {
            saveScene(crsScene, sceneFile);
            editor.saveView(editorViewFile(sceneFile));
            sceneFileStatus = "Cena salva em " + sceneFile;
        } catch (const std::exception &e) {
            sceneFileStatus = std::string{"Erro ao salvar: "} + e.what();
        }
    }

    void FirstApp::openSceneDialog() {
        try {
            if (auto chosen = openFileDialog("Cena", "json", SCENE_FOLDER)) {
                openSceneFile(*chosen);
            }
        } catch (const std::exception &e) {
            sceneFileStatus = e.what();
        }
    }

    void FirstApp::importModelDialog() {
        try {
            auto chosen = openFileDialog("Modelo glTF", "glb", MODEL_FOLDER);
            if (!chosen) return;
            //carrega antes de criar: se o arquivo der erro, a cena não ganha um objeto vazio
            std::shared_ptr<CrsModel> model = modelCache.load(*chosen);
            auto& obj = crsScene.createGameObject();
            obj.name = std::filesystem::path{*chosen}.stem().string();
            obj.model = model;
            obj.transform.translation = editor.getFocusPoint();
            editor.select(obj.getId());
            sceneFileStatus = "Importado: " + *chosen;
        } catch (const std::exception &e) {
            sceneFileStatus = std::string{"Erro ao importar: "} + e.what();
        }
    }

    void FirstApp::addObject(AddKind kind) {
        try {
            //gizmo de editor de cada tipo (provisório: a engine vai gerar essas formas)
            std::shared_ptr<CrsModel> model{};
            if (kind == AddKind::Empty) model = modelCache.load("assets/editor/empty.glb");
            if (kind == AddKind::Camera) model = modelCache.load("assets/editor/camera.glb");

            auto& obj = crsScene.createGameObject();
            obj.model = model;
            obj.transform.translation = editor.getFocusPoint();
            switch (kind) {
                case AddKind::Empty:
                    obj.name = "Empty";
                    obj.editorOnly = true;
                    break;
                case AddKind::Camera:
                    obj.name = "Camera";
                    obj.editorOnly = true;
                    obj.camera.emplace();
                    break;
                case AddKind::PointLight:
                    obj.name = "Luz";
                    obj.color = glm::vec3{1.f};
                    obj.transform.scale.x = 0.1f;  //raio da bolinha desenhada
                    obj.pointLight.emplace();
                    break;
                case AddKind::None:
                    break;
            }
            editor.select(obj.getId());
        } catch (const std::exception &e) {
            sceneFileStatus = std::string{"Erro ao adicionar: "} + e.what();
        }
    }

    void FirstApp::saveSceneAsDialog() {
        try {
            std::string name = std::filesystem::path{sceneFile}.filename().string();
            if (auto chosen = saveFileDialog("Cena", "json", SCENE_FOLDER, name)) {
                sceneFile = *chosen;
                saveSceneFile();
            }
        } catch (const std::exception &e) {
            sceneFileStatus = e.what();
        }
    }

    CrsScene& FirstApp::activeScene() {
        return playScene.has_value() ? *playScene : crsScene;
    }

    void FirstApp::drawStatsWindow(float fps) {
        //abaixo do menu do editor (WorkPos = o que sobra da janela tirando a barra de menu)
        const ImGuiViewport* screen = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(screen->WorkPos.x + 10.f, screen->WorkPos.y + 10.f));
        ImGui::SetNextWindowBgAlpha(0.35f);
        ImGui::Begin(
            "stats",
            nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoMove
        );
        ImGui::Text("FPS: %.0f", fps);
        ImGui::Text("Mode %s",playScene ? "Play" : "Editor");
        CrsGameObject* selected = editor.getSelected(crsScene);
        if (selected) {
            ImGui::Text("Selecionado: %s", selected->name.c_str());
        } else {
            ImGui::Text("Selecionado: nenhum");
        }

        ImGui::Text("Mouse delta: %.1f, %.1f", crsInput.getMouseDelta().x, crsInput.getMouseDelta().y);
        //no Play: quanto cada componente Python gasta por frame (pra achar o pesado)
        if (playScene) {
            for (const ComponentTiming &timing : scripting.componentTimings()) {
                ImGui::Text("%.3f ms  %s", timing.milliseconds, timing.label.c_str());
            }
        }
        if (!sceneFileStatus.empty()) {
            ImGui::Text("%s", sceneFileStatus.c_str());
        }

        ImGui::End();
    }

    VkRect2D FirstApp::computeSceneArea() const {
        VkExtent2D extent = crsRenderer.getSwapChainExtent();
        if (playScene) return VkRect2D{{0, 0}, extent};
        return editor.sceneArea(extent);
    }

    CrsGameObject* FirstApp::findGameCamera() {
        if (!playScene) return nullptr;
        auto id = playScene->getActiveCamera();
        if (!id) return nullptr;
        return playScene->findGameObjectById(*id);
    }

    void FirstApp::updateCamera(CrsGameObject* gameCamera, float aspect) {
        if (gameCamera) {
            //Posição e frente no mundo
            glm::mat4 world = playScene->worldMatrix(*gameCamera);
            glm::vec3 forward = -glm::normalize(glm::vec3{world[2]});  //a câmera olha pro -Z dela
            camera.setViewDirection(glm::vec3{world[3]}, forward);
            const CameraComponent& lens = *gameCamera->camera;
            camera.setPerspectiveProjection(lens.fovY, aspect, lens.nearPlane, lens.farPlane);
            return;
        }
        //editor: orbital
        if (!playScene) {
            editor.updateCamera(crsWindow, camera);
        }
        camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 100.f);
    }

    glm::mat4 FirstApp::computeLightSpaceMatrix(CrsGameObject* gameCamera, glm::vec3 lightDirection, uint32_t shadowMapSize) {
        //centro: alvo da orbital, ou à frente da câmera de jogo
        glm::vec3 shadowCenter = editor.getFocusPoint();
        if (gameCamera) {
            glm::mat4 world = playScene->worldMatrix(*gameCamera);
            glm::vec3 forward = -glm::normalize(glm::vec3{world[2]});  //a câmera olha pro -Z dela
            shadowCenter = glm::vec3{world[3]} + forward * SHADOW_HALF_SIZE;
        }
        updateShadowCamera(shadowCamera, shadowCenter, lightDirection, shadowMapSize);
        return shadowCamera.getProjection() * shadowCamera.getView();
    }

    void FirstApp::renderFrame(FrameInfo& frameInfo) {
        //sombra primeiro: o passe principal lê o shadow map
        shadowRenderSystem->renderShadowMap(frameInfo);
        //máscara do selecionado (fora de qualquer passe, como a sombra); no Play não tem seleção
        const CrsGameObject *selected = frameInfo.playing ? nullptr : editor.getSelected(crsScene);
        selectionOutlineSystem->renderMask(frameInfo, selected, crsRenderer.getSwapChainExtent(), sceneArea);
        crsRenderer.beginSwapChainRendering(frameInfo.commandBuffer);
        //a cena fica no retangulo
        crsRenderer.setSceneArea(frameInfo.commandBuffer, sceneArea);
        simpleRenderSystem->renderGameObjects(frameInfo);
        pointLightSystem->render(frameInfo);

        //gizmos: transparentes, depois do que é opaco
        if (!frameInfo.playing) {
            gizmoRenderSystem->render(frameInfo);
        }
        //aura por cima da cena e dos gizmos, embaixo da interface
        selectionOutlineSystem->renderOutline(frameInfo);

        crsImgui->render(frameInfo.commandBuffer);
        crsRenderer.endSwapChainRendering(frameInfo.commandBuffer);
    }

    void FirstApp::updateGlobalUbo(FrameInfo& frameInfo, CrsGameObject* gameCamera) {
        //câmera
        GlobalUbo ubo{};
        ubo.projection = camera.getProjection();
        ubo.view = camera.getView();
        ubo.inverseView = camera.getInverseView();

        //configuração de sombra
        glm::vec3 lightDirection = -glm::normalize(glm::vec3{ubo.sunDirection});
        ubo.lightSpaceMatrix = computeLightSpaceMatrix(gameCamera, lightDirection, shadowMap.getExtent().width);

        //point lights da cena
        if (pointLightSystem->update(frameInfo, ubo)) {
            //buffer de luzes cresceu: atualiza o descriptor
            auto lightBufferInfo = pointLightSystem->lightBufferInfo(frameInfo.frameIndex);
            CrsDescriptorWriter(*globalSetLayout, *globalPool)
                .writeBuffer(2, &lightBufferInfo)
                .overwrite(globalDescriptorSets[frameInfo.frameIndex]);
        }

        //ossos dos objetos animados
        if (skinningSystem->update(frameInfo.crsScene, frameInfo.frameIndex)) {
            //buffer de ossos cresceu: atualiza o descriptor
            auto boneBufferInfo = skinningSystem->boneBufferInfo(frameInfo.frameIndex);
            CrsDescriptorWriter(*globalSetLayout, *globalPool)
                .writeBuffer(3, &boneBufferInfo)
                .overwrite(globalDescriptorSets[frameInfo.frameIndex]);
        }

        //envia pra GPU
        uboBuffers[frameInfo.frameIndex]->writeToBuffer(&ubo);
        uboBuffers[frameInfo.frameIndex]->flush();
    }

    float FirstApp::updateFrameTime() {
        auto newTime = std::chrono::high_resolution_clock::now();
        float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
        currentTime = newTime;
        frameTime = glm::min(frameTime, MAX_FRAME_TIME);

        //FPS com média móvel (não treme)
        if (frameTime > 0.f) {
            float instantFps = 1.f / frameTime;
            displayFps = displayFps <= 0.f ? instantFps : glm::mix(displayFps, instantFps, 0.1f);
        }
        return frameTime;
    }

    void FirstApp::drawFrame() {
        //trava contra reentrada (refresh callback no meio do frame)
        if (drawingFrame) return;
        drawingFrame = true;

        float frameTime = updateFrameTime();

        //teclado: lido antes de qualquer coisa do frame
        crsInput.update(crsWindow);
        //P / ' alternam editor e Play
        updatePlayMode();
        //Ctrl+S salva a cena
        updateSceneFile();
        //clique seleciona (só no editor)
        if (!playScene) {
            editor.updateSelection(crsScene, crsInput, camera, sceneArea);
            editor.updateShortcuts(crsInput);
        }

        //câmera de jogo
        CrsGameObject* gameCamera = findGameCamera();

        //personagens e física (só no Play)
        if (playScene) {
            //componentes Python primeiro: eles decidem o comportamento, os sistemas abaixo executam
            scripting.updateComponents(frameTime);
            showScriptErrors();

            cameraRigSystem.update(*playScene, crsInput.getMouseDelta());
            characterController.update(*playScene);
            physicsSystem.update(*playScene, frameTime);
            characterController.faceMovement(*playScene, frameTime);
            characterAnimationSystem.update(*playScene);
            cameraRigSystem.placeCamera(*playScene);
            animatorSystem.update(*playScene, frameTime);
        }

        if (auto commandBuffer = crsRenderer.beginFrame()) {
            int frameIndex = crsRenderer.getFrameIndex();

            sceneArea = computeSceneArea();
            float sceneAspect = static_cast<float>(sceneArea.extent.width) / static_cast<float>(sceneArea.extent.height);
            updateCamera(gameCamera, sceneAspect);

            FrameInfo frameInfo{
                frameIndex,
                frameTime,
                commandBuffer,
                camera,
                globalDescriptorSets[frameIndex],
                activeScene(),
                playScene.has_value()
            };

            updateGlobalUbo(frameInfo, gameCamera);

            //UI
            crsImgui->newFrame();
            editor.beginFrame();
            drawStatsWindow(displayFps);

            //interface do editor (só fora do Play)
            if (!playScene) {
                EditorRequests requests = editor.drawUi(crsScene, camera, sceneArea, componentCatalog);
                if (requests.refreshComponents) refreshComponents();
                if (!requests.generateComponent.empty()) generateComponent(requests.generateComponent);
                if (requests.openScene) openSceneDialog();
                if (requests.saveScene) saveSceneFile();
                if (requests.saveSceneAs) saveSceneAsDialog();
                if (requests.importModel) importModelDialog();
                if (requests.add != AddKind::None) addObject(requests.add);
                if (requests.quit) glfwSetWindowShouldClose(crsWindow.getGLFWwindow(), GLFW_TRUE);
            }

            //render
            renderFrame(frameInfo);
            crsRenderer.endFrame();
        }

        drawingFrame = false;
    }
}