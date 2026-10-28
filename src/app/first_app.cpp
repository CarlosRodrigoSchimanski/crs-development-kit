#include "app/first_app.hpp"
#include "engine/rendering/lve_camera.hpp"
#include "engine/rendering/systems/simple_render_system.hpp"
#include "engine/rendering/systems/point_light_system.hpp"
#include "engine/rendering/systems/shadow_render_system.hpp"
#include "engine/physics/lve_physics_system.hpp"
#include "engine/rendering/lve_buffer.hpp"
#include "editor/lve_imgui.hpp"
#include "engine/rendering/lve_shadow_map.hpp"
#include "engine/platform/lve_platform.hpp"

#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <chrono>
#include <thread>

namespace lve {
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

    //posiciona a câmera do sol em volta do centro
    static void updateShadowCamera(LveCamera &shadowCamera, glm::vec3 center, glm::vec3 lightDirection, uint32_t shadowMapSize) {
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
        LveWindow &window;
        ~RefreshCallbackGuard() { window.setRefreshCallback(nullptr); }
    };

    //espera a GPU terminar ao sair do run()
    struct DeviceIdleGuard {
        LveDevice &device;
        //sem vkCheck de propósito (destrutor não pode lançar exceção)
        ~DeviceIdleGuard() { vkDeviceWaitIdle(device.device()); }
    };

