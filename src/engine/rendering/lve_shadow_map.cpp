#include "engine/rendering/lve_shadow_map.hpp"

#include <stdexcept>

namespace lve {

    LveShadowMap::LveShadowMap(LveDevice &device, uint32_t size) : lveDevice{device}, size{size} {
        createDepthImage();
        createImageView();
        createSampler();
    }

    VkImage LveShadowMap::getImage() const { return depthImage.get(); }
    VkImageView LveShadowMap::getImageView() const { return depthImageView.get(); }
    VkSampler LveShadowMap::getSampler() const { return depthSampler.get(); }
    VkFormat LveShadowMap::getFormat() const { return depthFormat; }
    VkExtent2D LveShadowMap::getExtent() const { return {size, size}; }

    void LveShadowMap::createDepthImage() {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = size;
        imageInfo.extent.height = size;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = depthFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        //desenhada pela GPU no passe de sombra (attachment) e lida no passe principal (sampled)
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        depthImage = lveDevice.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

    void LveShadowMap::createImageView() {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = depthImage.get();
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = depthFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView imageView;
        if (vkCreateImageView(lveDevice.device(), &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shadow map image view!");
        }
        depthImageView = LveImageView{lveDevice.device(), imageView};
    }

    void LveShadowMap::createSampler() {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        //profundidade não se interpola (por enquanto; suavização da borda vem com PCF)
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        //fora da área do sol lê profundidade 1.0 (máxima) = nada na frente = iluminado
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        VkSampler sampler;
        if (vkCreateSampler(lveDevice.device(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shadow map sampler!");
        }
        depthSampler = LveSampler{lveDevice.device(), sampler};
    }
}
