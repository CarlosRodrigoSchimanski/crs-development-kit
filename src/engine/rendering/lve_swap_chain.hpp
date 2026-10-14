#pragma once

#include "engine/core/lve_device.hpp"

#include <vector>


namespace lve {

    // Responsável pelo swap chain: a fila de imagens que alternam entre
    // "sendo desenhada pela GPU" e "sendo exibida na tela", junto com a imagem de
    // profundidade de cada uma e a sincronização entre CPU e GPU necessária pra usar tudo isso
    class LveSwapChain {
        public:
            //quantos frames podem estar "em voo" (sendo processados) ao mesmo tempo
            static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

            //"previous" é o swap chain antigo, quando esse novo está substituindo ele
            //(janela redimensionada); nullptr quando é a primeira criação. Só é usado durante
            //a construção, quem chama continua sendo o dono dele
            LveSwapChain(LveDevice &deviceRef, VkExtent2D windowExtent, const LveSwapChain *previous = nullptr);
            ~LveSwapChain();

            LveSwapChain(const LveSwapChain &) = delete;
            LveSwapChain &operator=(const LveSwapChain &) = delete;

            //compara se o novo swap chain manteve os mesmos formatos do antigo
            bool compareSwapFormats(const LveSwapChain &other) const;

            //imagens de cor (as que vão pra tela) e de profundidade, uma de cada por imagem da swap chain
            VkImage getImage(uint32_t index) const;
            VkImageView getImageView(uint32_t index) const;
            VkImage getDepthImage(uint32_t index) const;
            VkImageView getDepthImageView(uint32_t index) const;
            size_t imageCount() const;
            VkFormat getSwapChainImageFormat() const;
            VkFormat getSwapChainDepthFormat() const;
            VkExtent2D getSwapChainExtent() const;

            float extentAspectRatio() const;

            VkFormat findDepthFormat() const;

            //pega o índice da próxima imagem disponível pra desenhar
            VkResult acquireNextImage(uint32_t *imageIndex);
            //envia os comandos gravados pra GPU e apresenta o resultado na tela
            VkResult submitCommandBuffers(VkCommandBuffer commandBuffer, uint32_t imageIndex);

        private:
            void createSwapChain(VkSwapchainKHR oldSwapchain);
            void createImageViews();
            void createDepthResources();
            void createSyncObjects();

            //funções auxiliares pra escolher a melhor configuração disponível na GPU
            VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);
            VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);
            VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

            VkFormat swapChainImageFormat;
            VkFormat swapChainDepthFormat;
            VkExtent2D swapChainExtent;

            std::vector<VkImage> depthImages;
            std::vector<VkDeviceMemory> depthImageMemorys;
            std::vector<VkImageView> depthImageViews;
            std::vector<VkImage> swapChainImages;
            std::vector<VkImageView> swapChainImageViews;

            //referência ao device: mesma lógica de sempre, o device precisa sobreviver ao swap chain
            LveDevice &device;
            //tamanho da janela, usado como referência caso a GPU não dite o tamanho da imagem
            VkExtent2D windowExtent;

            VkSwapchainKHR swapChain;

            //sincronização: imageAvailable é por frame em voo (usado ANTES de saber qual imagem vem);
            //renderFinished é por IMAGEM da swap chain, porque a apresentação da imagem segura o
            //semáforo até aquela mesma imagem ser adquirida de novo
            std::vector<VkSemaphore> imageAvailableSemaphores;
            std::vector<VkSemaphore> renderFinishedSemaphores;
            std::vector<VkFence> inFlightFences;
            std::vector<VkFence> imagesInFlight;
            size_t currentFrame = 0;
    };
}
