#pragma once

#include "engine/rendering/crs_camera.hpp"
#include "engine/core/crs_device.hpp"
#include "engine/rendering/crs_pipeline.hpp"
#include "engine/scene/crs_game_object.hpp"
#include "engine/rendering/crs_frameInfo.hpp"
#include "engine/rendering/crs_descriptors.hpp"
#include "engine/rendering/crs_texture.hpp"

#include <memory>
#include <vector>
#include <unordered_map>

namespace crsdk {
    //desenha os objetos que têm modelo, com textura (set 1) e iluminação (set 0)
    class SimpleRenderSystem {
        public:

            SimpleRenderSystem(CrsDevice& device, VkFormat colorFormat, VkFormat depthFormat, VkDescriptorSetLayout globalSetLayout);

            //codigo para evitar duplicação de ponteiros
            SimpleRenderSystem(const SimpleRenderSystem&) = delete;
            SimpleRenderSystem& operator=(const SimpleRenderSystem&) = delete;

            void renderGameObjects(FrameInfo& frameInfo);

        private:
            void createPipeline(VkFormat colorFormat, VkFormat depthFormat);
            //descriptor set (set 1) da textura; criado na primeira vez que a textura é desenhada e reaproveitado depois
            VkDescriptorSet getMaterialSet(const CrsTexture &texture);
            //liga a textura do objeto (set 1) na pipeline atual
            void bindMaterial(FrameInfo &frameInfo, CrsGameObject &obj, VkPipelineLayout layout);

            CrsDevice& crsDevice;
            std::unique_ptr<CrsPipeline> crsPipeline;
            CrsPipelineLayout pipelineLayout;
            //objetos animados: lê também os pesos dos ossos (binding 1) e o buffer de ossos (binding 3)
            std::unique_ptr<CrsPipeline> skinnedPipeline;
            CrsPipelineLayout skinnedPipelineLayout;

            std::unique_ptr<CrsDescriptorSetLayout> materialSetLayout;
            std::unique_ptr<CrsDescriptorPool> materialPool;
            //usada por quem não tem textura: branco * cor = a própria cor
            std::unique_ptr<CrsTexture> whiteTexture;
            std::unordered_map<const CrsTexture *, VkDescriptorSet> materialSets;

    };
}