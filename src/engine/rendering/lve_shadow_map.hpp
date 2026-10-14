#pragma once

#include "engine/core/lve_device.hpp"

namespace lve {
    //textura de profundidade onde o sol "desenha" a cena; depois o passe principal lê ela pra saber o que está na sombra
    class LveShadowMap {
        public:
            LveShadowMap(LveDevice &device, uint32_t size = 2048);
            //sem destrutor escrito: os handles RAII abaixo se destroem sozinhos (sampler, view, imagem)

            LveShadowMap(const LveShadowMap&) = delete;
            LveShadowMap& operator=(const LveShadowMap&) = delete;

            //a imagem em si: as barreiras de layout (antes/depois do passe de sombra) precisam dela
            VkImage getImage() const;
            VkImageView getImageView() const;
            VkSampler getSampler() const;
            //o pipeline do passe de sombra precisa saber o formato (dynamic rendering)
            VkFormat getFormat() const;
            VkExtent2D getExtent() const;

        private:
            void createDepthImage();
            void createImageView();
            void createSampler();

            LveDevice &lveDevice;
            uint32_t size;
            VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

            LveAllocatedImage depthImage;
            LveImageView depthImageView;
            LveSampler depthSampler;
    };
}
