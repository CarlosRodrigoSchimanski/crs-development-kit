#include "editor/material_preview.hpp"

#include "engine/rendering/crs_image_barrier.hpp"
#include "engine/rendering/crs_material_files.hpp"
#include "engine/rendering/crs_upload_batch.hpp"

#include <imgui_impl_vulkan.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace crsdk {
    //tem que bater com o material_preview_push.glsl
    struct PreviewPushConstants {
        glm::mat4 viewProjection{1.f};
        glm::vec4 cameraPosition{0.f};
    };

    struct SphereVertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 uv;
        glm::vec3 tangent;
    };

    //a miniatura é em sRGB: o shader escreve a luz linear e o ImGui lê de volta já convertida
    constexpr VkFormat COLOR_FORMAT = VK_FORMAT_R8G8B8A8_SRGB;
    //o desenho de uma miniatura pode estar na GPU por até esses frames: os mapas só saem depois
    constexpr uint64_t RELEASE_AFTER_FRAMES = 4;
    //câmera de estúdio: de frente pra esfera (raio 1), um pouco afastada
    const glm::vec3 CAMERA_POSITION{0.f, 0.f, 3.1f};

    MaterialPreviews::MaterialPreviews(CrsDevice &device) : crsDevice{device} {
        depthFormat = crsDevice.findSupportedFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

        //set 0: os quatro mapas do material (cor, normal, roughness, AO)
        CrsDescriptorSetLayout::Builder layout{crsDevice};
        for (uint32_t binding = 0; binding < 4; binding++) {
            layout.addBinding(binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        }
        materialSetLayout = layout.build();
        materialPool = CrsDescriptorPool::Builder{crsDevice}
            .setMaxSets(8)
            .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 8 * 4)
            .allowFreeingSets()
            .build();

        const unsigned char white[4] = {255, 255, 255, 255};
        const unsigned char flat[4] = {128, 128, 255, 255};
        CrsUploadBatch upload{crsDevice};
        whiteData = std::make_unique<CrsTexture>(crsDevice, 1, 1, white, upload, TextureKind::Data);
        flatNormal = std::make_unique<CrsTexture>(crsDevice, 1, 1, flat, upload, TextureKind::Data);
        upload.submit();

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.maxLod = 1.f;
        VkSampler vkSampler;
        if (vkCreateSampler(crsDevice.device(), &samplerInfo, nullptr, &vkSampler) != VK_SUCCESS) {
            throw std::runtime_error("failed to create material preview sampler!");
        }
        sampler = CrsSampler{crsDevice.device(), vkSampler};

        pipelineLayout = CrsPipeline::createPipelineLayout(crsDevice, {materialSetLayout->getDescriptorSetLayout()},
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(PreviewPushConstants));
        createPipeline();
        createSphere();
    }

    MaterialPreviews::~MaterialPreviews() {
        //as miniaturas são imagens registradas no ImGui: devolve antes do ImGui fechar
        vkDeviceWaitIdle(crsDevice.device());
        for (auto &kv : previews) {
            if (kv.second.imguiSet != VK_NULL_HANDLE) ImGui_ImplVulkan_RemoveTexture(kv.second.imguiSet);
        }
    }

    void MaterialPreviews::createPipeline() {
        PipelineConfigInfo config{};
        CrsPipeline::defaultPipelineConfigInfo(config);
        config.bindingDescriptions = {{0, sizeof(SphereVertex), VK_VERTEX_INPUT_RATE_VERTEX}};
        config.attributeDescriptions = {
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SphereVertex, position)},
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SphereVertex, normal)},
            {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SphereVertex, uv)},
            {3, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SphereVertex, tangent)},
        };
        config.colorAttachmentFormats = {COLOR_FORMAT};
        config.depthAttachmentFormat = depthFormat;
        config.pipelineLayout = pipelineLayout.get();
        pipeline = std::make_unique<CrsPipeline>(crsDevice, "shaders/material_preview.vert.spv", "shaders/material_preview.frag.spv", config);
    }

    void MaterialPreviews::createSphere() {
        //esfera de raio 1 em faixas (latitude) e gomos (longitude); a textura dá 2 voltas em volta e
        //1 de cima a baixo, pra ficar na mesma proporção da foto
        constexpr uint32_t RINGS = 48;
        constexpr uint32_t SEGMENTS = 96;
        std::vector<SphereVertex> vertices;
        for (uint32_t ring = 0; ring <= RINGS; ring++) {
            const float theta = glm::pi<float>() * static_cast<float>(ring) / RINGS;  //0 = polo de cima
            for (uint32_t segment = 0; segment <= SEGMENTS; segment++) {
                const float phi = glm::two_pi<float>() * static_cast<float>(segment) / SEGMENTS;
                const glm::vec3 p{std::sin(theta) * std::sin(phi), std::cos(theta), std::sin(theta) * std::cos(phi)};
                //a tangente é pra onde a esfera anda quando o gomo aumenta (pra direita, vista de frente)
                vertices.push_back({p, p, {2.f * static_cast<float>(segment) / SEGMENTS, static_cast<float>(ring) / RINGS},
                                    {std::cos(phi), 0.f, -std::sin(phi)}});
            }
        }
        std::vector<uint32_t> indices;
        for (uint32_t ring = 0; ring < RINGS; ring++) {
            for (uint32_t segment = 0; segment < SEGMENTS; segment++) {
                const uint32_t a = ring * (SEGMENTS + 1) + segment;
                const uint32_t b = a + SEGMENTS + 1;
                indices.insert(indices.end(), {a, b, b + 1, a, b + 1, a + 1});
            }
        }
        indexCount = static_cast<uint32_t>(indices.size());

        const VkMemoryPropertyFlags hostMemory = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        vertexBuffer = std::make_unique<CrsBuffer>(crsDevice, sizeof(SphereVertex), static_cast<uint32_t>(vertices.size()),
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, hostMemory);
        vertexBuffer->map();
        vertexBuffer->writeToBuffer(vertices.data());
        vertexBuffer->unmap();
        indexBuffer = std::make_unique<CrsBuffer>(crsDevice, sizeof(uint32_t), indexCount, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, hostMemory);
        indexBuffer->map();
        indexBuffer->writeToBuffer(indices.data());
        indexBuffer->unmap();
    }

    ImTextureID MaterialPreviews::get(const std::string &folder) {
        if (folder.empty()) return ImTextureID{};
        auto found = previews.find(folder);
        if (found == previews.end()) {
            previews[folder];  //marca como pedida (desenhada no próximo render)
            pending.push_back(folder);
            return ImTextureID{};
        }
        return found->second.ready ? reinterpret_cast<ImTextureID>(found->second.imguiSet) : ImTextureID{};
    }

    //uma imagem do tamanho da miniatura (cor ou profundidade) e a view dela
    static void createTarget(CrsDevice &device, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect,
                             CrsAllocatedImage &image, CrsImageView &view) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = {MaterialPreviews::SIZE, MaterialPreviews::SIZE, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image = device.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image.get();
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange = {aspect, 0, 1, 0, 1};
        VkImageView vkView;
        if (vkCreateImageView(device.device(), &viewInfo, nullptr, &vkView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create material preview image view!");
        }
        view = CrsImageView{device.device(), vkView};
    }

    void MaterialPreviews::draw(VkCommandBuffer commandBuffer, const std::string &folder, Preview &preview) {
        //os mapas do material (sem a cor não tem o que mostrar: fica sem miniatura)
        const MaterialFiles files = findMaterialFiles(folder);
        if (files.albedo.empty()) return;
        CrsUploadBatch upload{crsDevice};
        auto load = [&](const std::string &path, TextureKind kind, const CrsTexture &fallback) -> const CrsTexture & {
            if (path.empty()) return fallback;
            preview.textures.push_back(std::make_unique<CrsTexture>(crsDevice, path, upload, kind));
            return *preview.textures.back();
        };
        const CrsTexture &albedo = load(files.albedo, TextureKind::Color, *whiteData);
        const CrsTexture &normal = load(files.normal, TextureKind::Data, *flatNormal);
        const CrsTexture &roughness = load(files.roughness, TextureKind::Data, *whiteData);
        const CrsTexture &ao = load(files.ao, TextureKind::Data, *whiteData);
        upload.submit();

        const CrsTexture *maps[4] = {&albedo, &normal, &roughness, &ao};
        VkDescriptorImageInfo infos[4]{};
        CrsDescriptorWriter writer{*materialSetLayout, *materialPool};
        for (uint32_t i = 0; i < 4; i++) {
            infos[i] = {maps[i]->getSampler(), maps[i]->getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            writer.writeImage(i, &infos[i]);
        }
        if (!writer.build(preview.materialSet)) throw std::runtime_error("pool da prévia de material cheio");

        createTarget(crsDevice, COLOR_FORMAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_IMAGE_ASPECT_COLOR_BIT, preview.color, preview.colorView);
        createTarget(crsDevice, depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                     VK_IMAGE_ASPECT_DEPTH_BIT, preview.depth, preview.depthView);

        //imagens novas: o conteúdo não importa, só ficam prontas pra receber o desenho
        imageBarrier(commandBuffer, preview.color.get(), VK_IMAGE_ASPECT_COLOR_BIT,
            VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        imageBarrier(commandBuffer, preview.depth.get(), VK_IMAGE_ASPECT_DEPTH_BIT,
            VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = preview.colorView.get();
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue.color = {{0.f, 0.f, 0.f, 0.f}};  //fundo transparente: aparece o card
        VkRenderingAttachmentInfo depthAttachment{};
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = preview.depthView.get();
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.clearValue.depthStencil = {1.f, 0};

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea = {{0, 0}, {SIZE, SIZE}};
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;
        renderingInfo.pDepthAttachment = &depthAttachment;
        vkCmdBeginRendering(commandBuffer, &renderingInfo);

        VkViewport viewport{0.f, 0.f, static_cast<float>(SIZE), static_cast<float>(SIZE), 0.f, 1.f};
        VkRect2D scissor{{0, 0}, {SIZE, SIZE}};
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        PreviewPushConstants push{};
        glm::mat4 projection = glm::perspective(glm::radians(36.f), 1.f, 0.1f, 10.f);
        projection[1][1] *= -1.f;  //o Vulkan tem o Y da tela pra baixo
        push.viewProjection = projection * glm::lookAt(CAMERA_POSITION, glm::vec3{0.f}, glm::vec3{0.f, 1.f, 0.f});
        push.cameraPosition = glm::vec4{CAMERA_POSITION, 1.f};

        pipeline->bind(commandBuffer);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(),
            0, 1, &preview.materialSet, 0, nullptr);
        vkCmdPushConstants(commandBuffer, pipelineLayout.get(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(push), &push);
        VkBuffer vertices = vertexBuffer->getBuffer();
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertices, &offset);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
        vkCmdEndRendering(commandBuffer);

        //o desenho termina antes do ImGui ler a miniatura (no mesmo frame, no passe da tela)
        imageBarrier(commandBuffer, preview.color.get(), VK_IMAGE_ASPECT_COLOR_BIT,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        preview.imguiSet = ImGui_ImplVulkan_AddTexture(sampler.get(), preview.colorView.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        preview.drawnFrame = frameCounter;
        preview.ready = true;
    }

    void MaterialPreviews::render(VkCommandBuffer commandBuffer) {
        frameCounter++;
        //o que foi usado só pra desenhar (os mapas 2K e o set deles) sai quando a GPU já terminou com certeza
        for (auto &kv : previews) {
            Preview &preview = kv.second;
            if (!preview.ready || preview.textures.empty() && preview.materialSet == VK_NULL_HANDLE) continue;
            if (frameCounter - preview.drawnFrame < RELEASE_AFTER_FRAMES) continue;
            if (preview.materialSet != VK_NULL_HANDLE) materialPool->freeDescriptor(preview.materialSet);
            preview.materialSet = VK_NULL_HANDLE;
            preview.textures.clear();
        }

        //no máximo uma miniatura nova por frame (carregar os mapas trava um pouquinho)
        if (pending.empty()) return;
        const std::string folder = pending.front();
        pending.erase(pending.begin());
        try {
            draw(commandBuffer, folder, previews[folder]);
        } catch (const std::exception &e) {
            //material com defeito: fica sem miniatura (o card mostra só o nome)
            std::cerr << "prévia do material " << folder << ": " << e.what() << std::endl;
        }
    }
}
