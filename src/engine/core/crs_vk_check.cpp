#include "engine/core/crs_vk_check.hpp"

//helper do próprio SDK do Vulkan: string_VkResult(VK_ERROR_DEVICE_LOST) == "VK_ERROR_DEVICE_LOST"
#include <vulkan/vk_enum_string_helper.h>

#include <stdexcept>
#include <string>

namespace crsdk {

    void vkCheck(VkResult result, const char *what) {
        if (result < 0) {
            throw std::runtime_error(std::string{"Vulkan: "} + what + " falhou (" + string_VkResult(result) + ")");
        }
    }
}
