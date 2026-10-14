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
            //sem destrutor escrito: os handles RAII se destroem sozinhos (ver a ordem dos membros)

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

            //frameIndex = qual frame em voo é este (0..MAX_FRAMES_IN_FLIGHT-1). Quem conta é o
            //LveRenderer: a swap chain só RECEBE, assim a fence e o semáforo do frame são escolhidos
            //pelo mesmo número que escolhe o command buffer, o UBO e o descriptor set (antes a swap
            //chain tinha o próprio contador, que voltava a 0 a cada recriação e ficava defasado)

            //espera a GPU liberar os recursos deste frame e pega o índice da próxima imagem pra desenhar
            VkResult acquireNextImage(int frameIndex, uint32_t *imageIndex);
            //envia os comandos gravados pra GPU e apresenta o resultado na tela
            VkResult submitCommandBuffers(VkCommandBuffer commandBuffer, uint32_t imageIndex, int frameIndex);

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

            //referência ao device: mesma lógica de sempre, o device precisa sobreviver ao swap chain
            LveDevice &device;
            //tamanho da janela, usado como referência caso a GPU não dite o tamanho da imagem
            VkExtent2D windowExtent;

            //A ORDEM DOS MEMBROS IMPORTA: eles são destruídos na ordem INVERSA da declaração. O swap
            //chain vem antes das views das imagens dele (as views morrem primeiro), e cada imagem de
            //profundidade vem antes da view dela
            LveSwapchainHandle swapChain;
            //as imagens em si pertencem à swap chain (vêm do vkGetSwapchainImagesKHR): não são nossas pra destruir
            std::vector<VkImage> swapChainImages;
            std::vector<LveImageView> swapChainImageViews;
            std::vector<LveAllocatedImage> depthImages;
            std::vector<LveImageView> depthImageViews;

            //sincronização: imageAvailable é por frame em voo (usado ANTES de saber qual imagem vem);
            //renderFinished é por IMAGEM da swap chain, porque a apresentação da imagem segura o
            //semáforo até aquela mesma imagem ser adquirida de novo
            std::vector<LveSemaphore> imageAvailableSemaphores;
            std::vector<LveSemaphore> renderFinishedSemaphores;
            std::vector<LveFence> inFlightFences;
            //NÃO é dono: só aponta pra uma das inFlightFences (qual frame está usando cada imagem)
            std::vector<VkFence> imagesInFlight;
    };
}
