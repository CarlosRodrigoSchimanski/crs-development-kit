#pragma once

#include "engine/core/crs_device.hpp"
#include "engine/rendering/crs_pipeline.hpp"
#include "engine/rendering/crs_frameInfo.hpp"
#include "engine/rendering/crs_shadow_map.hpp"

#include <memory>

namespace crsdk {
    //desenha a cena do ponto de vista do sol no shadow map (só profundidade)
    class ShadowRenderSystem {
        public:
            ShadowRenderSystem(CrsDevice& device, CrsShadowMap& shadowMap, VkDescriptorSetLayout globalSetLayout);

            ShadowRenderSystem(const ShadowRenderSystem&) = delete;
            ShadowRenderSystem& operator=(const ShadowRenderSystem&) = delete;

            //grava o passe de sombra inteiro (barreira -> begin rendering -> objetos -> end -> barreira);
            //chamar ANTES do passe da swap chain
            void renderShadowMap(FrameInfo& frameInfo);

        private:
            void createPipeline();

            CrsDevice& crsDevice;
            CrsShadowMap& shadowMap;
            std::unique_ptr<CrsPipeline> crsPipeline;
            CrsPipelineLayout pipelineLayout;
            //objetos animados: a sombra também segue a pose (ossos do binding 3)
            std::unique_ptr<CrsPipeline> skinnedPipeline;
            CrsPipelineLayout skinnedPipelineLayout;
    };
}
