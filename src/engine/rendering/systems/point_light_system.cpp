#include "engine/rendering/systems/point_light_system.hpp"

#include <glm/glm.hpp>

#include <cassert>
#include <map>

namespace lve {

    struct PointLightPushConstants {
        glm::vec4 position{};
        glm::vec4 color{};
        float radius{};
    };

    PointLightSystem::PointLightSystem(LveDevice& device, VkFormat colorFormat, VkFormat depthFormat, VkDescriptorSetLayout globalSetLayout) : lveDevice{device} {
        pipelineLayout = LvePipeline::createPipelineLayout(
            lveDevice,
            {globalSetLayout},
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            sizeof(PointLightPushConstants));
        createPipeline(colorFormat, depthFormat);
    }

    PointLightSystem::~PointLightSystem() {
        vkDeviceWaitIdle(lveDevice.device());
        vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
    }

    void PointLightSystem::createPipeline(VkFormat colorFormat, VkFormat depthFormat) {
        assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

        PipelineConfigInfo pipelineConfig{};
        LvePipeline::defaultPipelineConfigInfo(pipelineConfig);
        LvePipeline::enableAlphaBlending(pipelineConfig);
        pipelineConfig.bindingDescriptions.clear();
        pipelineConfig.attributeDescriptions.clear();
        //desenha na imagem da tela (cor + profundidade)
        pipelineConfig.colorAttachmentFormats = {colorFormat};
        pipelineConfig.depthAttachmentFormat = depthFormat;
        pipelineConfig.pipelineLayout = pipelineLayout;
        lvePipeline = std::make_unique<LvePipeline>(
            lveDevice,
            "shaders/point_light.vert.spv",
            "shaders/point_light.frag.spv",
            pipelineConfig);
    }

    //copia as point lights da cena pro storage buffer do frame (só lê a cena; mover as luzes é com
    //quem usa a engine)
    void PointLightSystem::update(FrameInfo& frameInfo, GlobalUbo& ubo, LveBuffer& lightBuffer) {
        const size_t capacity = lightBuffer.getBufferSize() / sizeof(PointLight);

        std::vector<PointLight> lights;
        for (auto& kv : frameInfo.gameObjects) {
            auto& obj = kv.second;
            if (obj.pointLight == nullptr) continue;

            //o assert só avisa no Debug; no Release some, então a trava de verdade é o continue:
            //escrever além do buffer atropelaria a memória vizinha
            if (lights.size() >= capacity) {
                assert(false && "Point lights exceed light buffer capacity");
                continue;
            }

            lights.push_back({glm::vec4(obj.transform.translation, 1.f), glm::vec4(obj.color, obj.pointLight->lightIntensity)});
        }

        if (!lights.empty()) {
            lightBuffer.writeToBuffer(lights.data(), lights.size() * sizeof(PointLight));
        }
        ubo.numLights = static_cast<int>(lights.size());
    }

    void PointLightSystem::render(FrameInfo& frameInfo) {
        //de trás pra frente por causa da transparência; multimap porque duas luzes podem estar
        //à mesma distância (com map, a segunda sobrescreveria a primeira e uma sumiria)
        std::multimap<float, LveGameObject::id_t> sorted;
        for (auto& kv : frameInfo.gameObjects) {
            auto& obj = kv.second;
            if (obj.pointLight == nullptr) continue;
        
            auto offset = obj.transform.translation - frameInfo.camera.getPosition();
            float dis = glm::dot(offset, offset);
            sorted.emplace(dis, obj.getId());
        }

        lvePipeline->bind(frameInfo.commandBuffer);

        vkCmdBindDescriptorSets(
            frameInfo.commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout,
            0,
            1,
            &frameInfo.globalDescriptorSet,
            0,
            nullptr);
        
        for (auto it = sorted.rbegin(); it != sorted.rend(); ++it) {
            auto& obj = frameInfo.gameObjects.at(it->second);

            PointLightPushConstants push{};
            push.position = glm::vec4(obj.transform.translation, 1.f);
            push.color = glm::vec4(obj.color, obj.pointLight->lightIntensity);
            push.radius = obj.transform.scale.x;

            vkCmdPushConstants(
                frameInfo.commandBuffer,
                pipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(PointLightPushConstants),
                &push
            );

            vkCmdDraw(frameInfo.commandBuffer, 6, 1, 0, 0);
        }

    }

}
