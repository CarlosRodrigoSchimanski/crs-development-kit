#pragma once

#include "engine/rendering/lve_camera.hpp"
#include "engine/core/lve_device.hpp"
#include "engine/rendering/lve_pipeline.hpp"
#include "engine/scene/lve_game_object.hpp"
#include "engine/rendering/lve_frameInfo.hpp"

#include <memory>
#include <vector>

namespace lve {
    class PointLightSystem {
        public:

            PointLightSystem(LveDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
            ~PointLightSystem();

            PointLightSystem(const PointLightSystem&) = delete;
            PointLightSystem& operator=(const PointLightSystem&) = delete;

            void update(FrameInfo& frameInfo, GlobalUbo& ubo);
            void render(FrameInfo& frameInfo);

        private:
            void createPipeline(VkRenderPass renderPass);

            LveDevice& lveDevice;
            std::unique_ptr<LvePipeline> lvePipeline;
            VkPipelineLayout pipelineLayout;

    };
}
