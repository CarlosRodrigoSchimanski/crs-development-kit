#pragma once

#include "lve_device.hpp"
#include <vulkan/vulkan.h>

#include <memory>
#include <vector>


namespace lve {

    // Responsável pelo swap chain: a fila de imagens que alternam entre
    // "sendo desenhada pela GPU" e "sendo exibida na tela", junto com o render pass,
    // os framebuffers e a sincronização entre CPU e GPU necessários pra usar tudo isso
    class LveSwapChain {
        public:
            //quantos frames podem estar "em voo" (sendo processados) ao mesmo tempo
            static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

            //"previous" é o swap chain antigo, quando esse novo está substituindo ele
            //(janela redimensionada); nullptr quando é a primeira criação
            LveSwapChain(LveDevice &deviceRef, VkExtent2D windowExtent, std::shared_ptr<LveSwapChain> previous = nullptr);
            ~LveSwapChain();

            LveSwapChain(const LveSwapChain &) = delete;
            LveSwapChain &operator=(const LveSwapChain &) = delete;

            //compara se o novo swap chain manteve os mesmos formatos do antigo
            bool compareSwapFormats(const LveSwapChain &swapChain) const;

            VkFramebuffer getFrameBuffer(int index);
            VkRenderPass getRenderPass();
            size_t imageCount();
            VkFormat getSwapChainImageFormat();
            VkExtent2D getSwapChainExtent();

            float extentAspectRatio();

            VkFormat findDepthFormat();

            //pega o índice da próxima imagem disponível pra desenhar
            VkResult acquireNextImage(uint32_t *imageIndex);
            //envia os comandos gravados pra GPU e apresenta o resultado na tela
            VkResult submitCommandBuffers(const VkCommandBuffer *buffers, uint32_t *imageIndex);

        private:
            void createSwapChain();
            void createImageViews();
            void createDepthResources();
            void createRenderPass();
            void createFramebuffers();
            void createSyncObjects();

            //funções auxiliares pra escolher a melhor configuração disponível na GPU
            VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);
            VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);
            VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

            VkFormat swapChainImageFormat;
            VkFormat swapChainDepthFormat;
            VkExtent2D swapChainExtent;

            std::vector<VkFramebuffer> swapChainFramebuffers;
            VkRenderPass renderPass;

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
            //swap chain que este está substituindo (se houver); usado em oldSwapchain
            //na criação, e descartado depois que a criação termina
            std::shared_ptr<LveSwapChain> oldSwapChain;

            //objetos de sincronização entre CPU e GPU (explicados quando forem usados)
            std::vector<VkSemaphore> imageAvailableSemaphores;
            std::vector<VkSemaphore> renderFinishedSemaphores;
            std::vector<VkFence> inFlightFences;
            std::vector<VkFence> imagesInFlight;
            size_t currentFrame = 0;
    };
}
