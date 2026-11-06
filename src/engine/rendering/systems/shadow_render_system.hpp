#pragma once

#include "engine/core/crs_device.hpp"
#include "engine/rendering/crs_pipeline.hpp"
#include "engine/rendering/crs_frameInfo.hpp"
#include "engine/rendering/crs_shadow_map.hpp"

#include "engine/rendering/crs_instance_batches.hpp"
#include "engine/rendering/crs_descriptors.hpp"
#include "engine/rendering/crs_texture.hpp"

#include <memory>
#include <unordered_map>

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
            //descriptor set (set 1) da textura, pra pipeline de recorte; criado na primeira vez e reaproveitado
            VkDescriptorSet getTextureSet(const CrsTexture &texture);

            CrsDevice& crsDevice;
            CrsShadowMap& shadowMap;
            std::unique_ptr<CrsPipeline> crsPipeline;
            CrsPipelineLayout pipelineLayout;
            //modelos com recorte (grama, folhas): lê a textura e descarta o transparente. Separada da
            //normal porque o discard tira um atalho da GPU: os modelos sólidos não pagam esse custo
            std::unique_ptr<CrsPipeline> cutoutPipeline;
            CrsPipelineLayout cutoutPipelineLayout;
            std::unique_ptr<CrsDescriptorSetLayout> textureSetLayout;
            std::unique_ptr<CrsDescriptorPool> texturePool;
            std::unordered_map<const CrsTexture *, VkDescriptorSet> textureSets;
            //objetos animados: a sombra também segue a pose (ossos do binding 3)
            std::unique_ptr<CrsPipeline> skinnedPipeline;
            //os objetos parados agrupados por modelo: uma chamada por grupo (como no SimpleRenderSystem)
            CrsInstanceBatches batches;
            CrsPipelineLayout skinnedPipelineLayout;
    };
}
