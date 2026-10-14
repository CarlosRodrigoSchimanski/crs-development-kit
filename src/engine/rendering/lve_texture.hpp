#pragma once

#include "engine/core/lve_device.hpp"

#include <string>
#include <vector>

namespace lve {
    class LveTexture {
        public:
            LveTexture(LveDevice &device, const std::string &filepath);
            //imagem codificada (png/jpg) já na memória, ex: embutida num .glb
            LveTexture(LveDevice &device, const std::vector<unsigned char> &encodedImage);
            //pixels RGBA crus, ex: textura branca 1x1 de reserva
            LveTexture(LveDevice &device, uint32_t width, uint32_t height, const unsigned char *rgbaPixels);
            ~LveTexture();

            LveTexture(const LveTexture&) = delete;
            LveTexture& operator=(const LveTexture&) = delete;

            VkImageView getImageView() const;
            VkSampler getSampler() const;

        private:
            //RGBA 8 bits por canal, em sRGB (o shader recebe a cor já convertida pra linear)
            static constexpr VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;

            void upload(uint32_t width, uint32_t height, const unsigned char *pixels);
            void createTextureImage(uint32_t width, uint32_t height, const unsigned char *pixels);
            void copyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer buffer, uint32_t width, uint32_t height);
            void createTextureImageView();
            void createTextureSampler();

            LveDevice &lveDevice;
            VkImage textureImage;
            VkDeviceMemory textureImageMemory;
            VkImageView textureImageView;
            VkSampler textureSampler;
    };
}
