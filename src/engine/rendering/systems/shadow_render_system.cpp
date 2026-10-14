#include "engine/rendering/systems/shadow_render_system.hpp"

#include <glm/glm.hpp>

#include <cassert>

namespace lve {
    //o passe de sombra só precisa posicionar o vértice, então só a matriz do modelo
    struct ShadowPushConstantData {
        glm::mat4 modelMatrix{1.f};
    };

    ShadowRenderSystem::ShadowRenderSystem(LveDevice& device, LveShadowMap& shadowMap, VkDescriptorSetLayout globalSetLayout)
        : lveDevice{device}, shadowMap{shadowMap} {
        //mesmo set global dos outros sistemas: é dele que sai a lightSpaceMatrix
        pipelineLayout = LvePipeline::createPipelineLayout(
            lveDevice,
            {globalSetLayout},
            VK_SHADER_STAGE_VERTEX_BIT,
            sizeof(ShadowPushConstantData));
        createPipeline();
    }

    ShadowRenderSystem::~ShadowRenderSystem() {
        vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
    }

    void ShadowRenderSystem::createPipeline() {
        assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

        PipelineConfigInfo pipelineConfig{};
        LvePipeline::defaultPipelineConfigInfo(pipelineConfig);
        //o render pass do shadow map não tem color attachment, então o pipeline também não pode ter
        pipelineConfig.colorBlendInfo.attachmentCount = 0;
        pipelineConfig.colorBlendInfo.pAttachments = nullptr;
        //depth bias: empurra a profundidade gravada um pouco pra longe da luz,
        //evita a superfície "sombrear a si mesma" (shadow acne) por erro de precisão
        pipelineConfig.rasterizationInfo.depthBiasEnable = VK_TRUE;
        pipelineConfig.rasterizationInfo.depthBiasConstantFactor = 1.25f;
        pipelineConfig.rasterizationInfo.depthBiasSlopeFactor = 1.75f;
        //o shadow.vert só lê a posição (location 0); cor, normal e uv ficam de fora
        std::vector<VkVertexInputAttributeDescription> positionOnly{};
        for (auto& attribute : pipelineConfig.attributeDescriptions) {
            if (attribute.location == 0) positionOnly.push_back(attribute);
        }
        pipelineConfig.attributeDescriptions = positionOnly;
        pipelineConfig.renderPass = shadowMap.getRenderPass();
        pipelineConfig.pipelineLayout = pipelineLayout;
        lvePipeline = std::make_unique<LvePipeline>(
            lveDevice,
            "shaders/shadow.vert.spv",
            "shaders/shadow.frag.spv",
            pipelineConfig);
    }

    void ShadowRenderSystem::renderShadowMap(FrameInfo& frameInfo) {
        VkExtent2D extent = shadowMap.getExtent();

        VkClearValue clearValue{};
        clearValue.depthStencil = {1.0f, 0};  //1.0 = "nada na frente", o mais longe possível

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = shadowMap.getRenderPass();
        renderPassInfo.framebuffer = shadowMap.getFramebuffer();
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = extent;
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearValue;

        vkCmdBeginRenderPass(frameInfo.commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        //viewport e scissor são dinâmicos: aqui do tamanho do shadow map, não da janela
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(extent.width);
        viewport.height = static_cast<float>(extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        VkRect2D scissor{{0, 0}, extent};
        vkCmdSetViewport(frameInfo.commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(frameInfo.commandBuffer, 0, 1, &scissor);

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

        for (auto& kv : frameInfo.gameObjects) {
            auto& obj = kv.second;
            if (obj.model == nullptr) continue;
            ShadowPushConstantData push{};
            push.modelMatrix = obj.transform.mat4();

            vkCmdPushConstants(
                frameInfo.commandBuffer,
                pipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(ShadowPushConstantData),
                &push
            );

            obj.model->bind(frameInfo.commandBuffer);
            obj.model->draw(frameInfo.commandBuffer);
        }

        vkCmdEndRenderPass(frameInfo.commandBuffer);
    }
}
