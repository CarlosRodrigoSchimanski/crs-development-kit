#include "engine/rendering/systems/selection_outline_system.hpp"

#include "engine/rendering/crs_image_barrier.hpp"

#include <glm/glm.hpp>

#include <cassert>
#include <stdexcept>

namespace crsdk {
    //uma cor por pixel basta: 0 = fora do objeto, 1 = dentro
    constexpr VkFormat MASK_FORMAT = VK_FORMAT_R8_UNORM;
    //laranja do Blender (#FFA033) já em linear (a tela é sRGB); a = força da aura
    const glm::vec4 OUTLINE_COLOR{1.f, 0.352f, 0.033f, 0.95f};
    //até onde o brilho vai, em pixels (cada pixel em volta olha (2r+1)^2 vizinhos: não exagerar)
    constexpr float OUTLINE_RADIUS = 5.f;

    struct MaskPushConstantData {
        glm::mat4 modelMatrix{1.f};
        uint32_t boneOffset = 0;
    };

    struct OutlinePushConstantData {
        glm::vec4 color;
        float radius;
    };

    SelectionOutlineSystem::SelectionOutlineSystem(CrsDevice &device, VkFormat colorFormat, VkFormat depthFormat, VkDescriptorSetLayout globalSetLayout)
        : crsDevice{device} {
        //passo 1: set global (câmera e ossos), como os outros sistemas
        maskPipelineLayout = CrsPipeline::createPipelineLayout(
            crsDevice, {globalSetLayout}, VK_SHADER_STAGE_VERTEX_BIT, sizeof(MaskPushConstantData));

        //passo 2: só a máscara
        maskSetLayout = CrsDescriptorSetLayout::Builder{crsDevice}
            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();
        maskPool = CrsDescriptorPool::Builder{crsDevice}
            .setMaxSets(1)
            .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
            .build();
        if (!maskPool->allocateDescriptor(maskSetLayout->getDescriptorSetLayout(), maskSet)) {
            throw std::runtime_error("failed to allocate selection mask descriptor set!");
        }
        outlinePipelineLayout = CrsPipeline::createPipelineLayout(
            crsDevice, {maskSetLayout->getDescriptorSetLayout()}, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(OutlinePushConstantData));

        //máscara lida pixel a pixel (texelFetch): sem filtro
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.maxLod = 0.f;
        VkSampler sampler;
        if (vkCreateSampler(crsDevice.device(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
            throw std::runtime_error("failed to create selection mask sampler!");
        }
        maskSampler = CrsSampler{crsDevice.device(), sampler};

        createPipelines(colorFormat, depthFormat);
    }

    void SelectionOutlineSystem::createPipelines(VkFormat colorFormat, VkFormat depthFormat) {
        assert(maskPipelineLayout.get() != VK_NULL_HANDLE && outlinePipelineLayout.get() != VK_NULL_HANDLE);

        PipelineConfigInfo config{};
        CrsPipeline::defaultPipelineConfigInfo(config);
        //a máscara não tem profundidade: o objeto inteiro entra, mesmo o que estiver atrás de outro
        config.depthStencilInfo.depthTestEnable = VK_FALSE;
        config.depthStencilInfo.depthWriteEnable = VK_FALSE;
        config.colorAttachmentFormats = {MASK_FORMAT};
        config.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
        //o selection_mask.vert só lê a posição (location 0)
        std::vector<VkVertexInputAttributeDescription> positionOnly{};
        for (auto &attribute : config.attributeDescriptions) {
            if (attribute.location == 0) positionOnly.push_back(attribute);
        }
        config.attributeDescriptions = positionOnly;
        config.pipelineLayout = maskPipelineLayout.get();
        maskPipeline = std::make_unique<CrsPipeline>(
            crsDevice, "shaders/selection_mask.vert.spv", "shaders/selection_mask.frag.spv", config);

        //animado: lê também os ossos (4) e os pesos (5), como o passe de sombra
        config.bindingDescriptions = CrsModel::getSkinnedBindingDescriptions();
        std::vector<VkVertexInputAttributeDescription> positionAndBones{};
        for (auto &attribute : CrsModel::getSkinnedAttributeDescriptions()) {
            if (attribute.location == 0 || attribute.location == 4 || attribute.location == 5) positionAndBones.push_back(attribute);
        }
        config.attributeDescriptions = positionAndBones;
        skinnedMaskPipeline = std::make_unique<CrsPipeline>(
            crsDevice, "shaders/selection_mask_skinned.vert.spv", "shaders/selection_mask.frag.spv", config);

        //passo 2: triângulo da tela inteira, sem vértices; mistura por transparência por cima da cena
        PipelineConfigInfo outline{};
        CrsPipeline::defaultPipelineConfigInfo(outline);
        outline.bindingDescriptions.clear();
        outline.attributeDescriptions.clear();
        CrsPipeline::enableAlphaBlending(outline);
        //desenhado dentro do passe da tela, que tem profundidade: os formatos têm que ser os dela
        //(dynamic rendering), mas a aura não testa nem escreve profundidade
        outline.depthStencilInfo.depthTestEnable = VK_FALSE;
        outline.depthStencilInfo.depthWriteEnable = VK_FALSE;
        outline.colorAttachmentFormats = {colorFormat};
        outline.depthAttachmentFormat = depthFormat;
        outline.pipelineLayout = outlinePipelineLayout.get();
        outlinePipeline = std::make_unique<CrsPipeline>(
            crsDevice, "shaders/selection_outline.vert.spv", "shaders/selection_outline.frag.spv", outline);
    }

    void SelectionOutlineSystem::createMask(VkExtent2D extent) {
        //a máscara velha pode estar sendo lida por um frame ainda na GPU (só acontece ao redimensionar)
        vkDeviceWaitIdle(crsDevice.device());
        maskImageView = {};
        maskImage = {};

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = {extent.width, extent.height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = MASK_FORMAT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        //desenhada no passo 1 (attachment) e lida no passo 2 (sampled)
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        maskImage = crsDevice.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = maskImage.get();
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = MASK_FORMAT;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VkImageView view;
        if (vkCreateImageView(crsDevice.device(), &viewInfo, nullptr, &view) != VK_SUCCESS) {
            throw std::runtime_error("failed to create selection mask image view!");
        }
        maskImageView = CrsImageView{crsDevice.device(), view};
        maskExtent = extent;

        //o set do passo 2 passa a apontar pra máscara nova
        VkDescriptorImageInfo maskInfo{};
        maskInfo.sampler = maskSampler.get();
        maskInfo.imageView = maskImageView.get();
        maskInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        CrsDescriptorWriter(*maskSetLayout, *maskPool).writeImage(0, &maskInfo).overwrite(maskSet);
    }

    void SelectionOutlineSystem::renderMask(FrameInfo &frameInfo, const CrsGameObject *selected, VkExtent2D screen, VkRect2D sceneArea) {
        hasSelection = selected != nullptr && selected->model != nullptr;
        if (!hasSelection) return;
        if (screen.width != maskExtent.width || screen.height != maskExtent.height) createMask(screen);

        VkCommandBuffer commandBuffer = frameInfo.commandBuffer;
        //ENTRADA: o frame anterior pode ainda estar lendo a máscara no passo 2; o conteúdo velho não importa
        imageBarrier(commandBuffer, maskImage.get(), VK_IMAGE_ASPECT_COLOR_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_NONE,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = maskImageView.get();
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue.color = {{0.f, 0.f, 0.f, 0.f}};

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea = {{0, 0}, maskExtent};
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;
        vkCmdBeginRendering(commandBuffer, &renderingInfo);

        //mesma área da cena que o passe principal: cada pixel da máscara cai no mesmo pixel da tela
        VkViewport viewport{};
        viewport.x = static_cast<float>(sceneArea.offset.x);
        viewport.y = static_cast<float>(sceneArea.offset.y);
        viewport.width = static_cast<float>(sceneArea.extent.width);
        viewport.height = static_cast<float>(sceneArea.extent.height);
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &sceneArea);

        MaskPushConstantData push{};
        push.modelMatrix = frameInfo.crsScene.worldMatrix(*selected);
        if (selected->isAnimated()) {
            skinnedMaskPipeline->bind(commandBuffer);
            push.boneOffset = selected->animator->boneOffset;
        } else {
            maskPipeline->bind(commandBuffer);
        }
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, maskPipelineLayout.get(),
            0, 1, &frameInfo.globalDescriptorSet, 0, nullptr);
        vkCmdPushConstants(commandBuffer, maskPipelineLayout.get(), VK_SHADER_STAGE_VERTEX_BIT,
            0, sizeof(MaskPushConstantData), &push);
        if (selected->isAnimated()) {
            selected->model->bindSkinned(commandBuffer);
        } else {
            selected->model->bind(commandBuffer);
        }
        selected->model->draw(commandBuffer);

        vkCmdEndRendering(commandBuffer);

        //SAÍDA: a máscara pronta antes do fragment shader do passo 2 ler
        imageBarrier(commandBuffer, maskImage.get(), VK_IMAGE_ASPECT_COLOR_BIT,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    void SelectionOutlineSystem::renderOutline(FrameInfo &frameInfo) {
        if (!hasSelection) return;
        VkCommandBuffer commandBuffer = frameInfo.commandBuffer;
        outlinePipeline->bind(commandBuffer);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, outlinePipelineLayout.get(),
            0, 1, &maskSet, 0, nullptr);
        OutlinePushConstantData push{OUTLINE_COLOR, OUTLINE_RADIUS};
        vkCmdPushConstants(commandBuffer, outlinePipelineLayout.get(), VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(OutlinePushConstantData), &push);
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    }
}
