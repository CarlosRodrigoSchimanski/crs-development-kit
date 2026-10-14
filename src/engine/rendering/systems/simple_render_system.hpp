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
    // Classe FirstApp esta no mesmo nivel que a classe lvewindow
    class SimpleRenderSystem {
        public:

            // Construtor e destrutor da classe FirstApp
            SimpleRenderSystem(LveDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
            ~SimpleRenderSystem();

            //codigo para evitar duplicação de ponteiros
            SimpleRenderSystem(const SimpleRenderSystem&) = delete;
            SimpleRenderSystem& operator=(const SimpleRenderSystem&) = delete;

            void renderGameObjects(FrameInfo& frameInfo);

        private:
            //função createPipeline() que será responsável por criar o pipeline gráfico
            void createPipeline(VkRenderPass renderPass);
            //descriptor set (set 1) da textura; criado na primeira vez que a textura é desenhada e reaproveitado depois
            VkDescriptorSet getMaterialSet(const LveTexture &texture);

            LveDevice& lveDevice;
            std::unique_ptr<LvePipeline> lvePipeline;
            VkPipelineLayout pipelineLayout;

            std::unique_ptr<LveDescriptorSetLayout> materialSetLayout;
            std::unique_ptr<LveDescriptorPool> materialPool;
            //usada por quem não tem textura: branco * cor = a própria cor
            std::unique_ptr<LveTexture> whiteTexture;
            std::unordered_map<const LveTexture *, VkDescriptorSet> materialSets;

    };
}