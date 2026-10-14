#include "engine/rendering/systems/shadow_render_system.hpp"

#include <glm/glm.hpp>

#include <cassert>

namespace lve {
    //o passe de sombra só precisa posicionar o vértice, então só a matriz do modelo
    struct ShadowPushConstantData {
        glm::mat4 modelMatrix{1.f};
    };

    //troca o layout do shadow map, esperando (src) o que veio antes terminar antes de (dst) o que vem depois
    static void transitionShadowMap(
        VkCommandBuffer commandBuffer, VkImage image,
        VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess,
        VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess,
        VkImageLayout oldLayout, VkImageLayout newLayout) {
        VkImageMemoryBarrier2 barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.srcStageMask = srcStage;
        barrier.srcAccessMask = srcAccess;
        barrier.dstStageMask = dstStage;
        barrier.dstAccessMask = dstAccess;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};

        VkDependencyInfo dependencyInfo{};
        dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.imageMemoryBarrierCount = 1;
        dependencyInfo.pImageMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
    }

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
        pipelineConfig.pipelineLayout = pipelineLayout;
        lvePipeline = std::make_unique<LvePipeline>(
            lveDevice,
            "shaders/shadow.vert.spv",
            "shaders/shadow.frag.spv",
            pipelineConfig);
    }

    void ShadowRenderSystem::renderShadowMap(FrameInfo& frameInfo) {
        VkExtent2D extent = shadowMap.getExtent();

        //ENTRADA: o frame anterior pode ainda estar LENDO o shadow map no fragment shader do passe
        //principal; espera isso terminar antes de escrever profundidade nova. Basta esperar a
        //execução (acesso de origem NONE): leitura não deixa nada pendente pra "publicar".
        //layout antigo UNDEFINED = pode descartar o conteúdo, a imagem vai ser limpa mesmo
        transitionShadowMap(frameInfo.commandBuffer, shadowMap.getImage(),
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

        vkCmdEndRendering(frameInfo.commandBuffer);

        //SAÍDA: terminar de escrever a profundidade antes do fragment shader do passe principal
        //ler; e deixar a imagem no layout de leitura (tem que bater com o descriptor do first_app)
        transitionShadowMap(frameInfo.commandBuffer, shadowMap.getImage(),
            VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    }
}
