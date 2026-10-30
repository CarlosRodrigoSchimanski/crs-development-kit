#include "engine/rendering/systems/simple_render_system.hpp"

#include <glm/glm.hpp>

#include <stdexcept>
#include <cassert>

namespace crsdk {
    struct SimplePushConstantData{
        glm::mat4 modelMatrix{1.f};
        glm::mat4 normalMatrix{1.f};
    };

    //tem que bater com o bloco Push do skinned.vert (a matriz das normais é feita no shader:
    //com ela aqui passaria dos 128 bytes que toda GPU garante pra push constant)
    struct SkinnedPushConstantData {
        glm::mat4 modelMatrix{1.f};
        uint32_t boneOffset = 0;
    };

    //quantas texturas diferentes cabem no pool de materiais
    constexpr uint32_t MAX_MATERIALS = 100;


    SimpleRenderSystem::SimpleRenderSystem(CrsDevice& device, VkFormat colorFormat, VkFormat depthFormat, VkDescriptorSetLayout globalSetLayout) : crsDevice{device} {
        materialSetLayout = CrsDescriptorSetLayout::Builder{crsDevice}
            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();
        materialPool = CrsDescriptorPool::Builder{crsDevice}
            .setMaxSets(MAX_MATERIALS)
            .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_MATERIALS)
            .build();
        const unsigned char white[4] = {255, 255, 255, 255};
        CrsUploadBatch upload{crsDevice};
        whiteTexture = std::make_unique<CrsTexture>(crsDevice, 1, 1, white, upload);
        upload.submit();

        //set 0 = global (ubo + shadow map), set 1 = textura do objeto
        pipelineLayout = CrsPipeline::createPipelineLayout(
            crsDevice,
            {globalSetLayout, materialSetLayout->getDescriptorSetLayout()},
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            sizeof(SimplePushConstantData));
        //mesmos sets; push constant só no vertex shader
        skinnedPipelineLayout = CrsPipeline::createPipelineLayout(
            crsDevice,
            {globalSetLayout, materialSetLayout->getDescriptorSetLayout()},
            VK_SHADER_STAGE_VERTEX_BIT,
            sizeof(SkinnedPushConstantData));
        createPipeline(colorFormat, depthFormat);
    }

    void SimpleRenderSystem::createPipeline(VkFormat colorFormat, VkFormat depthFormat) {
        assert(pipelineLayout.get() != VK_NULL_HANDLE && "Cannot create pipeline before pipeline layout");

        PipelineConfigInfo pipelineConfig{};
        CrsPipeline::defaultPipelineConfigInfo(pipelineConfig);
        //desenha na imagem da tela (cor + profundidade)
        pipelineConfig.colorAttachmentFormats = {colorFormat};
        pipelineConfig.depthAttachmentFormat = depthFormat;
        pipelineConfig.pipelineLayout = pipelineLayout.get();
        crsPipeline = std::make_unique<CrsPipeline>(
            crsDevice,
            "shaders/simple_shader.vert.spv",
            "shaders/simple_shader.frag.spv",
            pipelineConfig);

        //skinning: dois buffers de vértice (o normal + os pesos dos ossos), o mesmo fragment shader
        PipelineConfigInfo skinnedConfig{};
        CrsPipeline::defaultPipelineConfigInfo(skinnedConfig);
        skinnedConfig.bindingDescriptions = CrsModel::getSkinnedBindingDescriptions();
        skinnedConfig.attributeDescriptions = CrsModel::getSkinnedAttributeDescriptions();
        skinnedConfig.colorAttachmentFormats = {colorFormat};
        skinnedConfig.depthAttachmentFormat = depthFormat;
        skinnedConfig.pipelineLayout = skinnedPipelineLayout.get();
        skinnedPipeline = std::make_unique<CrsPipeline>(
            crsDevice,
            "shaders/skinned.vert.spv",
            "shaders/simple_shader.frag.spv",
            skinnedConfig);
    }

    void SimpleRenderSystem::bindMaterial(FrameInfo &frameInfo, CrsGameObject &obj, VkPipelineLayout layout) {
        //set 1: textura do modelo; sem textura (ou com useTexture desligado) usa a branca
        CrsTexture *texture = obj.model->getTexture();
        if (texture == nullptr || !obj.useTexture) texture = whiteTexture.get();
        VkDescriptorSet materialSet = getMaterialSet(*texture);
        vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 1, 1, &materialSet, 0, nullptr);
    }

    void SimpleRenderSystem::renderGameObjects(FrameInfo& frameInfo) {
        crsPipeline->bind(frameInfo.commandBuffer);

        vkCmdBindDescriptorSets(
            frameInfo.commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout.get(),
            0,
            1,
            &frameInfo.globalDescriptorSet,
            0,
            nullptr);

        for (auto& kv : frameInfo.crsScene.getGameObjects()) {
            auto& obj = kv.second;
            if (obj.model == nullptr) continue;
            if (obj.editorOnly) continue;  //gizmos são do GizmoRenderSystem
            if (obj.isAnimated()) continue;  //desenhado depois, pela pipeline de skinning

            bindMaterial(frameInfo, obj, pipelineLayout.get());

            SimplePushConstantData push{};
            glm::mat4 world = frameInfo.crsScene.worldMatrix(obj);
            push.modelMatrix = world;
            //normais com a escala de todos os pais: inversa transposta da parte 3x3 do mundo
            push.normalMatrix = glm::transpose(glm::inverse(glm::mat3{world}));

            vkCmdPushConstants(
                frameInfo.commandBuffer,
                pipelineLayout.get(),
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(SimplePushConstantData),
                &push
            );

            obj.model->bind(frameInfo.commandBuffer);
            obj.model->draw(frameInfo.commandBuffer);
        }

        //objetos animados: outra pipeline (o layout é diferente, então o set global é ligado de novo)
        skinnedPipeline->bind(frameInfo.commandBuffer);
        vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            skinnedPipelineLayout.get(), 0, 1, &frameInfo.globalDescriptorSet, 0, nullptr);

        for (auto& kv : frameInfo.crsScene.getGameObjects()) {
            auto& obj = kv.second;
            if (!obj.isAnimated() || obj.editorOnly) continue;

            bindMaterial(frameInfo, obj, skinnedPipelineLayout.get());

            SkinnedPushConstantData push{};
            push.modelMatrix = frameInfo.crsScene.worldMatrix(obj);
            push.boneOffset = obj.animator->boneOffset;
            vkCmdPushConstants(frameInfo.commandBuffer, skinnedPipelineLayout.get(), VK_SHADER_STAGE_VERTEX_BIT,
                0, sizeof(SkinnedPushConstantData), &push);

            obj.model->bindSkinned(frameInfo.commandBuffer);
            obj.model->draw(frameInfo.commandBuffer);
        }
    }

    VkDescriptorSet SimpleRenderSystem::getMaterialSet(const CrsTexture &texture) {
        auto existing = materialSets.find(&texture);
        if (existing != materialSets.end()) return existing->second;

        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler = texture.getSampler();
        imageInfo.imageView = texture.getImageView();
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorSet materialSet;
        if (!CrsDescriptorWriter(*materialSetLayout, *materialPool).writeImage(0, &imageInfo).build(materialSet)) {
            throw std::runtime_error("failed to allocate material descriptor set (pool cheio?)");
        }
        materialSets.emplace(&texture, materialSet);
        return materialSet;
    }
}
