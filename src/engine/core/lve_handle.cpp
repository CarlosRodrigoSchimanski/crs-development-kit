#include "engine/core/lve_handle.hpp"

#include <vk_mem_alloc.h>

namespace lve {

    void LveAllocatedImage::reset() {
        if (image != VK_NULL_HANDLE) {
            vmaDestroyImage(allocator, image, allocation);
            image = VK_NULL_HANDLE;
            allocation = VK_NULL_HANDLE;
        }
    }
}
