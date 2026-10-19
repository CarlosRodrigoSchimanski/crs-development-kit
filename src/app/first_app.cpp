#include "app/first_app.hpp"
#include "engine/rendering/lve_camera.hpp"
#include "engine/rendering/systems/simple_render_system.hpp"
#include "engine/rendering/systems/point_light_system.hpp"
#include "engine/rendering/systems/shadow_render_system.hpp"
#include "engine/physics/lve_physics_system.hpp"
#include "editor/orbit_camera_controller.hpp"
#include "engine/rendering/lve_buffer.hpp"
#include "editor/lve_imgui.hpp"
#include "engine/rendering/lve_shadow_map.hpp"
#include "engine/platform/lve_platform.hpp"

#include <imgui.h>

#include <glm/glm.hpp>

#include <chrono>
#include <thread>

namespace lve {
    constexpr float MAX_FRAME_TIME = 0.05f;
    //metade do lado da caixa que o sol enxerga (sombra existe num quadrado de 2x isso em volta do alvo)
    constexpr float SHADOW_HALF_SIZE = 10.f;
    //quanto a câmera do sol fica "recuada" do centro; objetos mais altos que isso acima do centro não projetam sombra
    constexpr float SHADOW_SUN_DISTANCE = 30.f;

    //posiciona a câmera do sol olhando pra center, recuada SHADOW_SUN_DISTANCE na direção da luz
    //(função só deste arquivo: static = não aparece pra quem inclui o first_app.hpp)
    static void updateShadowCamera(LveCamera &shadowCamera, glm::vec3 center, glm::vec3 lightDirection, uint32_t shadowMapSize) {
        shadowCamera.setViewDirection(center - lightDirection * SHADOW_SUN_DISTANCE, lightDirection);
        //"encaixa" o centro na grade de texels do shadow map: sem isso, ao mover a câmera
        //a caixa anda frações de texel e a borda da sombra fica tremendo (shimmering)
        float texelWorldSize = (2.f * SHADOW_HALF_SIZE) / static_cast<float>(shadowMapSize);
        glm::vec3 centerInLightView = glm::vec3{shadowCamera.getView() * glm::vec4{center, 1.f}};
        glm::vec2 snapped = glm::floor(glm::vec2{centerInLightView} / texelWorldSize) * texelWorldSize;
        glm::vec3 snapOffset = glm::mat3{shadowCamera.getInverseView()}
            * glm::vec3{snapped - glm::vec2{centerInLightView}, 0.f};
        center += snapOffset;
        shadowCamera.setViewDirection(center - lightDirection * SHADOW_SUN_DISTANCE, lightDirection);
    }

    //desregistra o refresh callback da janela ao sair do run(), inclusive por exceção: a função
    //registrada captura variáveis locais do run() por referência, que deixam de existir ao sair
    //(se a janela ainda chamasse ela, seria acesso a memória já destruída)
    struct RefreshCallbackGuard {
        LveWindow &window;
        ~RefreshCallbackGuard() { window.setRefreshCallback(nullptr); }
    };

    //espera a GPU terminar todo o trabalho pendente quando é destruído. No run() ele é declarado
    //DEPOIS de todos os recursos de GPU: variáveis locais são destruídas na ordem inversa da
    //declaração, então ele é o PRIMEIRO a ser destruído e a espera acontece antes de qualquer
    //recurso que a GPU ainda possa estar usando sumir (no fim normal e também por exceção)
    struct DeviceIdleGuard {
        LveDevice &device;
        //sem vkCheck de propósito: exceção saindo de destrutor encerra o programa na hora
        ~DeviceIdleGuard() { vkDeviceWaitIdle(device.device()); }
    };

    FirstApp::FirstApp() {
        globalPool = LveDescriptorPool::Builder{lveDevice}
            .setMaxSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LveSwapChain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LveSwapChain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, LveSwapChain::MAX_FRAMES_IN_FLIGHT)
            .build();
        
        globalSetLayout = LveDescriptorSetLayout::Builder{lveDevice}
            .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
            .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
            .build();

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

        VkFormat colorFormat = lveRenderer.getSwapChainImageFormat();
        VkFormat depthFormat = lveRenderer.getSwapChainDepthFormat();
        VkDescriptorSetLayout globalLayout = globalSetLayout->getDescriptorSetLayout();
        simpleRenderSystem = std::make_unique<SimpleRenderSystem>(lveDevice, colorFormat, depthFormat, globalLayout);
        pointLightSystem = std::make_unique<PointLightSystem>(lveDevice, colorFormat, depthFormat, globalLayout);
        shadowRenderSystem = std::make_unique<ShadowRenderSystem>(lveDevice, shadowMap, globalLayout);

        lveImgui = std::make_unique<LveImgui>(lveWindow, lveDevice, colorFormat, depthFormat, lveRenderer.getImageCount());