    FirstApp::FirstApp() {
        //pool e layout dos descriptors globais
        globalPool = LveDescriptorPool::Builder{lveDevice}
            .setMaxSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LveSwapChain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LveSwapChain::MAX_FRAMES_IN_FLIGHT)
            //2 storage buffers por set: luzes (binding 2) e ossos (binding 3)
            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 * LveSwapChain::MAX_FRAMES_IN_FLIGHT)
            .build();
        globalSetLayout = LveDescriptorSetLayout::Builder{lveDevice}
            .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
            .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
            .addBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
            .build();

        //buffers do UBO
        uboBuffers.resize(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
        for(size_t i = 0; i < uboBuffers.size(); i++) {
            uboBuffers[i] = std::make_unique<LveBuffer>(
                lveDevice,
                sizeof(GlobalUbo),
                1,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                lveDevice.properties.limits.minUniformBufferOffsetAlignment
            );
            uboBuffers[i]->map();
        }

        //sistemas de render e UI
        VkFormat colorFormat = lveRenderer.getSwapChainImageFormat();
        VkFormat depthFormat = lveRenderer.getSwapChainDepthFormat();
        VkDescriptorSetLayout globalLayout = globalSetLayout->getDescriptorSetLayout();
        simpleRenderSystem = std::make_unique<SimpleRenderSystem>(lveDevice, colorFormat, depthFormat, globalLayout);
        pointLightSystem = std::make_unique<PointLightSystem>(lveDevice, colorFormat, depthFormat, globalLayout);
        shadowRenderSystem = std::make_unique<ShadowRenderSystem>(lveDevice, shadowMap, globalLayout);
        gizmoRenderSystem = std::make_unique<GizmoRenderSystem>(lveDevice, colorFormat, depthFormat, globalLayout);
        skinningSystem = std::make_unique<SkinningSystem>(lveDevice);
        lveImgui = std::make_unique<LveImgui>(lveWindow, lveDevice, colorFormat, depthFormat, lveRenderer.getImageCount());

        //descriptor sets globais: ligam UBO, shadow map, luzes e ossos aos shaders
        globalDescriptorSets.resize(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
        for (size_t i = 0; i < globalDescriptorSets.size(); i++) {
            auto bufferInfo = uboBuffers[i]->descriptorInfo();
            auto lightBufferInfo = pointLightSystem->lightBufferInfo(static_cast<int>(i));
            auto boneBufferInfo = skinningSystem->boneBufferInfo(static_cast<int>(i));
            VkDescriptorImageInfo shadowMapInfo{};
            shadowMapInfo.sampler = shadowMap.getSampler();
            shadowMapInfo.imageView = shadowMap.getImageView();
            //mesmo layout da barreira de saída do passe de sombra
            shadowMapInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

            LveDescriptorWriter(*globalSetLayout, *globalPool)
                .writeBuffer(0, &bufferInfo)
                .writeImage(1, &shadowMapInfo)
                .writeBuffer(2, &lightBufferInfo)
                .writeBuffer(3, &boneBufferInfo)
                .build(globalDescriptorSets[i]);
        }

        //projeção da câmera do sol (fixa)
        shadowCamera.setOrthographicProjection(-SHADOW_HALF_SIZE, SHADOW_HALF_SIZE, -SHADOW_HALF_SIZE, SHADOW_HALF_SIZE, 0.1f, 2.f * SHADOW_SUN_DISTANCE);

        //cena inicial
        loadGameObjects();
    }

    void FirstApp::run() {
        //relógio inicial
        currentTime = std::chrono::high_resolution_clock::now();

        //guards da saída: desregistram o callback e esperam a GPU
        RefreshCallbackGuard refreshGuard{lveWindow};
        DeviceIdleGuard deviceIdleGuard{lveDevice};
        //redesenha durante o arrasto da borda (Windows)
        if (platform::needsRedrawDuringResize()) {
            lveWindow.setRefreshCallback([this]() { drawFrame(); });
        }

        while (!lveWindow.shouldClose()) {
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
        std::shared_ptr<LveModel> lveModel = LveModel::createModelFromFile(lveDevice, "models/iron_cube.glb");
        auto& cube = lveScene.createGameObject();
        cube.name = "cube";
        cube.model = lveModel;
        cube.transform.translation = {0.f, 0.f, 0.f};
        cube.transform.scale = glm::vec3{1.f, 1.f, 1.f};
        cube.rigidBody.emplace();

        //personagem: cápsula com a física (visível no Play até o visual entrar como filho)
        lveModel = LveModel::createModelFromFile(lveDevice, "models/character.glb");
        auto& player = lveScene.createGameObject();
        player.name = "Player";
        player.model = lveModel;
        player.editorOnly = true;  //só a física: gizmo no editor, invisível no Play
        player.transform.translation = {3.f, -2.f, 0.f};
        player.transform.scale = {1.f, 1.f, 1.f};
        player.rigidBody.emplace();
        player.rigidBody->halfExtents = {0.5f, 0.87f, 0.49f};  //meia largura, altura e profundidade da cápsula
        player.rigidBody->center = {0.f, -0.87f, 0.f};  //caixa acima dos pés (o Y aponta pra baixo)
        player.character.emplace();

        //visual do personagem: filho da cápsula (anda junto com ela)
        lveModel = LveModel::createModelFromFile(lveDevice, "models/basicFemaleTest.glb");
        auto& playerVisual = lveScene.createGameObject();
        playerVisual.name = "PlayerMesh";
        playerVisual.model = lveModel;
        playerVisual.parentId = player.getId();
        playerVisual.transform.rotation.y = glm::radians(180.f);
        playerVisual.faceMovement.emplace();
        playerVisual.animator.emplace();  //toca a primeira animação do arquivo (o idle)

        //pivô da câmera (um Empty): filho do personagem, na altura da cabeça; o mouse vai girar ele
        lveModel = LveModel::createModelFromFile(lveDevice, "models/empty.glb");
        auto& cameraPivot = lveScene.createGameObject();
        cameraPivot.name = "CameraPivot";
        cameraPivot.model = lveModel;
        cameraPivot.cameraRig.emplace();
        cameraPivot.editorOnly = true;  //gizmo só no editor (provisório: a engine vai gerar a cruz de eixos)
        cameraPivot.parentId = player.getId();
        cameraPivot.transform.translation = {0.f, -1.6f, 0.f};  //1,6 acima dos pés (o Y aponta pra baixo)

        //câmera de jogo
        lveModel = LveModel::createModelFromFile(lveDevice, "models/camera.glb");
        auto& gameCamera = lveScene.createGameObject();
        gameCamera.name = "Camera";
        gameCamera.model = lveModel;
        gameCamera.editorOnly = true;  //gizmo só no editor
        gameCamera.parentId = cameraPivot.getId();
        gameCamera.transform.translation = {0.f, 0.f, -4.f};
        gameCamera.camera.emplace();
        lveScene.setActiveCamera(gameCamera.getId());

        //chão (estático)
        lveModel = LveModel::createModelFromFile(lveDevice, "models/plane.glb");
        auto& floor = lveScene.createGameObject();
        floor.name = "Floor";
        floor.model = lveModel;
        floor.transform.translation = {0.f, 1.5f, 0.f};
        floor.transform.scale = glm::vec3{10.f, 1.f, 10.f};
        floor.rigidBody.emplace();
        floor.rigidBody->type = BodyType::Static;
        floor.rigidBody->halfExtents = {1.f, 0.f, 1.f};
    }

    void FirstApp::startPlay() {
        //a cena do editor fica intacta
        playScene = lveScene;
        lveWindow.setCursorMode(PLAY_CURSOR);
        lveInput.skipNextMouseDelta();
    }

    void FirstApp::stopPlay() {
        playScene.reset();
        lveWindow.setCursorMode(CursorMode::Visible);
        lveInput.skipNextMouseDelta();
    }

    void FirstApp::updatePlayMode() {
        //não dispara enquanto digita no ImGui
        bool typing = ImGui::GetIO().WantCaptureKeyboard;
        if (!playScene && !typing && lveInput.wasPressed(GLFW_KEY_P)) {
            startPlay();
        }
        //tecla à esquerda do 1 (GRAVE_ACCENT no GLFW)
        else if (playScene && lveInput.wasPressed(GLFW_KEY_GRAVE_ACCENT)) {
            stopPlay();
        }
    }

    LveScene& FirstApp::activeScene() {
        return playScene.has_value() ? *playScene : lveScene;
    }

    void FirstApp::drawStatsWindow(float fps) {
        ImGui::SetNextWindowPos(ImVec2(10,10));
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
        LveGameObject* selected = editor.getSelected(lveScene);
        if (selected) {
            ImGui::Text("Selecionado: %s", selected->name.c_str());
        } else {
            ImGui::Text("Selecionado: nenhum");
        }

        ImGui::Text("Mouse delta: %.1f, %.1f", lveInput.getMouseDelta().x, lveInput.getMouseDelta().y);

        ImGui::End();
    }

    VkRect2D FirstApp::computeSceneArea() const {
        VkExtent2D extent = lveRenderer.getSwapChainExtent();
        if (playScene) return VkRect2D{{0, 0}, extent};
        return editor.sceneArea(extent);
    }

    LveGameObject* FirstApp::findGameCamera() {
        if (!playScene) return nullptr;
        auto id = playScene->getActiveCamera();
        if (!id) return nullptr;
        return playScene->findGameObjectById(*id);
    }

    CharacterInput FirstApp::readCharacterInput(LveGameObject* gameCamera) {
        CharacterInput input{};
        //digitando no ImGui: as teclas são texto
        if (ImGui::GetIO().WantCaptureKeyboard) return input;

        //frente e direita da câmera, achatadas no chão
        glm::vec3 forward{0.f, 0.f, 1.f};
        glm::vec3 right{1.f, 0.f, 0.f};
        if (gameCamera) {
            glm::mat3 rotation{playScene->worldMatrix(*gameCamera)};
            glm::vec3 flatForward{rotation[2].x, 0.f, rotation[2].z};
            glm::vec3 flatRight{rotation[0].x, 0.f, rotation[0].z};
            //câmera olhando reto pra baixo: a frente achatada some, mantém a do mundo
            if (glm::length(flatForward) > 0.001f) forward = glm::normalize(flatForward);
            if (glm::length(flatRight) > 0.001f) right = glm::normalize(flatRight);
        }

        if (lveInput.isDown(GLFW_KEY_W)) input.moveDirection += forward;
        if (lveInput.isDown(GLFW_KEY_S)) input.moveDirection -= forward;
        if (lveInput.isDown(GLFW_KEY_D)) input.moveDirection += right;
        if (lveInput.isDown(GLFW_KEY_A)) input.moveDirection -= right;
        input.jump = lveInput.wasPressed(GLFW_KEY_SPACE);
        return input;
    }

    void FirstApp::updateCamera(LveGameObject* gameCamera, float aspect) {
        if (gameCamera) {
            //Posição e frente no mundo
            glm::mat4 world = playScene->worldMatrix(*gameCamera);
            glm::vec3 forward = glm::normalize(glm::vec3{world[2]});
            camera.setViewDirection(glm::vec3{world[3]}, forward);
            const CameraComponent& lens = *gameCamera->camera;
            camera.setPerspectiveProjection(lens.fovY, aspect, lens.nearPlane, lens.farPlane);
            return;
        }
        //editor: orbital
        if (!playScene) {
            editor.updateCamera(lveWindow, camera);
        }
        camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 100.f);
    }

    glm::mat4 FirstApp::computeLightSpaceMatrix(LveGameObject* gameCamera, glm::vec3 lightDirection, uint32_t shadowMapSize) {
        //centro: alvo da orbital, ou à frente da câmera de jogo
        glm::vec3 shadowCenter = editor.getFocusPoint();
        if (gameCamera) {
            glm::mat4 world = playScene->worldMatrix(*gameCamera);
            glm::vec3 forward = glm::normalize(glm::vec3{world[2]});
            shadowCenter = glm::vec3{world[3]} + forward * SHADOW_HALF_SIZE;
        }
        updateShadowCamera(shadowCamera, shadowCenter, lightDirection, shadowMapSize);
        return shadowCamera.getProjection() * shadowCamera.getView();
    }

    void FirstApp::renderFrame(FrameInfo& frameInfo) {
        //sombra primeiro: o passe principal lê o shadow map
        shadowRenderSystem->renderShadowMap(frameInfo);
        lveRenderer.beginSwapChainRendering(frameInfo.commandBuffer);
        //a cena fica no retangulo
        lveRenderer.setSceneArea(frameInfo.commandBuffer, sceneArea);
        simpleRenderSystem->renderGameObjects(frameInfo);
        pointLightSystem->render(frameInfo);

        //gizmos: transparentes, depois do que é opaco
        if (!frameInfo.playing) {
            gizmoRenderSystem->render(frameInfo);
        }

        lveImgui->render(frameInfo.commandBuffer);
        lveRenderer.endSwapChainRendering(frameInfo.commandBuffer);
    }

    void FirstApp::updateGlobalUbo(FrameInfo& frameInfo, LveGameObject* gameCamera) {
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
            LveDescriptorWriter(*globalSetLayout, *globalPool)
                .writeBuffer(2, &lightBufferInfo)
                .overwrite(globalDescriptorSets[frameInfo.frameIndex]);
        }

        //ossos dos objetos animados
        if (skinningSystem->update(frameInfo.lveScene, frameInfo.frameIndex)) {
            //buffer de ossos cresceu: atualiza o descriptor
            auto boneBufferInfo = skinningSystem->boneBufferInfo(frameInfo.frameIndex);
            LveDescriptorWriter(*globalSetLayout, *globalPool)
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
        lveInput.update(lveWindow);
        //P / ' alternam editor e Play
        updatePlayMode();
        //clique seleciona (só no editor)
        if (!playScene) {
            editor.updateSelection(lveScene, lveInput, camera, sceneArea);
            editor.updateGizmoMode(lveInput);
        }

        //câmera de jogo
        LveGameObject* gameCamera = findGameCamera();

        //personagens e física (só no Play)
        if (playScene) {
            cameraRigSystem.update(*playScene, lveInput.getMouseDelta());
            characterController.update(*playScene, readCharacterInput(gameCamera));
            physicsSystem.update(*playScene, frameTime);
            characterController.faceMovement(*playScene, frameTime);
            cameraRigSystem.placeCamera(*playScene);
        }

        if (auto commandBuffer = lveRenderer.beginFrame()) {
            int frameIndex = lveRenderer.getFrameIndex();

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
            lveImgui->newFrame();
            editor.beginFrame();
            drawStatsWindow(displayFps);

            //interface do editor (só fora do Play)
            if (!playScene) {
                editor.drawUi(lveScene, camera, sceneArea);
            }

            //render
            renderFrame(frameInfo);
            lveRenderer.endFrame();
        }

        drawingFrame = false;
    }
}