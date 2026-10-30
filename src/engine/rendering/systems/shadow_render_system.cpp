#include "engine/rendering/systems/shadow_render_system.hpp"
#include "engine/rendering/crs_image_barrier.hpp"

#include <glm/glm.hpp>

#include <cassert>

namespace crsdk {
    //o passe de sombra só precisa posicionar o vértice, então só a matriz do modelo
    struct ShadowPushConstantData {
        glm::mat4 modelMatrix{1.f};
    };

    //tem que bater com o bloco Push do shadow_skinned.vert
    struct SkinnedShadowPushConstantData {
        glm::mat4 modelMatrix{1.f};
        uint32_t boneOffset = 0;
    };

    ShadowRenderSystem::ShadowRenderSystem(CrsDevice& device, CrsShadowMap& shadowMap, VkDescriptorSetLayout globalSetLayout)
        : crsDevice{device}, shadowMap{shadowMap} {
        //mesmo set global dos outros sistemas: é dele que sai a lightSpaceMatrix
        pipelineLayout = CrsPipeline::createPipelineLayout(
            crsDevice,
            {globalSetLayout},
            VK_SHADER_STAGE_VERTEX_BIT,
            sizeof(ShadowPushConstantData));
        skinnedPipelineLayout = CrsPipeline::createPipelineLayout(
            crsDevice,
            {globalSetLayout},
            VK_SHADER_STAGE_VERTEX_BIT,
            sizeof(SkinnedShadowPushConstantData));
        createPipeline();
    }

    void ShadowRenderSystem::createPipeline() {
        assert(pipelineLayout.get() != VK_NULL_HANDLE && "Cannot create pipeline before pipeline layout");

        PipelineConfigInfo pipelineConfig{};
        CrsPipeline::defaultPipelineConfigInfo(pipelineConfig);
        //o passe de sombra não tem imagem de cor, então o pipeline também não pode ter
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
        //dynamic rendering: sem render pass, o pipeline só precisa saber o formato da profundidade
        pipelineConfig.depthAttachmentFormat = shadowMap.getFormat();
        pipelineConfig.pipelineLayout = pipelineLayout.get();
        crsPipeline = std::make_unique<CrsPipeline>(
            crsDevice,
            "shaders/shadow.vert.spv",
            "shaders/shadow.frag.spv",
            pipelineConfig);

        //objetos animados: mesma configuração, mas lê também os pesos dos ossos (binding 1)
        pipelineConfig.bindingDescriptions = CrsModel::getSkinnedBindingDescriptions();
        std::vector<VkVertexInputAttributeDescription> positionAndBones{};
        for (auto& attribute : CrsModel::getSkinnedAttributeDescriptions()) {
            //o shadow_skinned.vert lê a posição (0), os ossos (4) e os pesos (5)
            if (attribute.location == 0 || attribute.location == 4 || attribute.location == 5) {
                positionAndBones.push_back(attribute);
            }
        }
        pipelineConfig.attributeDescriptions = positionAndBones;
        pipelineConfig.pipelineLayout = skinnedPipelineLayout.get();
        skinnedPipeline = std::make_unique<CrsPipeline>(
            crsDevice,
            "shaders/shadow_skinned.vert.spv",
            "shaders/shadow.frag.spv",
            pipelineConfig);
    }

    void ShadowRenderSystem::renderShadowMap(FrameInfo& frameInfo) {
        VkExtent2D extent = shadowMap.getExtent();

        //ENTRADA: o frame anterior pode ainda estar LENDO o shadow map no fragment shader do passe
        //principal; espera isso terminar antes de escrever profundidade nova. Basta esperar a
        //execução (acesso de origem NONE): leitura não deixa nada pendente pra "publicar".
        //layout antigo UNDEFINED = pode descartar o conteúdo, a imagem vai ser limpa mesmo
        imageBarrier(frameInfo.commandBuffer, shadowMap.getImage(), VK_IMAGE_ASPECT_DEPTH_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_NONE,
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

        //o que o render pass fazia escondido: limpar no começo e guardar no fim
        VkRenderingAttachmentInfo depthAttachment{};
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = shadowMap.getImageView();
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        //diferente do depth da tela: aqui a profundidade É o resultado do passe, tem que ser guardada
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.clearValue.depthStencil = {1.0f, 0};  //1.0 = "nada na frente", o mais longe possível

        //só profundidade, nenhuma imagem de cor
        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea = {{0, 0}, extent};
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 0;
        renderingInfo.pDepthAttachment = &depthAttachment;

        vkCmdBeginRendering(frameInfo.commandBuffer, &renderingInfo);

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

        for (auto& kv : frameInfo.crsScene.getGameObjects()) {
            auto& obj = kv.second;
            if (obj.model == nullptr) continue;
            if (obj.editorOnly) continue;  //não projeta sombra
            if (obj.isAnimated()) continue;  //desenhado abaixo, seguindo a pose
            ShadowPushConstantData push{};
            push.modelMatrix = frameInfo.crsScene.worldMatrix(obj);

            vkCmdPushConstants(
                frameInfo.commandBuffer,
                pipelineLayout.get(),
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(ShadowPushConstantData),
                &push
            );

            obj.model->bind(frameInfo.commandBuffer);
            obj.model->draw(frameInfo.commandBuffer);
        }

        //objetos animados: outra pipeline (layout diferente, então o set global é ligado de novo)
        skinnedPipeline->bind(frameInfo.commandBuffer);
        vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            skinnedPipelineLayout.get(), 0, 1, &frameInfo.globalDescriptorSet, 0, nullptr);
        for (auto& kv : frameInfo.crsScene.getGameObjects()) {
            auto& obj = kv.second;
            if (!obj.isAnimated() || obj.editorOnly) continue;

            SkinnedShadowPushConstantData push{};
            push.modelMatrix = frameInfo.crsScene.worldMatrix(obj);
            push.boneOffset = obj.animator->boneOffset;
            vkCmdPushConstants(frameInfo.commandBuffer, skinnedPipelineLayout.get(), VK_SHADER_STAGE_VERTEX_BIT,
                0, sizeof(SkinnedShadowPushConstantData), &push);

            obj.model->bindSkinned(frameInfo.commandBuffer);
            obj.model->draw(frameInfo.commandBuffer);
        }

        vkCmdEndRendering(frameInfo.commandBuffer);

        //SAÍDA: terminar de escrever a profundidade antes do fragment shader do passe principal
        //ler; e deixar a imagem no layout de leitura (tem que bater com o descriptor do first_app)
        imageBarrier(frameInfo.commandBuffer, shadowMap.getImage(), VK_IMAGE_ASPECT_DEPTH_BIT,
            VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    }
}
