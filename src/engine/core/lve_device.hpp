#pragma once

#include "engine/core/lve_window.hpp"

//declaração antecipada dos handles da VMA (são as mesmas linhas que o vk_mem_alloc.h usa): assim
//este header, incluído no projeto inteiro, não precisa puxar as ~20 mil linhas da VMA; só os .cpp
//que chamam funções da VMA incluem <vk_mem_alloc.h>
VK_DEFINE_HANDLE(VmaAllocator)
VK_DEFINE_HANDLE(VmaAllocation)

#include <array>
#include <optional>
#include <vector>

namespace lve {

    // O que a surface suporta (formatos, modos de apresentação, tamanhos): o swap chain consulta pra se configurar
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    // Agrupa os índices das filas (queues) que a GPU escolhida oferece
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() const;
    };

    // Classe responsável por: instance, validation layers, physical device e logical device
    class LveDevice {
        public:
            //static constexpr: é configuração do programa, não dado de cada LveDevice
#ifdef NDEBUG
            static constexpr bool enableValidationLayers = false;
#else
            static constexpr bool enableValidationLayers = true;
#endif

            //recebe a janela pra criar a surface
            LveDevice(LveWindow &window);
            ~LveDevice();

            //não copiável: é dono de recursos únicos do Vulkan (instance, device...).
            //apagar a cópia já impede o move também (o compilador não gera move sozinho nesse caso)
            LveDevice(const LveDevice &) = delete;
            LveDevice &operator=(const LveDevice &) = delete;

            //propriedades/limites da GPU escolhida (ex: alinhamento mínimo pra uniform buffer)
            VkPhysicalDeviceProperties properties;

            VkInstance instance() const;
            VkPhysicalDevice physicalDevice() const;
            VkDevice device() const;
            VkSurfaceKHR surface() const;
            VkQueue graphicsQueue() const;
            VkQueue presentQueue() const;
            VkCommandPool getCommandPool() const;
            //alocador de memória (VMA): de onde saem as memórias de buffers e imagens
            VmaAllocator allocator() const;

            SwapChainSupportDetails getSwapChainSupport() const;
            QueueFamilyIndices findPhysicalQueueFamilies() const;

            //acha, entre os formatos candidatos, o primeiro que a GPU realmente suporta pro uso pedido
            VkFormat findSupportedFormat(
                const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;
            //cria uma VkImage já com a memória alocada (pela VMA) e vinculada;
            //destruir com vmaDestroyImage(allocator(), image, allocation)
            void createImageWithInfo(
                const VkImageCreateInfo &imageInfo,
                VkMemoryPropertyFlags requiredProperties,
                VkImage &image,
                VmaAllocation &allocation);

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
            //cria o command pool de onde saem os command buffers
            void createCommandPool();
            //cria o alocador da VMA (precisa do instance, da GPU e do device já prontos)
            void createAllocator();

            //funções auxiliares
            bool checkValidationLayerSupport();
            std::vector<const char *> getRequiredExtensions();
            void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo);
            //dá uma nota pra GPU: quanto maior, melhor; 0 = inutilizável
            int rateDeviceSuitability(VkPhysicalDevice device);
            QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
            //confere se a GPU suporta as extensões que exigimos (ex: swap chain)
            bool checkDeviceExtensionSupport(VkPhysicalDevice device);
            SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) const;

            VkInstance instance_;
            VkDebugUtilsMessengerEXT debugMessenger_;
            VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
            //referência à janela: não é dona dela, só usa pra criar a surface
            LveWindow &window;
            VkCommandPool commandPool;
            VkSurfaceKHR surface_;
            VkDevice device_;
            VmaAllocator allocator_;
            VkQueue graphicsQueue_;
            VkQueue presentQueue_;

            static constexpr std::array<const char *, 1> validationLayers = {"VK_LAYER_KHRONOS_validation"};
            //extensão obrigatória pra poder usar swap chain
            static constexpr std::array<const char *, 1> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    };
}
