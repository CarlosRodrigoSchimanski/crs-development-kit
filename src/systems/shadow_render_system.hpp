#pragma once

#include "lve_device.hpp"
#include "lve_pipeline.hpp"
#include "lve_frameInfo.hpp"
#include "lve_shadow_map.hpp"

#include <memory>

namespace lve {
    //desenha a cena do ponto de vista do sol no shadow map (só profundidade)
    class ShadowRenderSystem {
        public:
            ShadowRenderSystem(LveDevice& device, LveShadowMap& shadowMap, VkDescriptorSetLayout globalSetLayout);
            ~ShadowRenderSystem();

            ShadowRenderSystem(const ShadowRenderSystem&) = delete;
            ShadowRenderSystem& operator=(const ShadowRenderSystem&) = delete;

            //grava o passe de sombra inteiro (begin render pass -> objetos -> end); chamar ANTES do passe da swap chain
            void renderShadowMap(FrameInfo& frameInfo);

        private:
            void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
            void createPipeline();

            LveDevice& lveDevice;
            LveShadowMap& shadowMap;
            std::unique_ptr<LvePipeline> lvePipeline;
            VkPipelineLayout pipelineLayout;
    };
}
