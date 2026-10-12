#pragma once

#include "lve_window.hpp"

#include <optional>
#include <vector>

namespace lve {

    // Detalhes que o swap chain (próximo passo) vai precisar consultar
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    // Agrupa os índices das filas (queues) que a GPU escolhida oferece
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete();
    };

    // Classe responsável por: instance, validation layers, physical device e logical device
    class LveDevice {
        public:
#ifdef NDEBUG
            const bool enableValidationLayers = false;
#else
            const bool enableValidationLayers = true;
#endif

            //agora recebe a janela, pois precisa dela pra criar a surface
            LveDevice(LveWindow &window);
            ~LveDevice();

            // Regra dos 3: uma LveDevice possui recursos únicos do Vulkan, cópia não é permitida
            LveDevice(const LveDevice &) = delete;
            LveDevice &operator=(const LveDevice &) = delete;

            LveDevice(LveDevice &&) = delete;
            LveDevice &operator=(LveDevice &&)= delete;

            //propriedades/limites da GPU escolhida (ex: alinhamento mínimo pra uniform buffer)
            VkPhysicalDeviceProperties properties;

            VkInstance instance();
            VkPhysicalDevice physicalDevice();
            VkDevice device();
            VkSurfaceKHR surface();
            VkQueue graphicsQueue();
            VkQueue presentQueue();
            VkCommandPool getCommandPool();

            SwapChainSupportDetails getSwapChainSupport();
            QueueFamilyIndices findPhysicalQueueFamilies();

            //acha, entre os formatos candidatos, o primeiro que a GPU realmente suporta pro uso pedido
            VkFormat findSupportedFormat(
                const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
            //acha o tipo de memória da GPU compatível com o que está sendo pedido
            uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
            //cria uma VkImage já com a memória alocada e vinculada
            void createImageWithInfo(
                const VkImageCreateInfo &imageInfo,
                VkMemoryPropertyFlags properties,
                VkImage &image,
                VkDeviceMemory &imageMemory);
            //cria um VkBuffer já com a memória alocada e vinculada
            void createBuffer(
                VkDeviceSize size,
                VkBufferUsageFlags usage,
                VkMemoryPropertyFlags properties,
                VkBuffer &buffer,
                VkDeviceMemory &bufferMemory);
            //copia o conteúdo de um buffer pra outro, executando isso como um comando na GPU
            void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

        private:
            //cria o VkInstance
            void createInstance();
            //configura o callback das validation layers
            void setupDebugMessenger();
            //cria a VkSurfaceKHR a partir da janela recebida
            void createSurface();
            //escolhe a GPU (VkPhysicalDevice) a ser usada
            void pickPhysicalDevice();
            //cria o VkDevice (logical device) e pega as filas gráfica e de apresentação
            void createLogicalDevice();
            //cria o command pool, usado mais pra frente pra alocar command buffers
            void createCommandPool();

            //funções auxiliares
            bool checkValidationLayerSupport();
            std::vector<const char *> getRequiredExtensions();
            void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo);
            void hasGflwRequiredInstanceExtensions();
            //dá uma nota pra GPU: quanto maior, melhor; 0 = inutilizável
            int rateDeviceSuitability(VkPhysicalDevice device);
            QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
            //confere se a GPU suporta as extensões que exigimos (ex: swap chain)
            bool checkDeviceExtensionSupport(VkPhysicalDevice device);
            SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

            VkInstance instance_;
            VkDebugUtilsMessengerEXT debugMessenger_;
            VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
            //referência à janela: não é dona dela, só usa pra criar a surface
            LveWindow &window;
            VkCommandPool commandPool;
            VkSurfaceKHR surface_;
            VkDevice device_;
            VkQueue graphicsQueue_;
            VkQueue presentQueue_;

            const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};
            //extensão obrigatória pra poder usar swap chain
            const std::vector<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    };
}
