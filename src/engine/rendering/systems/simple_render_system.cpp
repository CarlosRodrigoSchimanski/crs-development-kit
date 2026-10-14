#include "engine/rendering/systems/simple_render_system.hpp"

#include <glm/glm.hpp>

#include <stdexcept>
#include <cassert>

namespace lve {
    struct SimplePushConstantData{
        glm::mat4 modelMatrix{1.f};
        glm::mat4 normalMatrix{1.f};
    };

    //quantas texturas diferentes cabem no pool de materiais
    constexpr uint32_t MAX_MATERIALS = 100;
    

    SimpleRenderSystem::SimpleRenderSystem(LveDevice& device, VkFormat colorFormat, VkFormat depthFormat, VkDescriptorSetLayout globalSetLayout) : lveDevice{device} {
        materialSetLayout = LveDescriptorSetLayout::Builder{lveDevice}
            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();
        materialPool = LveDescriptorPool::Builder{lveDevice}
            .setMaxSets(MAX_MATERIALS)
            .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_MATERIALS)
            .build();
        const unsigned char white[4] = {255, 255, 255, 255};
        whiteTexture = std::make_unique<LveTexture>(lveDevice, 1, 1, white);

        //set 0 = global (ubo + shadow map), set 1 = textura do objeto
        pipelineLayout = LvePipeline::createPipelineLayout(
            lveDevice,
            {globalSetLayout, materialSetLayout->getDescriptorSetLayout()},
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            sizeof(SimplePushConstantData));
        createPipeline(colorFormat, depthFormat);
    }

    SimpleRenderSystem::~SimpleRenderSystem() {
        vkDeviceWaitIdle(lveDevice.device());
        vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
    }


    void SimpleRenderSystem::createPipeline(VkFormat colorFormat, VkFormat depthFormat) {
        assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

        PipelineConfigInfo pipelineConfig{};
        LvePipeline::defaultPipelineConfigInfo(pipelineConfig);
        //desenha na imagem da tela (cor + profundidade)
        pipelineConfig.colorAttachmentFormats = {colorFormat};
        pipelineConfig.depthAttachmentFormat = depthFormat;
        pipelineConfig.pipelineLayout = pipelineLayout;
        lvePipeline = std::make_unique<LvePipeline>(
            lveDevice,
            "shaders/simple_shader.vert.spv",
            "shaders/simple_shader.frag.spv",
            pipelineConfig);
    }

    void SimpleRenderSystem::renderGameObjects(FrameInfo& frameInfo) {
        lvePipeline->bind(frameInfo.commandBuffer);

        vkCmdBindDescriptorSets(
            frameInfo.commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout,
            0,
            1,
            &frameInfo.globalDescriptorSet,
            0,
            nullptr);

        for (auto& kv : frameInfo.gameObjects) {
            auto& obj = kv.second;
            if (obj.model == nullptr) continue;

            //set 1: textura do modelo; sem textura (ou com useTexture desligado) usa a branca
            LveTexture *texture = obj.model->getTexture();
            if (texture == nullptr || !obj.useTexture) texture = whiteTexture.get();
            VkDescriptorSet materialSet = getMaterialSet(*texture);
            vkCmdBindDescriptorSets(
                frameInfo.commandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipelineLayout,
                1,
                1,
                &materialSet,
                0,
                nullptr);

            SimplePushConstantData push{};
            push.modelMatrix = obj.transform.mat4();
            push.normalMatrix = obj.transform.normalMatrix();

            vkCmdPushConstants(
                frameInfo.commandBuffer,
                pipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(SimplePushConstantData),
                &push
            );

            obj.model->bind(frameInfo.commandBuffer);
            obj.model->draw(frameInfo.commandBuffer);
        }
    }

    VkDescriptorSet SimpleRenderSystem::getMaterialSet(const LveTexture &texture) {
        auto existing = materialSets.find(&texture);
        if (existing != materialSets.end()) return existing->second;

        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler = texture.getSampler();
        imageInfo.imageView = texture.getImageView();
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorSet materialSet;
        if (!LveDescriptorWriter(*materialSetLayout, *materialPool).writeImage(0, &imageInfo).build(materialSet)) {
            throw std::runtime_error("failed to allocate material descriptor set (pool cheio?)");
        }
        materialSets.emplace(&texture, materialSet);
        return materialSet;
    }
}
