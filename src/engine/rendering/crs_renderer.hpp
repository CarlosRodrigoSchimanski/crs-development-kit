#pragma once

#include "engine/core/crs_window.hpp"
#include "engine/core/crs_device.hpp"
#include "engine/rendering/crs_swap_chain.hpp"

#include <memory>
#include <vector>

namespace crsdk {
    //organiza o ciclo de cada frame: pega a imagem da swap chain, entrega o command buffer
    //pra quem vai desenhar, abre/fecha o desenho na tela e envia tudo pra GPU.
    //também recria a swap chain quando a janela muda de tamanho
    class CrsRenderer {
        public:
            CrsRenderer(CrsWindow &window,CrsDevice &device);
            ~CrsRenderer();

            //não copiável: é dono da swap chain e dos command buffers
            CrsRenderer(const CrsRenderer&) = delete;
            CrsRenderer& operator=(const CrsRenderer&) = delete;

            //formatos das imagens da tela: os pipelines que desenham nela precisam saber (dynamic rendering)
            VkFormat getSwapChainImageFormat() const;
            VkFormat getSwapChainDepthFormat() const;
            float getAspectRatio() const;
            VkExtent2D getSwapChainExtent() const;
            //limita o desenho da cena
            void setSceneArea(VkCommandBuffer commandBuffer, VkRect2D area) const;
            uint32_t getImageCount() const;

            VkCommandBuffer getCurrentCommandBuffer() const;

            int getFrameIndex() const;

            VkCommandBuffer beginFrame();
            void endFrame();
            //abre/fecha o desenho na imagem atual da swap chain (barreiras de layout + begin/end rendering)
            void beginSwapChainRendering(VkCommandBuffer commandBuffer);
            void endSwapChainRendering(VkCommandBuffer commandBuffer);

        private:
            //um command buffer por frame em voo
            void createCommandBuffers();
            //espera a GPU parar e cria uma swap chain nova no tamanho atual da janela
            void recreateSwapChain();

            CrsWindow& crsWindow;
            CrsDevice& crsDevice;
            std::unique_ptr<CrsSwapChain> crsSwapChain;
            std::vector<VkCommandBuffer> commandBuffers;
            //índice da imagem da swap chain sendo desenhada neste frame (vem do acquireNextImage)
            uint32_t currentImageIndex;
            //qual frame em voo é este: escolhe command buffer, UBO, descriptor set E (repassado à
            //swap chain) a fence e o semáforo. Único contador de frame da engine
            int currentFrameIndex{0};
            bool isFrameStarted{false};

    };
}