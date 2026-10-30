#include "engine/rendering/systems/gizmo_system.hpp"

#include <glm/glm.hpp>

#include <cassert>

namespace crsdk {
    struct GizmoPushConstantData {
        glm::mat4 modelMatrix{1.f};
        glm::vec4 color{1.f};
    };
    
    //tint do preenchimento: branco = cor do modelo; a = opacidade
    const glm::vec4 GIZMO_FILL_TINT{1.f, 1.f, 1.f, 0.25f};

    //tint das arestas: cor do modelo mais clara e opaca
    const glm::vec4 GIZMO_EDGE_TINT{1.5f, 1.5f, 1.5f, 1.f};

    GizmoRenderSystem::GizmoRenderSystem(
        CrsDevice& device, 
        VkFormat colorFormat, 
        VkFormat depthFormat, 
        VkDescriptorSetLayout globalSetLayout) : crsDevice{device} {
            //set 0 = global (camera)
            pipelineLayout = CrsPipeline::createPipelineLayout(
                crsDevice,
                {globalSetLayout},
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                sizeof(GizmoPushConstantData)
            );

            createPipeline(colorFormat, depthFormat);
    }

    void GizmoRenderSystem::createPipeline(VkFormat colorFormat, VkFormat depthFormat) {
        assert(pipelineLayout.get() != VK_NULL_HANDLE && "Cannot create pipeline before pipeline layout");

        PipelineConfigInfo pipelineConfig{};
        CrsPipeline::defaultPipelineConfigInfo(pipelineConfig);
        //o gizmo.vert só lê posição (location 0) e cor (location 1)
        std::vector<VkVertexInputAttributeDescription> positionAndColor{};
        for (auto& attribute : pipelineConfig.attributeDescriptions) {
            if (attribute.location <= 1) positionAndColor.push_back(attribute);
        }
        pipelineConfig.attributeDescriptions = positionAndColor;

        CrsPipeline::enableAlphaBlending(pipelineConfig);

        //transparente: testa a profundidade, mas não escreve
        pipelineConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;
        pipelineConfig.colorAttachmentFormats = {colorFormat};
        pipelineConfig.depthAttachmentFormat = depthFormat;
        pipelineConfig.pipelineLayout = pipelineLayout.get();
        fillPipeline = std::make_unique<CrsPipeline>(
            crsDevice,
            "shaders/gizmo.vert.spv",
            "shaders/gizmo.frag.spv",
            pipelineConfig);

        //arestas: mesma configuração, mas cada par de índices vira uma linha
        pipelineConfig.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        edgePipeline = std::make_unique<CrsPipeline>(
            crsDevice,
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

        for (auto& kv : frameInfo.crsScene.getGameObjects()) {
            auto& obj = kv.second;
            //só gizmos com modelo
            if (!obj.editorOnly || obj.model == nullptr) continue;

            GizmoPushConstantData push{};
            push.modelMatrix = frameInfo.crsScene.worldMatrix(obj);
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

        //arestas por cima do preenchimento
        edgePipeline->bind(frameInfo.commandBuffer);
        for (auto& kv : frameInfo.crsScene.getGameObjects()) {
            auto& obj = kv.second;
            if (!obj.editorOnly || obj.model == nullptr) continue;

            GizmoPushConstantData push{};
            push.modelMatrix = frameInfo.crsScene.worldMatrix(obj);
            push.color = GIZMO_EDGE_TINT;

            vkCmdPushConstants(
                frameInfo.commandBuffer,
                pipelineLayout.get(),
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(GizmoPushConstantData),
                &push);

            obj.model->drawEdges(frameInfo.commandBuffer);
        }
    }
}