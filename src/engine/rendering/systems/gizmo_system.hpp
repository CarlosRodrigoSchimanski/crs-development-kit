#pragma once

#include "engine/core/lve_device.hpp"
#include "engine/rendering/lve_pipeline.hpp"
#include "engine/rendering/lve_frameInfo.hpp"

#include <memory>

namespace lve {
    //desenha os objetos só do editor 'gizmos', semi tranparente
    class GizmoRenderSystem {
        public:
            GizmoRenderSystem(LveDevice& device, VkFormat colorFormat, VkFormat depthFormat, VkDescriptorSetLayout globalSetLayout);

            GizmoRenderSystem(const GizmoRenderSystem&) = delete;
            GizmoRenderSystem& operator = (const GizmoRenderSystem&) = delete;

            void render(FrameInfo& frameInfo);
        
        private:
            void createPipeline(VkFormat colorFormat, VkFormat depthFormat);

            LveDevice& lveDevice;
            LvePipelineLayout pipelineLayout;

            //preenchimento transparente
            std::unique_ptr<LvePipeline> fillPipeline;
        };
}