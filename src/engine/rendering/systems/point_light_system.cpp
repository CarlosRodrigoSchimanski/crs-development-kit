#include "engine/rendering/systems/point_light_system.hpp"

#include <glm/glm.hpp>

#include <cassert>
#include <map>

namespace crsdk {

    struct PointLightPushConstants {
        glm::vec4 position{};
        glm::vec4 color{};
        float radius{};
    };

    //quantas luzes cabem no storage buffer de cada frame no começo (cresce sozinho se precisar)
    constexpr size_t INITIAL_LIGHT_CAPACITY = 64;

    PointLightSystem::PointLightSystem(CrsDevice& device, VkFormat colorFormat, VkFormat depthFormat, VkDescriptorSetLayout globalSetLayout) : crsDevice{device} {
        pipelineLayout = CrsPipeline::createPipelineLayout(
            crsDevice,
            {globalSetLayout},
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            sizeof(PointLightPushConstants));
        createPipeline(colorFormat, depthFormat);

        lightBuffers.resize(CrsSwapChain::MAX_FRAMES_IN_FLIGHT);
        for (auto &lightBuffer : lightBuffers) {
            lightBuffer = createLightBuffer(INITIAL_LIGHT_CAPACITY);
        }
    }

    std::unique_ptr<CrsBuffer> PointLightSystem::createLightBuffer(size_t capacity) {
        auto lightBuffer = std::make_unique<CrsBuffer>(
            crsDevice,
            sizeof(PointLight),
            static_cast<uint32_t>(capacity),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        lightBuffer->map();
        return lightBuffer;
    }

    VkDescriptorBufferInfo PointLightSystem::lightBufferInfo(int frameIndex) const {
        return lightBuffers[frameIndex]->descriptorInfo();
    }

    void PointLightSystem::createPipeline(VkFormat colorFormat, VkFormat depthFormat) {
        assert(pipelineLayout.get() != VK_NULL_HANDLE && "Cannot create pipeline before pipeline layout");

        PipelineConfigInfo pipelineConfig{};
        CrsPipeline::defaultPipelineConfigInfo(pipelineConfig);
        CrsPipeline::enableAlphaBlending(pipelineConfig);
        pipelineConfig.bindingDescriptions.clear();
        pipelineConfig.attributeDescriptions.clear();
        //desenha na imagem da tela (cor + profundidade)
        pipelineConfig.colorAttachmentFormats = {colorFormat};
        pipelineConfig.depthAttachmentFormat = depthFormat;
        pipelineConfig.pipelineLayout = pipelineLayout.get();
        crsPipeline = std::make_unique<CrsPipeline>(
            crsDevice,
            "shaders/point_light.vert.spv",
            "shaders/point_light.frag.spv",
            pipelineConfig);
    }

    //copia as point lights da cena pro storage buffer do frame (só lê a cena; mover as luzes é com
    //quem usa a engine)
    bool PointLightSystem::update(FrameInfo& frameInfo, GlobalUbo& ubo) {
        std::vector<PointLight> lights;
        for (auto& kv : frameInfo.crsScene.getGameObjects()) {
            auto& obj = kv.second;
            if (!obj.pointLight) continue;
            lights.push_back({glm::vec4(obj.transform.translation, 1.f), glm::vec4(obj.color, obj.pointLight->lightIntensity)});
        }

        //não cabe: troca o buffer deste frame por um maior. Seguro porque o beginFrame já esperou a
        //fence deste frame (a GPU terminou de ler o buffer antigo) e nenhum sistema fez o bind do
        //descriptor neste frame ainda
        auto &lightBuffer = lightBuffers[frameInfo.frameIndex];
        size_t capacity = lightBuffer->getBufferSize() / sizeof(PointLight);
        bool recreated = false;
        if (lights.size() > capacity) {
            while (capacity < lights.size()) capacity *= 2;
            lightBuffer = createLightBuffer(capacity);
            recreated = true;
        }

        if (!lights.empty()) {
            lightBuffer->writeToBuffer(lights.data(), lights.size() * sizeof(PointLight));
        }
        ubo.numLights = static_cast<int>(lights.size());
        return recreated;
    }

    void PointLightSystem::render(FrameInfo& frameInfo) {
        //de trás pra frente por causa da transparência; multimap porque duas luzes podem estar
        //à mesma distância (com map, a segunda sobrescreveria a primeira e uma sumiria)
        std::multimap<float, CrsGameObject::id_t> sorted;
        for (auto& kv : frameInfo.crsScene.getGameObjects()) {
            auto& obj = kv.second;
            if (!obj.pointLight) continue;
        
            auto offset = obj.transform.translation - frameInfo.camera.getPosition();
            float dis = glm::dot(offset, offset);
            sorted.emplace(dis, obj.getId());
        }

        crsPipeline->bind(frameInfo.commandBuffer);

        vkCmdBindDescriptorSets(
            frameInfo.commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout.get(),
            0,
            1,
            &frameInfo.globalDescriptorSet,
            0,
            nullptr);
        
        for (auto it = sorted.rbegin(); it != sorted.rend(); ++it) {
            auto& obj = frameInfo.crsScene.getGameObjects().at(it->second);

            PointLightPushConstants push{};
            push.position = glm::vec4(obj.transform.translation, 1.f);
            push.color = glm::vec4(obj.color, obj.pointLight->lightIntensity);
            push.radius = obj.transform.scale.x;

            vkCmdPushConstants(
                frameInfo.commandBuffer,
                pipelineLayout.get(),
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(PointLightPushConstants),
                &push
            );

            vkCmdDraw(frameInfo.commandBuffer, 6, 1, 0, 0);
        }

    }

}
