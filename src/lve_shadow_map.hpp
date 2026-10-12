#pragma once

#include "lve_device.hpp"

namespace lve {
    //textura de profundidade onde o sol "desenha" a cena; depois o passe principal lê ela pra saber o que está na sombra
    class LveShadowMap {
        public:
            LveShadowMap(LveDevice &device, uint32_t size = 2048);
            ~LveShadowMap();

            LveShadowMap(const LveShadowMap&) = delete;
            LveShadowMap& operator=(const LveShadowMap&) = delete;

            VkRenderPass getRenderPass() const;
            VkFramebuffer getFramebuffer() const;
            VkImageView getImageView() const;
            VkSampler getSampler() const;
            VkExtent2D getExtent() const;

        private:
            void createDepthImage();
            void createImageView();
            void createSampler();
            void createRenderPass();
            void createFramebuffer();

            LveDevice &lveDevice;
            uint32_t size;
            VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

            VkImage depthImage;
            VkDeviceMemory depthImageMemory;
            VkImageView depthImageView;
            VkSampler depthSampler;
            VkRenderPass renderPass;
            VkFramebuffer framebuffer;
    };
}
