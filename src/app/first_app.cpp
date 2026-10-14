#include "app/first_app.hpp"
#include "engine/rendering/lve_camera.hpp"
#include "engine/rendering/systems/simple_render_system.hpp"
#include "engine/rendering/systems/point_light_system.hpp"
#include "engine/rendering/systems/shadow_render_system.hpp"
#include "editor/orbit_camera_controller.hpp"
#include "engine/rendering/lve_buffer.hpp"
#include "editor/lve_imgui.hpp"
#include "engine/rendering/lve_shadow_map.hpp"

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

    FirstApp::FirstApp() {
        globalPool = LveDescriptorPool::Builder{lveDevice}
            .setMaxSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LveSwapChain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LveSwapChain::MAX_FRAMES_IN_FLIGHT)
            .build();
        loadGameObjects();
    }

    void FirstApp::run() {

        std::vector<std::unique_ptr<LveBuffer>> uboBuffers(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
        for (size_t i = 0; i < uboBuffers.size(); i++)
        {
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

        LveShadowMap shadowMap{lveDevice};


        auto globalSetLayout = LveDescriptorSetLayout::Builder{lveDevice}
            .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
            .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();
        
        std::vector<VkDescriptorSet> globalDescriptorSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
        for (size_t i = 0; i < globalDescriptorSets.size(); i++)
        {
            auto bufferInfo = uboBuffers[i]->descriptorInfo();
            VkDescriptorImageInfo shadowMapInfo{};
            shadowMapInfo.sampler = shadowMap.getSampler();
            shadowMapInfo.imageView = shadowMap.getImageView();
            //tem que bater com o finalLayout do render pass do shadow map
            shadowMapInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

            LveDescriptorWriter(*globalSetLayout, *globalPool)
                .writeBuffer(0, &bufferInfo)
                .writeImage(1, &shadowMapInfo)
                .build(globalDescriptorSets[i]);
        }
        
        SimpleRenderSystem simpleRenderSystem{lveDevice, lveRenderer.getSwapChainRenderPass(), globalSetLayout->getDescriptorSetLayout()};
        PointLightSystem pointLightSystem{lveDevice, lveRenderer.getSwapChainRenderPass(), globalSetLayout->getDescriptorSetLayout()};
        ShadowRenderSystem shadowRenderSystem{lveDevice, shadowMap, globalSetLayout->getDescriptorSetLayout()};
        LveImgui lveImgui{lveWindow, lveDevice, lveRenderer.getSwapChainRenderPass(), lveRenderer.getImageCount()};
        LveCamera camera{};
        LveCamera shadowCamera{};
        //o tamanho da caixa do sol é fixo: a projeção só precisa ser montada uma vez
        shadowCamera.setOrthographicProjection(
            -SHADOW_HALF_SIZE, SHADOW_HALF_SIZE, -SHADOW_HALF_SIZE, SHADOW_HALF_SIZE, 0.1f, 2.f * SHADOW_SUN_DISTANCE);
        OrbitCameraController cameraController{};

        auto currentTime = std::chrono::high_resolution_clock::now();
        float displayFps = 0.f;

        while (!lveWindow.shouldClose()) {
            glfwPollEvents();

            auto newTime = std::chrono::high_resolution_clock::now();
            float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            currentTime = newTime;

            frameTime = glm::min(frameTime, MAX_FRAME_TIME);

            //suaviza o valor exibido (média móvel), senão o número fica tremendo a cada frame
            if (frameTime > 0.f) {
                float instantFps = 1.f / frameTime;
                displayFps = displayFps <= 0.f ? instantFps : glm::mix(displayFps, instantFps, 0.1f);
            }

            cameraController.update(lveWindow, camera);

            float aspect = lveRenderer.getAspectRatio();

            camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 100.f);

            if (auto commandBuffer = lveRenderer.beginFrame()) {
                int frameIndex = lveRenderer.getFrameIndex();

                FrameInfo frameInfo{
                    frameIndex, 
                    frameTime, 
                    commandBuffer, 
                    camera, 
                    globalDescriptorSets[frameIndex], 
                    gameObjects
                };

                // update
                GlobalUbo ubo{};
                ubo.projection = camera.getProjection();
                ubo.view = camera.getView();
                ubo.inverseView = camera.getInverseView();
                glm::vec3 lightDirection = -glm::normalize(glm::vec3{ubo.sunDirection});
                //a caixa do sol acompanha o ponto que a câmera orbital está olhando
                updateShadowCamera(shadowCamera, cameraController.target, lightDirection, shadowMap.getExtent().width);
                ubo.lightSpaceMatrix = shadowCamera.getProjection() * shadowCamera.getView();
                pointLightSystem.update(frameInfo, ubo);
                uboBuffers[frameIndex]->writeToBuffer(&ubo);
                uboBuffers[frameIndex]->flush();

                //UI (contador de FPS, por enquanto)
                lveImgui.newFrame();
                ImGui::SetNextWindowPos(ImVec2(10, 10));
                ImGui::SetNextWindowBgAlpha(0.35f);
                ImGui::Begin(
                    "Stats",
                    nullptr,
                    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove);
                ImGui::Text("FPS: %.0f", displayFps);
                ImGui::End();

                //render
                //passe de sombra primeiro: o passe principal vai ler o shadow map que ele produz
                shadowRenderSystem.renderShadowMap(frameInfo);
                lveRenderer.beginSwapChainRenderPass(commandBuffer);
                simpleRenderSystem.renderGameObjects(frameInfo);
                pointLightSystem.render(frameInfo);
                lveImgui.render(commandBuffer);
                lveRenderer.endSwapChainRenderPass(commandBuffer);
                lveRenderer.endFrame();
            }

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
        vkDeviceWaitIdle(lveDevice.device());
    }

    void FirstApp::loadGameObjects() {
        std::shared_ptr<LveModel> lveModel = LveModel::createModelFromFile(lveDevice, "models/iron_cube.glb");
        auto cube = LveGameObject::createGameObject();
        cube.model = lveModel;
        cube.transform.translation = {0.f, 0.f, 0.f};
        cube.transform.scale = glm::vec3{1.f, 1.f, 1.f};
        gameObjects.emplace(cube.getId(), std::move(cube));

        //chão embaixo do cubo (Y aponta pra baixo, então +1 = base do cubo), pra receber a sombra
        lveModel = LveModel::createModelFromFile(lveDevice, "models/plane.obj");
        auto floor = LveGameObject::createGameObject();
        floor.model = lveModel;
        floor.transform.translation = {0.f, 1.5f, 0.f};
        floor.transform.scale = glm::vec3{5.f, 1.f, 5.f};
        gameObjects.emplace(floor.getId(), std::move(floor));
    }
}