#pragma once

#include "engine/rendering/lve_camera.hpp"
#include "engine/core/lve_device.hpp"
#include "engine/rendering/lve_pipeline.hpp"
#include "engine/scene/lve_game_object.hpp"
#include "engine/rendering/lve_frameInfo.hpp"
#include "engine/rendering/lve_descriptors.hpp"
#include "engine/rendering/lve_texture.hpp"

#include <memory>
#include <vector>
#include <unordered_map>

namespace lve {
    //desenha os objetos que têm modelo, com textura (set 1) e iluminação (set 0)
    class SimpleRenderSystem {
        public:

            SimpleRenderSystem(LveDevice& device, VkFormat colorFormat, VkFormat depthFormat, VkDescriptorSetLayout globalSetLayout);

            //codigo para evitar duplicação de ponteiros
            SimpleRenderSystem(const SimpleRenderSystem&) = delete;
            SimpleRenderSystem& operator=(const SimpleRenderSystem&) = delete;

            void renderGameObjects(FrameInfo& frameInfo);

        private:
            void createPipeline(VkFormat colorFormat, VkFormat depthFormat);
            //descriptor set (set 1) da textura; criado na primeira vez que a textura é desenhada e reaproveitado depois
            VkDescriptorSet getMaterialSet(const LveTexture &texture);
            //liga a textura do objeto (set 1) na pipeline atual
            void bindMaterial(FrameInfo &frameInfo, LveGameObject &obj, VkPipelineLayout layout);

            LveDevice& lveDevice;
            std::unique_ptr<LvePipeline> lvePipeline;
            LvePipelineLayout pipelineLayout;
            //objetos animados: lê também os pesos dos ossos (binding 1) e o buffer de ossos (binding 3)
            std::unique_ptr<LvePipeline> skinnedPipeline;
            LvePipelineLayout skinnedPipelineLayout;

            std::unique_ptr<LveDescriptorSetLayout> materialSetLayout;
            std::unique_ptr<LveDescriptorPool> materialPool;
            //usada por quem não tem textura: branco * cor = a própria cor
            std::unique_ptr<LveTexture> whiteTexture;
            std::unordered_map<const LveTexture *, VkDescriptorSet> materialSets;

    };
}