#pragma once

#include "lve_device.hpp"

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
            void createTextureImage(const unsigned char *pixels);
            void transitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout);
            void copyBufferToImage(VkBuffer buffer, uint32_t width, uint32_t height);
            void createTextureImageView();
            void createTextureSampler();

            LveDevice &lveDevice;
            VkImage textureImage;
            VkDeviceMemory textureImageMemory;
            VkImageView textureImageView;
            VkSampler textureSampler;
            int width, height, channels;
    };
}
