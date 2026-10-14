#include "engine/core/lve_device.hpp"

#include <vk_mem_alloc.h>

#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>

namespace lve {

    //callback chamado pelas validation layers sempre que detectam algo suspeito
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT /*messageSeverity*/,
        VkDebugUtilsMessageTypeFlagsEXT /*messageType*/,
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
        void * /*pUserData*/) {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
        return VK_FALSE;
    }

    //vkCreateDebugUtilsMessengerEXT é uma função de extensão, precisa ser carregada manualmente
    VkResult CreateDebugUtilsMessengerEXT(
        VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
        const VkAllocationCallbacks *pAllocator,
        VkDebugUtilsMessengerEXT *pDebugMessenger) {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr) {
            return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
        } else {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    bool QueueFamilyIndices::isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }

    VkInstance LveDevice::instance() const { return instance_.get(); }
    VkPhysicalDevice LveDevice::physicalDevice() const { return physicalDevice_; }
    VkDevice LveDevice::device() const { return device_.get(); }
    VkSurfaceKHR LveDevice::surface() const { return surface_.get(); }
    VkQueue LveDevice::graphicsQueue() const { return graphicsQueue_; }
    VmaAllocator LveDevice::allocator() const { return allocator_.get(); }
    VkQueue LveDevice::presentQueue() const { return presentQueue_; }
    VkCommandPool LveDevice::getCommandPool() const { return commandPool.get(); }

    SwapChainSupportDetails LveDevice::getSwapChainSupport() const { return querySwapChainSupport(physicalDevice_); }
    QueueFamilyIndices LveDevice::findPhysicalQueueFamilies() const { return findQueueFamilies(physicalDevice_); }

    LveDevice::LveDevice(LveWindow &window) : window{window} {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createAllocator();
        createCommandPool();    }

    void LveDevice::createInstance() {
        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "geYoutube App";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        //1.3: dynamic rendering e synchronization2 fazem parte do núcleo (sem extensão)
        appInfo.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;
        } else {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
        }

        VkInstance instance;
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }
        instance_ = LveInstance{instance};
    }

    void LveDevice::setupDebugMessenger() {
        if (!enableValidationLayers) return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);
        VkDebugUtilsMessengerEXT messenger;
        if (CreateDebugUtilsMessengerEXT(instance_.get(), &createInfo, nullptr, &messenger) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
        debugMessenger_ = LveDebugMessenger{instance_.get(), messenger};
    }

    void LveDevice::createSurface() {
        VkSurfaceKHR surface;
        window.createWindowSurface(instance_.get(), &surface);
        surface_ = LveSurface{instance_.get(), surface};
    }

    void LveDevice::pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance_.get(), &deviceCount, nullptr);
        if (deviceCount == 0) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance_.get(), &deviceCount, devices.data());

        //compara a pontuação de cada GPU disponível e guarda a melhor
        int bestScore = 0;
        for (const auto &device : devices) {
            int score = rateDeviceSuitability(device);
            if (score > bestScore) {
                bestScore = score;
                physicalDevice_ = device;
            }
        }

        if (physicalDevice_ == VK_NULL_HANDLE) {
            throw std::runtime_error("failed to find a suitable GPU!");
        }

        vkGetPhysicalDeviceProperties(physicalDevice_, &properties);
        std::cout << "GPU escolhida: " << properties.deviceName << " (pontuação: " << bestScore << ")" << std::endl;
    }

    void LveDevice::createLogicalDevice() {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);

        //usa um set pra não criar 2 filas se graphics e present forem a mesma família
        std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        //recursos do 1.3 que a engine usa; a forma moderna (Features2 + pNext) permite encadear
        //as structs de cada versão, e substitui o pEnabledFeatures (que tem que ficar nulo)
        VkPhysicalDeviceVulkan13Features vulkan13Features{};
        vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        vulkan13Features.dynamicRendering = VK_TRUE;
        vulkan13Features.synchronization2 = VK_TRUE;

        VkPhysicalDeviceFeatures2 deviceFeatures{};
        deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures.pNext = &vulkan13Features;

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pNext = &deviceFeatures;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        //validation layers só são válidas no nível da instance (ver createInstance);
        //configurá-las aqui no device é comportamento legado que o Vulkan moderno rejeita
        createInfo.enabledLayerCount = 0;

        VkDevice device;
        if (vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }
        device_ = LveDeviceHandle{device};

        vkGetDeviceQueue(device_.get(), indices.graphicsFamily.value(), 0, &graphicsQueue_);
        vkGetDeviceQueue(device_.get(), indices.presentFamily.value(), 0, &presentQueue_);
    }

    void LveDevice::createCommandPool() {
        QueueFamilyIndices queueFamilyIndices = findPhysicalQueueFamilies();

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
        poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        VkCommandPool pool;
        if (vkCreateCommandPool(device_.get(), &poolInfo, nullptr, &pool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }
        commandPool = LveCommandPool{device_.get(), pool};
    }

    void LveDevice::createAllocator() {
        VmaAllocatorCreateInfo allocatorInfo{};
        //a mesma versão pedida no createInstance: deixa a VMA usar o que é do núcleo do 1.3
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
        allocatorInfo.instance = instance_.get();
        allocatorInfo.physicalDevice = physicalDevice_;
        allocatorInfo.device = device_.get();

        VmaAllocator allocator;
        if (vmaCreateAllocator(&allocatorInfo, &allocator) != VK_SUCCESS) {
            throw std::runtime_error("failed to create memory allocator!");
        }
        allocator_ = LveAllocatorHandle{allocator};
    }

    int LveDevice::rateDeviceSuitability(VkPhysicalDevice device) {
        QueueFamilyIndices indices = findQueueFamilies(device);

        //sem fila gráfica+apresentação a GPU não serve pra nada aqui, pontuação zero
        if (!indices.isComplete()) {
            return 0;
        }

        //sem suporte à extensão de swap chain, também não serve
        if (!checkDeviceExtensionSupport(device)) {
            return 0;
        }

        //precisa ter pelo menos um formato de imagem e um modo de apresentação disponível
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        bool swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        if (!swapChainAdequate) {
            return 0;
        }

        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);

        //a engine pede Vulkan 1.3 (createInstance); GPU/driver mais antigo não serve
        if (deviceProperties.apiVersion < VK_API_VERSION_1_3) {
            return 0;
        }

        //e os recursos do 1.3 que o createLogicalDevice liga têm que existir de fato
        VkPhysicalDeviceVulkan13Features vulkan13Features{};
        vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        VkPhysicalDeviceFeatures2 deviceFeatures{};
        deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures.pNext = &vulkan13Features;
        vkGetPhysicalDeviceFeatures2(device, &deviceFeatures);
        if (!vulkan13Features.dynamicRendering || !vulkan13Features.synchronization2) {
            return 0;
        }

        int score = 0;

        //GPU dedicada tem prioridade enorme sobre a integrada
        if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 1000;
        }

        //quanto maior a textura máxima suportada, mais capaz costuma ser a GPU
        score += deviceProperties.limits.maxImageDimension2D;

        return score;
    }

    QueueFamilyIndices LveDevice::findQueueFamilies(VkPhysicalDevice device) const {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto &queueFamily : queueFamilies) {
            if (queueFamily.queueCount > 0 && (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                indices.graphicsFamily = i;
            }

            //confere se essa família de filas consegue apresentar imagem na nossa surface
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_.get(), &presentSupport);
            if (queueFamily.queueCount > 0 && presentSupport) {
                indices.presentFamily = i;
            }

            if (indices.isComplete()) {
                break;
            }
            i++;
        }

        return indices;
    }

    bool LveDevice::checkDeviceExtensionSupport(VkPhysicalDevice device) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto &extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    SwapChainSupportDetails LveDevice::querySwapChainSupport(VkPhysicalDevice device) const {
        SwapChainSupportDetails details;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_.get(), &details.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_.get(), &formatCount, nullptr);
        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_.get(), &formatCount, details.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_.get(), &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_.get(), &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    std::vector<const char *> LveDevice::getRequiredExtensions() {
        uint32_t glfwExtensionCount = 0;
        const char **glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    bool LveDevice::checkValidationLayerSupport() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char *layerName : validationLayers) {
            bool layerFound = false;
            for (const auto &layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }
            if (!layerFound) {
                return false;
            }
        }

        return true;
    }

    void LveDevice::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo) {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        //só avisos e erros; VERBOSE é diagnóstico interno do loader ("Loading layer library...")
        //e escondia os problemas reais no meio do ruído
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
        createInfo.pUserData = nullptr;
    }

    VkFormat LveDevice::findSupportedFormat(
        const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const {
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physicalDevice_, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                return format;
            } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }
        throw std::runtime_error("failed to find supported format!");
    }

    LveAllocatedImage LveDevice::createImageWithInfo(const VkImageCreateInfo &imageInfo, VkMemoryPropertyFlags requiredProperties) {
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.requiredFlags = requiredProperties;

        //cria a imagem, aloca e liga a memória, tudo numa chamada (igual o vmaCreateBuffer do LveBuffer)
        VkImage image;
        VmaAllocation allocation;
        if (vmaCreateImage(allocator_.get(), &imageInfo, &allocInfo, &image, &allocation, nullptr) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image!");
        }
        //daqui em diante a imagem já tem dono
        return LveAllocatedImage{allocator_.get(), image, allocation};
    }
}
