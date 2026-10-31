#include "engine/rendering/systems/terrain_render_system.hpp"

#include "engine/rendering/crs_material_files.hpp"
#include "engine/rendering/crs_upload_batch.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cassert>
#include <iostream>
#include <stdexcept>

namespace crsdk {
    //tem que bater com o terrain_push.glsl (96 bytes: cabe nos 128 que toda GPU garante)
    struct TerrainPushConstants {
        glm::mat4 modelMatrix{1.f};
        glm::vec4 brush{0.f};
        glm::vec4 options{0.f};
    };

    struct TerrainVertex {
        glm::vec3 position;
        glm::vec3 normal;
    };

    constexpr uint32_t TILE_VERTEX_COUNT = TERRAIN_TILE_SAMPLES * TERRAIN_TILE_SAMPLES;
    //malha que ninguém desenhou por esses frames é apagada (já saiu de todo frame em voo)
    constexpr uint64_t FORGET_AFTER_FRAMES = CrsSwapChain::MAX_FRAMES_IN_FLIGHT + 1;
    //quantos materiais diferentes de terreno cabem no pool (+1 do neutro)
    constexpr uint32_t MAX_TERRAIN_MATERIALS = 32;

    TerrainRenderSystem::TerrainRenderSystem(CrsDevice &device, VkFormat colorFormat, VkFormat depthFormat, VkDescriptorSetLayout globalSetLayout)
        : crsDevice{device} {
        //set 1: os quatro mapas do material (cor, normal, roughness, AO)
        materialSetLayout = CrsDescriptorSetLayout::Builder{crsDevice}
            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();
        materialPool = CrsDescriptorPool::Builder{crsDevice}
            .setMaxSets(MAX_TERRAIN_MATERIALS + 1)
            .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (MAX_TERRAIN_MATERIALS + 1) * 4)
            .build();
        const unsigned char white[4] = {255, 255, 255, 255};
        const unsigned char flat[4] = {128, 128, 255, 255};
        CrsUploadBatch upload{crsDevice};
        whiteTexture = std::make_unique<CrsTexture>(crsDevice, 1, 1, white, upload, TextureKind::Data);
        flatNormal = std::make_unique<CrsTexture>(crsDevice, 1, 1, flat, upload, TextureKind::Data);
        upload.submit();
        neutralSet = makeMaterialSet(*whiteTexture, *flatNormal, *whiteTexture, *whiteTexture);

