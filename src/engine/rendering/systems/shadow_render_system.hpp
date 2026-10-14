#pragma once

#include "engine/core/lve_device.hpp"
#include "engine/rendering/lve_pipeline.hpp"
#include "engine/rendering/lve_frameInfo.hpp"
#include "engine/rendering/lve_shadow_map.hpp"

#include <memory>

namespace lve {
    //desenha a cena do ponto de vista do sol no shadow map (só profundidade)
    class ShadowRenderSystem {
        public:
            ShadowRenderSystem(LveDevice& device, LveShadowMap& shadowMap, VkDescriptorSetLayout globalSetLayout);

            ShadowRenderSystem(const ShadowRenderSystem&) = delete;
            ShadowRenderSystem& operator=(const ShadowRenderSystem&) = delete;

            //grava o passe de sombra inteiro (barreira -> begin rendering -> objetos -> end -> barreira);
            //chamar ANTES do passe da swap chain
            void renderShadowMap(FrameInfo& frameInfo);

        private:
            void createPipeline();

            LveDevice& lveDevice;
            LveShadowMap& shadowMap;
            std::unique_ptr<LvePipeline> lvePipeline;
            LvePipelineLayout pipelineLayout;
    };
}
