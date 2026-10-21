#include "engine/rendering/systems/gizmo_system.hpp"

#include <glm/glm.hpp>

#include <cassert>

namespace lve {
    struct GizmoPushConstantData {
        glm::mat4 modelMatrix{1.f};
        glm::vec4 color{1.f};
    };
    
    //tint do preenchimento: branco = cor do modelo; a = opacidade
    const glm::vec4 GIZMO_FILL_TINT{1.f, 1.f, 1.f, 0.25f};

    GizmoRenderSystem::GizmoRenderSystem(
        LveDevice& device, 
        VkFormat colorFormat, 
        VkFormat depthFormat, 
        VkDescriptorSetLayout globalSetLayout) : lveDevice{device} {
            //set 0 = global (camera)
            pipelineLayout = LvePipeline::createPipelineLayout(
                lveDevice,
                {globalSetLayout},
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                sizeof(GizmoPushConstantData)
            );

            createPipeline(colorFormat, depthFormat);
    }

    void GizmoRenderSystem::createPipeline(VkFormat colorFormat, VkFormat depthFormat) {
        assert(pipelineLayout.get() != VK_NULL_HANDLE && "Cannot create pipeline before pipeline layout");

        PipelineConfigInfo pipelineConfig{};
        LvePipeline::defaultPipelineConfigInfo(pipelineConfig);
        LvePipeline::enableAlphaBlending(pipelineConfig);

        //transparente: testa a profundidade, mas não escreve
        pipelineConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;
        pipelineConfig.colorAttachmentFormats = {colorFormat};
        pipelineConfig.depthAttachmentFormat = depthFormat;
        pipelineConfig.pipelineLayout = pipelineLayout.get();
        fillPipeline = std::make_unique<LvePipeline>(
            lveDevice,
            "shaders/gizmo.vert.spv",
            "shaders/gizmo.frag.spv",
            pipelineConfig);
    }

    void GizmoRenderSystem::render(FrameInfo& frameInfo) {
        fillPipeline->bind(frameInfo.commandBuffer);

        vkCmdBindDescriptorSets(
            frameInfo.commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout.get(),
            0,
            1,
            &frameInfo.globalDescriptorSet,
            0,
            nullptr);

        for (auto& kv : frameInfo.lveScene.getGameObjects()) {
            auto& obj = kv.second;
            //só gizmos com modelo
            if (!obj.editorOnly || obj.model == nullptr) continue;

            GizmoPushConstantData push{};
            push.modelMatrix = obj.transform.mat4();
            push.color = GIZMO_FILL_TINT;

            vkCmdPushConstants(
                frameInfo.commandBuffer,
                pipelineLayout.get(),
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(GizmoPushConstantData),
                &push);

            obj.model->bind(frameInfo.commandBuffer);
            obj.model->draw(frameInfo.commandBuffer);
        }
    }
}