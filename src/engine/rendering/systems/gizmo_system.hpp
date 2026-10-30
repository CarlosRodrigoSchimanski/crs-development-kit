#pragma once

#include "engine/core/crs_device.hpp"
#include "engine/rendering/crs_pipeline.hpp"
#include "engine/rendering/crs_frameInfo.hpp"

#include <memory>

namespace crsdk {
    //desenha os objetos só do editor 'gizmos', semi tranparente
    class GizmoRenderSystem {
        public:
            GizmoRenderSystem(CrsDevice& device, VkFormat colorFormat, VkFormat depthFormat, VkDescriptorSetLayout globalSetLayout);

            GizmoRenderSystem(const GizmoRenderSystem&) = delete;
            GizmoRenderSystem& operator = (const GizmoRenderSystem&) = delete;

            void render(FrameInfo& frameInfo);
        
        private:
            void createPipeline(VkFormat colorFormat, VkFormat depthFormat);

            CrsDevice& crsDevice;
            CrsPipelineLayout pipelineLayout;

            //preenchimento transparente
            std::unique_ptr<CrsPipeline> fillPipeline;

            //arestas reais em linha, opacas
            std::unique_ptr<CrsPipeline> edgePipeline;

        };
}