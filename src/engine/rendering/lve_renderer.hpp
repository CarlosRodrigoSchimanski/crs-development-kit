#pragma once

#include "engine/core/lve_window.hpp"
#include "engine/core/lve_device.hpp"
#include "engine/rendering/lve_swap_chain.hpp"

#include <memory>
#include <vector>

namespace lve {
    //organiza o ciclo de cada frame: pega a imagem da swap chain, entrega o command buffer
    //pra quem vai desenhar, abre/fecha o render pass da tela e envia tudo pra GPU.
    //também recria a swap chain quando a janela muda de tamanho
    class LveRenderer {
        public:
            LveRenderer(LveWindow &window,LveDevice &device);
            ~LveRenderer();

            //não copiável: é dono da swap chain e dos command buffers
            LveRenderer(const LveRenderer&) = delete;
            LveRenderer& operator=(const LveRenderer&) = delete;

            VkRenderPass getSwapChainRenderPass() const;
            float getAspectRatio() const;
            uint32_t getImageCount() const;

            VkCommandBuffer getCurrentCommandBuffer() const;

            int getFrameIndex() const;

            VkCommandBuffer beginFrame();
            void endFrame();
            void beginSwapChainRenderPass(VkCommandBuffer commandBuffer);
            void endSwapChainRenderPass(VkCommandBuffer commandBuffer);

        private:
            //um command buffer por frame em voo
            void createCommandBuffers();
            //espera a GPU parar e cria uma swap chain nova no tamanho atual da janela
            void recreateSwapChain();

            LveWindow& lveWindow;
            LveDevice& lveDevice;
            std::unique_ptr<LveSwapChain> lveSwapChain;
            std::vector<VkCommandBuffer> commandBuffers;
            //índice da imagem da swap chain sendo desenhada neste frame (vem do acquireNextImage)
            uint32_t currentImageIndex;
            int currentFrameIndex{0};
            bool isFrameStarted{false};

    };
}