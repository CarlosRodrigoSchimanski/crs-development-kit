#include "engine/rendering/crs_texture.hpp"
#include "engine/rendering/crs_image_barrier.hpp"

#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>

namespace crsdk {

    //pixels decodificados pelo stb: liberados pelo stbi_image_free ao sair do escopo, por qualquer
    //caminho (mesmo padrão do cgltf_data no loadModel)
    using StbPixels = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>;

    static VkFormat formatFor(TextureKind kind) {
        return kind == TextureKind::Color ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
    }

    CrsTexture::CrsTexture(CrsDevice &device, const std::string &filepath, CrsUploadBatch &batch, TextureKind kind)
        : crsDevice{device}, format{formatFor(kind)} {
        int width, height, channels;
        StbPixels pixels{stbi_load(filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha), &stbi_image_free};
        if (!pixels) {
            throw std::runtime_error("failed to load texture image: " + filepath);
        }
        upload(static_cast<uint32_t>(width), static_cast<uint32_t>(height), pixels.get(), batch);
    }

    CrsTexture::CrsTexture(CrsDevice &device, const std::vector<unsigned char> &encodedImage, CrsUploadBatch &batch, TextureKind kind)
        : crsDevice{device}, format{formatFor(kind)} {
        //mesmo decodificador do stbi_load, só que lendo de um buffer em vez de um arquivo
        int width, height, channels;
        StbPixels pixels{stbi_load_from_memory(
            encodedImage.data(), static_cast<int>(encodedImage.size()), &width, &height, &channels, STBI_rgb_alpha),
            &stbi_image_free};
        if (!pixels) {
            throw std::runtime_error("failed to decode texture image from memory");
        }
        upload(static_cast<uint32_t>(width), static_cast<uint32_t>(height), pixels.get(), batch);
    }

    CrsTexture::CrsTexture(CrsDevice &device, uint32_t width, uint32_t height, const unsigned char *rgbaPixels, CrsUploadBatch &batch,
                           TextureKind kind)
        : crsDevice{device}, format{formatFor(kind)} {
        upload(width, height, rgbaPixels, batch);
    }

    VkImageView CrsTexture::getImageView() const { return textureImageView.get(); }
    VkSampler CrsTexture::getSampler() const { return textureSampler.get(); }

    //parte comum dos 3 construtores: com os pixels já em RGBA, cria image, view e sampler
    void CrsTexture::upload(uint32_t width, uint32_t height, const unsigned char *pixels, CrsUploadBatch &batch) {
        createTextureImage(width, height, pixels, batch);
        createTextureImageView();
        createTextureSampler();
    }

    //cria a VkImage e grava no lote a subida dos pixels RGBA pra ela (via staging buffer do lote)
    void CrsTexture::createTextureImage(uint32_t width, uint32_t height, const unsigned char *pixels, CrsUploadBatch &batch) {
        //4 bytes por pixel (RGBA), forçado pelo STBI_rgb_alpha
        VkBuffer stagingBuffer = batch.stage(pixels, static_cast<VkDeviceSize>(width) * height * 4);
        mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = mipLevels;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        //TRANSFER_SRC: cada mip é feito copiando (reduzido) o nível anterior
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        //a partir daqui a imagem tem dono: se algo abaixo (ou a view, ou o sampler) falhar, ela é liberada
        textureImage = crsDevice.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        //só grava no command buffer do lote; a cópia acontece de fato no batch.submit()
        VkCommandBuffer commandBuffer = batch.commandBuffer();
        //antes da cópia: nada pra esperar (imagem nova), só deixar pronta pra receber a cópia
        imageBarrier(commandBuffer, textureImage.get(), VK_IMAGE_ASPECT_COLOR_BIT,
            VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
            VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(commandBuffer, stagingBuffer, width, height);
        //os níveis menores, e no fim todos prontos pro fragment shader ler
        generateMipmaps(commandBuffer, width, height);
    }

    void CrsTexture::generateMipmaps(VkCommandBuffer commandBuffer, uint32_t width, uint32_t height) {
        //(RGBA8 sRGB/UNORM sempre aceitam blit com filtro linear: é obrigatório no Vulkan)
        int32_t mipWidth = static_cast<int32_t>(width);
        int32_t mipHeight = static_cast<int32_t>(height);
        for (uint32_t level = 1; level < mipLevels; level++) {
            //o nível anterior acabou de ser escrito: vira a fonte da cópia
            imageBarrier(commandBuffer, textureImage.get(), VK_IMAGE_ASPECT_COLOR_BIT,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, level - 1, 1);

            const int32_t nextWidth = std::max(mipWidth / 2, 1);
            const int32_t nextHeight = std::max(mipHeight / 2, 1);
            VkImageBlit blit{};
            blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level - 1, 0, 1};
            blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
            blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level, 0, 1};
            blit.dstOffsets[1] = {nextWidth, nextHeight, 1};
            vkCmdBlitImage(commandBuffer, textureImage.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                textureImage.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

            //o anterior já serviu: pronto pro shader
            imageBarrier(commandBuffer, textureImage.get(), VK_IMAGE_ASPECT_COLOR_BIT,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, level - 1, 1);
            mipWidth = nextWidth;
            mipHeight = nextHeight;
        }
        //o último nível só foi escrito (ninguém copiou dele)
        imageBarrier(commandBuffer, textureImage.get(), VK_IMAGE_ASPECT_COLOR_BIT,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels - 1, 1);
    }

    void CrsTexture::copyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer buffer, uint32_t width, uint32_t height) {
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
            commandBuffer, buffer, textureImage.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }

    void CrsTexture::createTextureImageView() {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = textureImage.get();
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView imageView;
        if (vkCreateImageView(crsDevice.device(), &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture image view!");
        }
        textureImageView = CrsImageView{crsDevice.device(), imageView};
    }

    void CrsTexture::createTextureSampler() {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        //anisotrópico: o chão visto de lado continua nítido (sem ele o mip menor deixa tudo borrado)
        samplerInfo.anisotropyEnable = crsDevice.samplerAnisotropy ? VK_TRUE : VK_FALSE;
        samplerInfo.maxAnisotropy = crsDevice.samplerAnisotropy ? std::min(16.f, crsDevice.properties.limits.maxSamplerAnisotropy) : 1.f;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = static_cast<float>(mipLevels);

        VkSampler sampler;
        if (vkCreateSampler(crsDevice.device(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture sampler!");
        }
        textureSampler = CrsSampler{crsDevice.device(), sampler};
    }


}
