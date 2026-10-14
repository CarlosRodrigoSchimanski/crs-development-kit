#pragma once

#include "engine/rendering/lve_camera.hpp"
#include "engine/scene/lve_game_object.hpp"

#include <vulkan/vulkan.h>


namespace lve {
    //uma point light como o shader enxerga (um item do storage buffer de luzes, binding 2)
    struct PointLight {
        glm::vec4 position{};
        glm::vec4 color{};  //w = intensidade
    };

    struct GlobalUbo {
        glm::mat4 projection{1.f};
        glm::mat4 view{1.f};
        glm::mat4 inverseView{1.f};
        glm::mat4 lightSpaceMatrix{1.f};
        glm::vec4 sunDirection = {-1.f, -4.f, -1.f, 1.f};

        glm::vec4 lightAmbientColor = {1.f, 1.f, 1.f, 0.35f};
        int numLights;
    };

    struct FrameInfo {
        int frameIndex;
        float frameTime;
        VkCommandBuffer commandBuffer;
        LveCamera &camera;
        VkDescriptorSet globalDescriptorSet;
        LveGameObject::Map &gameObjects;
    };

}