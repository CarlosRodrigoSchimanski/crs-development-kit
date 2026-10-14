#include "engine/core/lve_handle.hpp"

#include <vk_mem_alloc.h>

namespace lve {

    void destroyInstance(VkInstance instance) { vkDestroyInstance(instance, nullptr); }
    void destroyDevice(VkDevice device) { vkDestroyDevice(device, nullptr); }
    void destroyAllocator(VmaAllocator allocator) { vmaDestroyAllocator(allocator); }

    void destroyDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks *allocator) {
        auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (func != nullptr) {
            func(instance, messenger, allocator);
        }
    }

    void LveAllocatedImage::reset() {
        if (image != VK_NULL_HANDLE) {
            vmaDestroyImage(allocator, image, allocation);
            image = VK_NULL_HANDLE;
            allocation = VK_NULL_HANDLE;
        }
    }
}