        //set 0 global (câmera, luzes, sombra) + set 1 material; a push constant é lida pelos dois shaders
        pipelineLayout = CrsPipeline::createPipelineLayout(
            crsDevice, {globalSetLayout, materialSetLayout->getDescriptorSetLayout()},
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(TerrainPushConstants));
        createPipeline(colorFormat, depthFormat);
        createIndexBuffer();
    }

    void TerrainRenderSystem::createPipeline(VkFormat colorFormat, VkFormat depthFormat) {
        assert(pipelineLayout.get() != VK_NULL_HANDLE && "Cannot create pipeline before pipeline layout");
        PipelineConfigInfo config{};
        CrsPipeline::defaultPipelineConfigInfo(config);
        config.bindingDescriptions = {{0, sizeof(TerrainVertex), VK_VERTEX_INPUT_RATE_VERTEX}};
        config.attributeDescriptions = {
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(TerrainVertex, position)},
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(TerrainVertex, normal)},
        };
        config.colorAttachmentFormats = {colorFormat};
        config.depthAttachmentFormat = depthFormat;
        config.pipelineLayout = pipelineLayout.get();
        pipeline = std::make_unique<CrsPipeline>(crsDevice, "shaders/terrain.vert.spv", "shaders/terrain.frag.spv", config);
    }

    void TerrainRenderSystem::createIndexBuffer() {
        //dois triângulos por quadrado, cortados na diagonal (0,0)-(1,1): a mesma do heightAt e da Jolt
        std::vector<uint32_t> indices;
        indices.reserve(TERRAIN_TILE_CELLS * TERRAIN_TILE_CELLS * 6);
        for (uint32_t z = 0; z < TERRAIN_TILE_CELLS; z++) {
            for (uint32_t x = 0; x < TERRAIN_TILE_CELLS; x++) {
                const uint32_t i00 = z * TERRAIN_TILE_SAMPLES + x;
                const uint32_t i10 = i00 + 1;
                const uint32_t i01 = i00 + TERRAIN_TILE_SAMPLES;
                const uint32_t i11 = i01 + 1;
                indices.insert(indices.end(), {i00, i10, i11, i00, i11, i01});
            }
        }
        indexCount = static_cast<uint32_t>(indices.size());
        indexBuffer = std::make_unique<CrsBuffer>(crsDevice, sizeof(uint32_t), indexCount, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        indexBuffer->map();
        indexBuffer->writeToBuffer(indices.data());
        indexBuffer->unmap();
    }

    VkDescriptorSet TerrainRenderSystem::makeMaterialSet(const CrsTexture &albedo, const CrsTexture &normal,
                                                         const CrsTexture &roughness, const CrsTexture &ao) {
        const CrsTexture *maps[4] = {&albedo, &normal, &roughness, &ao};
        VkDescriptorImageInfo infos[4]{};
        CrsDescriptorWriter writer{*materialSetLayout, *materialPool};
        for (uint32_t i = 0; i < 4; i++) {
            infos[i] = {maps[i]->getSampler(), maps[i]->getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            writer.writeImage(i, &infos[i]);
        }
        VkDescriptorSet set = VK_NULL_HANDLE;
        if (!writer.build(set)) throw std::runtime_error("pool de materiais do terreno cheio");
        return set;
    }

    const TerrainRenderSystem::Material &TerrainRenderSystem::getMaterial(const std::string &folder) {
        auto found = materials.find(folder);
        if (found != materials.end()) return found->second;

        Material &material = materials[folder];
        const MaterialFiles files = findMaterialFiles(folder);
        if (files.albedo.empty() || materials.size() > MAX_TERRAIN_MATERIALS) return material;
        try {
            //cada mapa que existe vira textura; o que falta usa o neutro (cor é sRGB, o resto é dado)
            CrsUploadBatch upload{crsDevice};
            auto load = [&](const std::string &path, TextureKind kind) -> const CrsTexture * {
                if (path.empty()) return nullptr;
                material.textures.push_back(std::make_unique<CrsTexture>(crsDevice, path, upload, kind));
                return material.textures.back().get();
            };
            const CrsTexture *albedo = load(files.albedo, TextureKind::Color);
            const CrsTexture *normal = load(files.normal, TextureKind::Data);
            const CrsTexture *roughness = load(files.roughness, TextureKind::Data);
            const CrsTexture *ao = load(files.ao, TextureKind::Data);
            upload.submit();
            material.set = makeMaterialSet(*albedo, normal ? *normal : *flatNormal,
                roughness ? *roughness : *whiteTexture, ao ? *ao : *whiteTexture);
            material.loaded = true;
        } catch (const std::exception &e) {
            //arquivo com defeito: avisa uma vez e o terreno fica com a cor provisória
            std::cerr << "material do terreno " << folder << ": " << e.what() << std::endl;
            material.textures.clear();
        }
        return material;
    }

    void TerrainRenderSystem::writeTile(const TerrainData &data, int tileX, int tileZ, CrsBuffer &buffer) {
        std::vector<TerrainVertex> vertices(TILE_VERTEX_COUNT);
        for (int z = 0; z < TERRAIN_TILE_SAMPLES; z++) {
            for (int x = 0; x < TERRAIN_TILE_SAMPLES; x++) {
                //amostra no terreno inteiro: a normal da borda olha o pedaço vizinho (sem costura na luz)
                const int sampleX = tileX * TERRAIN_TILE_CELLS + x;
                const int sampleZ = tileZ * TERRAIN_TILE_CELLS + z;
                TerrainVertex &v = vertices[static_cast<size_t>(z * TERRAIN_TILE_SAMPLES + x)];
                v.position = {static_cast<float>(sampleX) * TERRAIN_CELL_SIZE, data.sample(sampleX, sampleZ),
                              static_cast<float>(sampleZ) * TERRAIN_CELL_SIZE};
                v.normal = data.normalAt(sampleX, sampleZ);
            }
        }
        buffer.writeToBuffer(vertices.data());
    }

    void TerrainRenderSystem::render(FrameInfo &frameInfo, const TerrainOverlay &overlay) {
        frameCounter++;
        const int frame = frameInfo.frameIndex;
        bool bound = false;

        for (auto &kv : frameInfo.crsScene.getGameObjects()) {
            CrsGameObject &obj = kv.second;
            if (!obj.terrain) continue;
            const TerrainData &data = *obj.terrain->data;

            TerrainMesh &mesh = meshes[&data];
            if (!mesh.data) {
                mesh.data = obj.terrain->data;
                mesh.tiles.resize(static_cast<size_t>(data.tilesPerSide() * data.tilesPerSide()));
            }
            mesh.lastUsedFrame = frameCounter;

            if (!bound) {
                pipeline->bind(frameInfo.commandBuffer);
                vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(),
                    0, 1, &frameInfo.globalDescriptorSet, 0, nullptr);
                vkCmdBindIndexBuffer(frameInfo.commandBuffer, indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
                bound = true;
            }

            //material deste terreno (ou o neutro, com a cor provisória)
            const Material *material = obj.terrain->material.empty() ? nullptr : &getMaterial(obj.terrain->material);
            const bool hasMaterial = material && material->loaded;
            VkDescriptorSet materialSet = hasMaterial ? material->set : neutralSet;
            vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(),
                1, 1, &materialSet, 0, nullptr);

            TerrainPushConstants push{};
            push.modelMatrix = glm::translate(glm::mat4{1.f}, obj.terrain->corner(obj.transform.translation));
            push.brush = glm::vec4{overlay.brushCenter, overlay.brushRadius};
            push.options.x = overlay.contourInterval;
            push.options.y = 1.f / std::max(obj.terrain->textureSize, 0.01f);
            push.options.z = hasMaterial ? 1.f : 0.f;
            vkCmdPushConstants(frameInfo.commandBuffer, pipelineLayout.get(),
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);

            for (int tileZ = 0; tileZ < data.tilesPerSide(); tileZ++) {
                for (int tileX = 0; tileX < data.tilesPerSide(); tileX++) {
                    TileMesh &tile = mesh.tiles[static_cast<size_t>(tileZ * data.tilesPerSide() + tileX)];
                    std::unique_ptr<CrsBuffer> &buffer = tile.vertices[frame];
                    if (!buffer) {
                        buffer = std::make_unique<CrsBuffer>(crsDevice, sizeof(TerrainVertex), TILE_VERTEX_COUNT,
                            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                        buffer->map();
                    }
                    //o buffer deste frame está atrasado (pedaço esculpido depois que ele foi escrito)
                    const uint64_t version = data.tile(tileX, tileZ).version;
                    if (!tile.written[frame] || tile.versions[frame] != version) {
                        writeTile(data, tileX, tileZ, *buffer);
                        tile.versions[frame] = version;
                        tile.written[frame] = true;
                    }

                    VkBuffer vertexBuffer = buffer->getBuffer();
                    VkDeviceSize offset = 0;
                    vkCmdBindVertexBuffers(frameInfo.commandBuffer, 0, 1, &vertexBuffer, &offset);
                    vkCmdDrawIndexed(frameInfo.commandBuffer, indexCount, 1, 0, 0, 0);
                }
            }
        }

        //terreno apagado da cena: a malha sai quando nenhum frame em voo usa mais ela
        std::erase_if(meshes, [&](auto &kv) { return frameCounter - kv.second.lastUsedFrame > FORGET_AFTER_FRAMES; });
    }
}