        loadGameObjects();
    }

    void FirstApp::run() {
        
        std::vector<VkDescriptorSet> globalDescriptorSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
        for (size_t i = 0; i < globalDescriptorSets.size(); i++)
        {
            auto bufferInfo = uboBuffers[i]->descriptorInfo();
            auto lightBufferInfo = pointLightSystem->lightBufferInfo(static_cast<int>(i));
            VkDescriptorImageInfo shadowMapInfo{};
            shadowMapInfo.sampler = shadowMap.getSampler();
            shadowMapInfo.imageView = shadowMap.getImageView();
            //tem que bater com o layout da barreira de saída do ShadowRenderSystem::renderShadowMap
            shadowMapInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

            LveDescriptorWriter(*globalSetLayout, *globalPool)
                .writeBuffer(0, &bufferInfo)
                .writeImage(1, &shadowMapInfo)
                .writeBuffer(2, &lightBufferInfo)
                .build(globalDescriptorSets[i]);
        }

        //o tamanho da caixa do sol é fixo: a projeção só precisa ser montada uma vez
        shadowCamera.setOrthographicProjection(-SHADOW_HALF_SIZE, SHADOW_HALF_SIZE, -SHADOW_HALF_SIZE, SHADOW_HALF_SIZE, 0.1f, 2.f * SHADOW_SUN_DISTANCE);

        auto currentTime = std::chrono::high_resolution_clock::now();
        float displayFps = 0.f;

        //um frame inteiro (tempo, câmera, update, desenho). É uma função (lambda) e não só o corpo
        //do while porque, onde o SO trava o loop durante o redimensionamento (Windows), ela também
        //é chamada de dentro do refresh callback da janela
        bool drawingFrame = false;
        auto drawFrame = [&]() {
            //trava contra reentrada: recriar a swap chain pode chamar glfwWaitEvents (janela
            //minimizada), que pode disparar o refresh callback NO MEIO deste frame
            if (drawingFrame) return;
            drawingFrame = true;

            auto newTime = std::chrono::high_resolution_clock::now();
            float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            currentTime = newTime;

            frameTime = glm::min(frameTime, MAX_FRAME_TIME);

            //suaviza o valor exibido (média móvel), senão o número fica tremendo a cada frame
            if (frameTime > 0.f) {
                float instantFps = 1.f / frameTime;
                displayFps = displayFps <= 0.f ? instantFps : glm::mix(displayFps, instantFps, 0.1f);
            }

            //uma vez por frame procura por inputs{inputs devem ser lidos antes de qualquer chamada do frame};
            lveInput.update(lveWindow);
            //alternam entre editor e Play
            updatePlayMode();

            if (playScene) {
                physicsSystem.update(*playScene, frameTime);
            }

            //câmera do jogo: o objeto câmera ativo da cena do Play
            LveGameObject* gameCamera = findGameCamera();

            if (auto commandBuffer = lveRenderer.beginFrame()) {
                int frameIndex = lveRenderer.getFrameIndex();
                updateCamera(gameCamera, lveRenderer.getAspectRatio());

                FrameInfo frameInfo{
                    frameIndex, 
                    frameTime, 
                    commandBuffer, 
                    camera, 
                    globalDescriptorSets[frameIndex], 
                    activeScene(),
                    playScene.has_value()
                };

                //dados globais do frame pros shaders (UBO): câmera, sol e luzes
                GlobalUbo ubo{};
                ubo.projection = camera.getProjection();
                ubo.view = camera.getView();
                ubo.inverseView = camera.getInverseView();
                glm::vec3 lightDirection = -glm::normalize(glm::vec3{ubo.sunDirection});

                //configuração de sombra 
                ubo.lightSpaceMatrix = computeLightSpaceMatrix(gameCamera, lightDirection, shadowMap.getExtent().width);

                //point lights: junta as luzes da cena no buffer de luzes e conta quantas são (ubo.numLights)
                if (pointLightSystem->update(frameInfo, ubo)) {
                    //o buffer de luzes deste frame foi recriado maior: o descriptor aponta pro novo
                    auto lightBufferInfo = pointLightSystem->lightBufferInfo(frameIndex);
                    LveDescriptorWriter(*globalSetLayout, *globalPool)
                        .writeBuffer(2, &lightBufferInfo)
                        .overwrite(globalDescriptorSets[frameIndex]);
                }

                //ubo pronto: envia pra GPU (o buffer deste frame em voo)
                uboBuffers[frameIndex]->writeToBuffer(&ubo);
                uboBuffers[frameIndex]->flush();

                //UI: contador de FPS e o modo atual (editor/play)
                lveImgui->newFrame();
                drawStatsWindow(displayFps);

                //render
                //passe de sombra primeiro: o passe principal vai ler o shadow map que ele produz
                shadowRenderSystem->renderShadowMap(frameInfo);
                lveRenderer.beginSwapChainRendering(commandBuffer);
                simpleRenderSystem->renderGameObjects(frameInfo);
                pointLightSystem->render(frameInfo);
                lveImgui->render(commandBuffer);
                lveRenderer.endSwapChainRendering(commandBuffer);
                lveRenderer.endFrame();
            }

            drawingFrame = false;
        };

        RefreshCallbackGuard refreshGuard{lveWindow};
        //último recurso declarado do run(): ver o comentário do DeviceIdleGuard
        DeviceIdleGuard deviceIdleGuard{lveDevice};
        if (platform::needsRedrawDuringResize()) {
            lveWindow.setRefreshCallback(drawFrame);
        }

        while (!lveWindow.shouldClose()) {
            //no Windows, durante o arrasto da borda, o drawFrame roda daqui de dentro (refresh callback)
            glfwPollEvents();
            drawFrame();

            //limitador de FPS: espera até completar a duração mínima de um frame no alvo
            //da câmera atual; <= 0 significa "sem limite"
            if (cameraController.targetFps > 0.f) {
                float targetFrameDuration = 1.f / cameraController.targetFps;
                //sleep_for sozinho não acorda na hora certa (o SO só garante dormir PELO
                //MENOS o tempo pedido, nunca exatamente). Em vez de pedir um sleep único
                //pro tempo que falta, dormimos só 1ms de cada vez e checamos o relógio de
                //novo a cada volta — isso reduz a chance de "perder a hora" por um estouro
                //grande. Quando sobrar pouco tempo (<2ms), paramos de dormir e deixamos o
                //próprio laço girar (espera ativa) até bater o alvo com precisão.
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
        //cubo de teste: modelo com textura, luz e sombra
        std::shared_ptr<LveModel> lveModel = LveModel::createModelFromFile(lveDevice, "models/iron_cube.glb");
        auto& cube = lveScene.createGameObject();
        cube.model = lveModel;
        cube.transform.translation = {0.f, 0.f, 0.f};
        cube.transform.scale = glm::vec3{1.f, 1.f, 1.f};
        cube.rigidBody.emplace();

        //chão embaixo do cubo (Y aponta pra baixo, então +1 = base do cubo), pra receber a sombra
        lveModel = LveModel::createModelFromFile(lveDevice, "models/plane.glb");
        auto& floor = lveScene.createGameObject();
        floor.model = lveModel;
        floor.transform.translation = {0.f, 1.5f, 0.f};
        floor.transform.scale = glm::vec3{5.f, 1.f, 5.f};
        floor.rigidBody.emplace();
        floor.rigidBody->type = BodyType::Static;
        floor.rigidBody->halfExtents = {1.f, 0.f, 1.f};

        //câmera de jogo: objeto da cena com o componente de câmera (a que renderiza no Play)
        lveModel = LveModel::createModelFromFile(lveDevice, "models/camera.glb");
        auto& gameCamera = lveScene.createGameObject();
        gameCamera.model = lveModel;
        gameCamera.editorOnly = true;  //o gizmo só aparece no editor (e nunca projeta sombra)
        gameCamera.transform.translation = {0.f, -1.f, -5.f};
        gameCamera.camera.emplace();
        lveScene.setActiveCamera(gameCamera.getId());
    }

    void FirstApp::startPlay() {
        //copia a cena do editor pro modo de jogo: a cena do editor fica guardada intacta
        playScene = lveScene;
    }

    void FirstApp::stopPlay() {
        playScene.reset();
    }

    void FirstApp::updatePlayMode() {
        //digitando num campo do ImGui: o P é texto, não comando
        bool typing = ImGui::GetIO().WantCaptureKeyboard;
        if (!playScene && !typing && lveInput.wasPressed(GLFW_KEY_P)) {
            startPlay();
        }
        //o GLFW nomeia pela posição no teclado americano: a tecla à esquerda do 1
        //(' no ABNT2) é a GRAVE_ACCENT
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
        ImGui::End();
    }

    LveGameObject* FirstApp::findGameCamera() {
        if (!playScene) return nullptr;
        auto id = playScene->getActiveCamera();
        if (!id) return nullptr;
        return playScene->findGameObjectById(*id);
    }

    void FirstApp::updateCamera(LveGameObject* gameCamera, float aspect) {
        if (gameCamera) {
            //vista da camera
            glm::vec3 forward = gameCamera->transform.rotationMatrix()[2];
            camera.setViewDirection(gameCamera->transform.translation, forward);
            //adiciona a lente a camera
            const CameraComponent& lens = *gameCamera->camera;
            camera.setPerspectiveProjection(lens.fovY, aspect, lens.nearPlane, lens.farPlane);
            return;
        }
        //ativar e desativar modo camera orbital
        if (!playScene) {
            cameraController.update(lveWindow, camera);
        }
        camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 100.f);
    }

    glm::mat4 FirstApp::computeLightSpaceMatrix(LveGameObject* gameCamera, glm::vec3 lightDirection, uint32_t shadowMapSize) {
        //projeta sombra apenas para aquilo que a camera ve
        glm::vec3 shadowCenter = cameraController.target;
        if (gameCamera) {
            glm::vec3 forward = gameCamera->transform.rotationMatrix()[2];
            shadowCenter = gameCamera->transform.translation + forward * SHADOW_HALF_SIZE;
        }
        updateShadowCamera(shadowCamera, shadowCenter, lightDirection, shadowMapSize);
        return shadowCamera.getProjection() * shadowCamera.getView();
    }
}