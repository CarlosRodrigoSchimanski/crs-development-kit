#pragma once

#include "engine/core/lve_window.hpp"

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

            SwapChainSupportDetails getSwapChainSupport() const;
            QueueFamilyIndices findPhysicalQueueFamilies() const;

            //acha, entre os formatos candidatos, o primeiro que a GPU realmente suporta pro uso pedido
            VkFormat findSupportedFormat(
                const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;
            //acha o tipo de memória da GPU compatível com o que está sendo pedido
            uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags requiredProperties) const;
            //cria uma VkImage já com a memória alocada e vinculada
            void createImageWithInfo(
                const VkImageCreateInfo &imageInfo,
                VkMemoryPropertyFlags requiredProperties,
                VkImage &image,
                VkDeviceMemory &imageMemory);
            //cria um VkBuffer já com a memória alocada e vinculada
            void createBuffer(
                VkDeviceSize size,
                VkBufferUsageFlags usage,
                VkMemoryPropertyFlags requiredProperties,
                VkBuffer &buffer,
                VkDeviceMemory &bufferMemory);
            //command buffer descartável pra um comando avulso (cópia, troca de layout...):
            //begin aloca e começa a gravar; end submete, espera a GPU terminar e libera
            VkCommandBuffer beginSingleTimeCommands();
            void endSingleTimeCommands(VkCommandBuffer commandBuffer);
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
            //cria o command pool de onde saem os command buffers
            void createCommandPool();

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
            VkQueue graphicsQueue_;
            VkQueue presentQueue_;

            static constexpr std::array<const char *, 1> validationLayers = {"VK_LAYER_KHRONOS_validation"};
            //extensão obrigatória pra poder usar swap chain
            static constexpr std::array<const char *, 1> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    };
}
