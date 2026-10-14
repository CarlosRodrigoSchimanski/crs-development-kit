#include "engine/rendering/lve_texture.hpp"
#include "engine/rendering/lve_buffer.hpp"
#include "engine/rendering/lve_image_barrier.hpp"

#include <vk_mem_alloc.h>

#include <stb_image.h>

#include <stdexcept>

namespace lve {

    LveTexture::LveTexture(LveDevice &device, const std::string &filepath) : lveDevice{device} {
        int width, height, channels;
        stbi_uc *pixels = stbi_load(filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        if (!pixels) {
            throw std::runtime_error("failed to load texture image: " + filepath);
        }
        upload(static_cast<uint32_t>(width), static_cast<uint32_t>(height), pixels);
        stbi_image_free(pixels);
    }

    LveTexture::LveTexture(LveDevice &device, const std::vector<unsigned char> &encodedImage) : lveDevice{device} {
        //mesmo decodificador do stbi_load, só que lendo de um buffer em vez de um arquivo
        int width, height, channels;
        stbi_uc *pixels = stbi_load_from_memory(
            encodedImage.data(), static_cast<int>(encodedImage.size()), &width, &height, &channels, STBI_rgb_alpha);
        if (!pixels) {
            throw std::runtime_error("failed to decode texture image from memory");
        }
        upload(static_cast<uint32_t>(width), static_cast<uint32_t>(height), pixels);
        stbi_image_free(pixels);
    }

    LveTexture::LveTexture(LveDevice &device, uint32_t width, uint32_t height, const unsigned char *rgbaPixels)
        : lveDevice{device} {
        upload(width, height, rgbaPixels);
    }

    VkImageView LveTexture::getImageView() const { return textureImageView; }
    VkSampler LveTexture::getSampler() const { return textureSampler; }

    //parte comum dos 3 construtores: com os pixels já em RGBA, cria image, view e sampler
    void LveTexture::upload(uint32_t width, uint32_t height, const unsigned char *pixels) {
        createTextureImage(width, height, pixels);
        createTextureImageView();
        createTextureSampler();
    }

    //sobe os pixels RGBA pra uma VkImage na GPU, passando por um staging buffer
    void LveTexture::createTextureImage(uint32_t width, uint32_t height, const unsigned char *pixels) {
        //4 bytes por pixel (RGBA), forçado pelo STBI_rgb_alpha
        //o destrutor do LveBuffer libera o staging ao sair da função, mesmo se der exceção no meio
        LveBuffer stagingBuffer{
            lveDevice,
            4,
            width * height,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        };
        stagingBuffer.map();
        stagingBuffer.writeToBuffer(pixels);

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        lveDevice.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageAllocation);

        //os 3 passos gravados num command buffer só: 1 submit e 1 espera em vez de 3
        VkCommandBuffer commandBuffer = lveDevice.beginSingleTimeCommands();
        //antes da cópia: nada pra esperar (imagem nova), só deixar pronta pra receber a cópia
        imageBarrier(commandBuffer, textureImage, VK_IMAGE_ASPECT_COLOR_BIT,
            VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
            VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(commandBuffer, stagingBuffer.getBuffer(), width, height);
        //depois da cópia: a escrita da cópia termina antes de qualquer fragment shader ler a textura
        imageBarrier(commandBuffer, textureImage, VK_IMAGE_ASPECT_COLOR_BIT,
            VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        lveDevice.endSingleTimeCommands(commandBuffer);
    }
    

    LveTexture::~LveTexture() {
        vkDestroySampler(lveDevice.device(), textureSampler, nullptr);
        vkDestroyImageView(lveDevice.device(), textureImageView, nullptr);
        vmaDestroyImage(lveDevice.allocator(), textureImage, textureImageAllocation);
    }

    void LveTexture::copyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer buffer, uint32_t width, uint32_t height) {
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {width, height, 1};

        vkCmdCopyBufferToImage(
            commandBuffer, buffer, textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }

    void LveTexture::createTextureImageView() {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = textureImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(lveDevice.device(), &viewInfo, nullptr, &textureImageView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture image view!");
        }
    }

    void LveTexture::createTextureSampler() {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        if (vkCreateSampler(lveDevice.device(), &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture sampler!");
        }
    }


}
