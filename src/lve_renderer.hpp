#pragma once

#include "lve_window.hpp"
#include "lve_device.hpp"
#include "lve_swap_chain.hpp"

#include <memory>
#include <vector>
#include <cassert>

namespace lve {
    // Classe FirstApp esta no mesmo nivel que a classe lvewindow
    class LveRenderer {
        public:

            // Construtor e destrutor da classe FirstApp
            LveRenderer(LveWindow &window,LveDevice &device);
            ~LveRenderer();

            //codigo para evitar duplicação de ponteiros
            LveRenderer(const LveRenderer&) = delete;
            LveRenderer& operator=(const LveRenderer&) = delete;

            VkRenderPass getSwapChainRenderPass() const;
            float getAscpectRation() const;
            uint32_t getImageCount() const;

            VkCommandBuffer getCurrentCommandBuffer() const;

            int getFameIndex() const;

            VkCommandBuffer beginFrame();
            void endFrame();
            void beginSwapChainRenderPass(VkCommandBuffer commandBuffer);
            void endSwapChainRenderPass(VkCommandBuffer commandBuffer);

        private:
            //função createCommandBuffers() que será responsável por criar os command buffers
            void createCommandBuffers();
            //libera os command buffers já alocados de volta pro command pool
            void freeCommandBuffers();
            //função 
            void recreateSwapChain();

            // Instancia da classe LveWindow
            LveWindow& lveWindow;
            // Instancia da classe LveDevice: precisa da janela pra criar a surface
            LveDevice& lveDevice;
            // Instancia da classe LveSwapChain, que é responsável pela gestão da swap chain
            std::unique_ptr<LveSwapChain> lveSwapChain;
            // Vetor de command buffers, que são usados para gravar comandos de renderização
            std::vector<VkCommandBuffer> commandBuffers;
            //
            uint32_t currentImageIndex;
            int currentFrameIndex{0};
            bool isFrameStarted{false};

    };
}