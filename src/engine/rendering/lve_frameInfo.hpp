#pragma once

#include "engine/rendering/lve_camera.hpp"
#include "engine/scene/lve_game_object.hpp"

#include <vulkan/vulkan.h>


namespace lve {
    //constexpr em vez de #define: respeita o namespace (lve::MAX_LIGHTS), tem tipo e aparece no debugger
    //tem que bater com o tamanho do array pointLight[] nos shaders
    constexpr int MAX_LIGHTS = 10;

    struct PointLight {
        glm::vec4 position{};
        glm::vec4 color{};
    };

    struct GlobalUbo {
        glm::mat4 projection{1.f};
        glm::mat4 view{1.f};
        glm::mat4 inverseView{1.f};
        glm::mat4 lightSpaceMatrix{1.f};
        glm::vec4 sunDirection = {-1.f, -4.f, -1.f, 1.f};

        glm::vec4 lightAmbientColor = {1.f, 1.f, 1.f, 0.35f};
        PointLight pointLights[MAX_LIGHTS];
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