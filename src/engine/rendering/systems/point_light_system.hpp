#pragma once

#include "engine/rendering/lve_camera.hpp"
#include "engine/core/lve_device.hpp"
#include "engine/rendering/lve_pipeline.hpp"
#include "engine/scene/lve_game_object.hpp"
#include "engine/rendering/lve_frameInfo.hpp"
#include "engine/rendering/lve_buffer.hpp"

#include <memory>
#include <vector>

namespace lve {
    class PointLightSystem {
        public:

            PointLightSystem(LveDevice& device, VkFormat colorFormat, VkFormat depthFormat, VkDescriptorSetLayout globalSetLayout);
            ~PointLightSystem();

            PointLightSystem(const PointLightSystem&) = delete;
            PointLightSystem& operator=(const PointLightSystem&) = delete;

            //copia as point lights da cena pro lightBuffer (storage buffer do frame) e põe a
            //quantidade no ubo.numLights
            void update(FrameInfo& frameInfo, GlobalUbo& ubo, LveBuffer& lightBuffer);
            void render(FrameInfo& frameInfo);

        private:
            void createPipeline(VkFormat colorFormat, VkFormat depthFormat);

            LveDevice& lveDevice;
            std::unique_ptr<LvePipeline> lvePipeline;
            VkPipelineLayout pipelineLayout;

    };
}
