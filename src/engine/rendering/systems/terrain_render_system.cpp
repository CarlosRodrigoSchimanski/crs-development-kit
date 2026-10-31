#include "engine/rendering/systems/terrain_render_system.hpp"

#include "engine/rendering/crs_material_files.hpp"
#include "engine/rendering/crs_upload_batch.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace crsdk {
    //tem que bater com o terrain_push.glsl (128 bytes: exatamente o que toda GPU garante)
    struct TerrainPushConstants {
        glm::mat4 modelMatrix{1.f};
        glm::vec4 brush{0.f};
        glm::vec4 options{0.f};
        glm::vec4 layerScales[2]{};  //1 / tamanho da textura de cada posição do quadradinho, em metros
    };
    static_assert(sizeof(TerrainPushConstants) <= 128, "push constant do terreno passou dos 128 bytes");

    struct TerrainVertex {
        glm::vec3 position;
        glm::vec3 normal;
        uint8_t weights[TERRAIN_CHUNK_LAYERS];  //quanto de cada material do quadradinho (0..255): o shader recebe 0..1
    };

    constexpr uint32_t CHUNKS = TERRAIN_CHUNKS_PER_TILE * TERRAIN_CHUNKS_PER_TILE;
    //cada quadradinho tem os próprios vértices (a borda repetida nos dois vizinhos): os pesos de um
    //vértice seguem a lista de materiais do quadradinho dele, que é diferente da do vizinho
    constexpr uint32_t CHUNK_VERTEX_COUNT = TERRAIN_CHUNK_SAMPLES * TERRAIN_CHUNK_SAMPLES;
    constexpr uint32_t TILE_VERTEX_COUNT = CHUNKS * CHUNK_VERTEX_COUNT;
    //malha que ninguém desenhou por esses frames é apagada (já saiu de todo frame em voo)
    constexpr uint64_t FORGET_AFTER_FRAMES = CrsSwapChain::MAX_FRAMES_IN_FLIGHT + 1;
    //material que nenhum quadradinho desenhou por esses frames sai da memória (~2 s a 60 fps: pintar e
    //apagar logo em seguida não fica carregando e descarregando)
    constexpr uint64_t MATERIAL_FORGET_FRAMES = 120;
    //quantas combinações diferentes de materiais cabem no pool ao mesmo tempo
    constexpr uint32_t MAX_LAYER_SETS = 512;
    //os mapas de cada posição, um binding por tipo, cada um um array de 8 (um por posição)
    constexpr uint32_t MAP_KINDS = 5;  //cor, normal, roughness, AO, displacement

    TerrainRenderSystem::TerrainRenderSystem(CrsDevice &device, VkFormat colorFormat, VkFormat depthFormat, VkDescriptorSetLayout globalSetLayout)
        : crsDevice{device} {
        //set 1: cinco arrays de 8 texturas (binding 0 = as 8 cores, 1 = os 8 normal maps...)
        CrsDescriptorSetLayout::Builder layout{crsDevice};
        for (uint32_t kind = 0; kind < MAP_KINDS; kind++) {
            layout.addBinding(kind, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, TERRAIN_CHUNK_LAYERS);
        }
        materialSetLayout = layout.build();
        //sets devolvidos um a um: combinação que ninguém usa mais sai, e libera o material dela
        materialPool = CrsDescriptorPool::Builder{crsDevice}
            .setMaxSets(MAX_LAYER_SETS)
            .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_LAYER_SETS * MAP_KINDS * TERRAIN_CHUNK_LAYERS)
            .allowFreeingSets()
            .build();

        const unsigned char white[4] = {255, 255, 255, 255};
        const unsigned char flat[4] = {128, 128, 255, 255};
        const unsigned char mid[4] = {128, 128, 128, 255};
        CrsUploadBatch upload{crsDevice};
        whiteColor = std::make_unique<CrsTexture>(crsDevice, 1, 1, white, upload, TextureKind::Color);
        whiteData = std::make_unique<CrsTexture>(crsDevice, 1, 1, white, upload, TextureKind::Data);
        flatNormal = std::make_unique<CrsTexture>(crsDevice, 1, 1, flat, upload, TextureKind::Data);
        midHeight = std::make_unique<CrsTexture>(crsDevice, 1, 1, mid, upload, TextureKind::Data);
        upload.submit();

        //set 0 global (câmera, luzes, sombra) + set 1 materiais; a push constant é lida pelos dois shaders
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
            //os 8 pesos em dois grupos de 4 bytes; UNORM: o shader recebe 0..1
            {2, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(TerrainVertex, weights)},
            {3, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(TerrainVertex, weights) + 4},
        };
        config.colorAttachmentFormats = {colorFormat};
        config.depthAttachmentFormat = depthFormat;
        config.pipelineLayout = pipelineLayout.get();
        pipeline = std::make_unique<CrsPipeline>(crsDevice, "shaders/terrain.vert.spv", "shaders/terrain.frag.spv", config);
    }

    void TerrainRenderSystem::createIndexBuffer() {
        //um quadradinho: dois triângulos por quadrado, cortados na diagonal (0,0)-(1,1), a mesma do heightAt
        //e da Jolt. Todo quadradinho usa estes índices, cada um a partir do primeiro vértice dele
        std::vector<uint32_t> indices;
        indices.reserve(TERRAIN_CHUNK_CELLS * TERRAIN_CHUNK_CELLS * 6);
        for (uint32_t z = 0; z < TERRAIN_CHUNK_CELLS; z++) {
            for (uint32_t x = 0; x < TERRAIN_CHUNK_CELLS; x++) {
                const uint32_t i00 = z * TERRAIN_CHUNK_SAMPLES + x;
                const uint32_t i10 = i00 + 1;
                const uint32_t i01 = i00 + TERRAIN_CHUNK_SAMPLES;
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

    TerrainRenderSystem::Material &TerrainRenderSystem::getMaterial(const std::string &folder) {
        auto found = materials.find(folder);
        if (found != materials.end()) return found->second;

        Material &material = materials[folder];
        const MaterialFiles files = findMaterialFiles(folder);
        if (files.albedo.empty()) return material;
        try {
            //cada mapa que existe vira textura; o que falta fica com o neutro (cor é sRGB, o resto é dado)
            CrsUploadBatch upload{crsDevice};
            auto load = [&](const std::string &path, TextureKind kind) -> const CrsTexture * {
                if (path.empty()) return nullptr;
                material.textures.push_back(std::make_unique<CrsTexture>(crsDevice, path, upload, kind));
                return material.textures.back().get();
            };
            material.albedo = load(files.albedo, TextureKind::Color);
            material.normal = load(files.normal, TextureKind::Data);
            material.roughness = load(files.roughness, TextureKind::Data);
            material.ao = load(files.ao, TextureKind::Data);
            material.displacement = load(files.displacement, TextureKind::Data);
            upload.submit();
            material.loaded = true;
        } catch (const std::exception &e) {
            //arquivo com defeito: avisa uma vez e a posição fica com os mapas neutros
            std::cerr << "material do terreno " << folder << ": " << e.what() << std::endl;
            material = Material{};
        }
        return material;
    }

    VkDescriptorSet TerrainRenderSystem::getLayerSet(const std::vector<std::string> &folders) {
        std::string key;
        for (const std::string &folder : folders) key += folder + '|';
        auto found = layerSets.find(key);
        if (found != layerSets.end()) {
            found->second.lastUsedFrame = frameCounter;
            for (const std::string &folder : folders) {
                if (!folder.empty()) getMaterial(folder).lastUsedFrame = frameCounter;
            }
            return found->second.set;
        }

        //posição sem material: os neutros (o peso dela é 0 de qualquer jeito)
        VkDescriptorImageInfo infos[MAP_KINDS][TERRAIN_CHUNK_LAYERS]{};
        for (int i = 0; i < TERRAIN_CHUNK_LAYERS; i++) {
            Material *material = i < static_cast<int>(folders.size()) && !folders[static_cast<size_t>(i)].empty()
                ? &getMaterial(folders[static_cast<size_t>(i)]) : nullptr;
            if (material) material->lastUsedFrame = frameCounter;
            const bool ok = material && material->loaded;
            const CrsTexture *maps[MAP_KINDS] = {
                ok ? material->albedo : whiteColor.get(),
                ok && material->normal ? material->normal : flatNormal.get(),
                ok && material->roughness ? material->roughness : whiteData.get(),
                ok && material->ao ? material->ao : whiteData.get(),
                ok && material->displacement ? material->displacement : midHeight.get(),
            };
            for (uint32_t kind = 0; kind < MAP_KINDS; kind++) {
                infos[kind][i] = {maps[kind]->getSampler(), maps[kind]->getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            }
        }
        CrsDescriptorWriter writer{*materialSetLayout, *materialPool};
        for (uint32_t kind = 0; kind < MAP_KINDS; kind++) writer.writeImages(kind, infos[kind], TERRAIN_CHUNK_LAYERS);
        VkDescriptorSet set = VK_NULL_HANDLE;
        if (!writer.build(set)) throw std::runtime_error("pool de materiais do terreno cheio");
        layerSets.emplace(key, LayerSet{set, folders, frameCounter});
        return set;
    }

    void TerrainRenderSystem::releaseUnused() {
        //combinação que nenhum quadradinho usou por um tempo: o set volta pro pool
        for (auto it = layerSets.begin(); it != layerSets.end();) {
            if (frameCounter - it->second.lastUsedFrame > MATERIAL_FORGET_FRAMES) {
                materialPool->freeDescriptor(it->second.set);
                it = layerSets.erase(it);
            } else {
                ++it;
            }
        }
        //material que nenhum quadradinho usou por um tempo (e nenhum set ainda aponta pra ele): sai da memória
        std::erase_if(materials, [&](auto &kv) {
            if (frameCounter - kv.second.lastUsedFrame <= MATERIAL_FORGET_FRAMES) return false;
            for (const auto &set : layerSets) {
                for (const std::string &folder : set.second.folders) {
                    if (folder == kv.first) return false;
                }
            }
            return true;
        });
    }

    size_t TerrainRenderSystem::loadedMaterialCount() const {
        size_t count = 0;
        for (const auto &kv : materials) {
            if (kv.second.loaded) count++;
        }
        return count;
    }

    void TerrainRenderSystem::writeTile(const TerrainData &data, int tileX, int tileZ, CrsBuffer &buffer, ChunkLayers &layers) {
        std::vector<TerrainVertex> vertices(TILE_VERTEX_COUNT);
        const TerrainTile &tile = data.tile(tileX, tileZ);
        for (int chunkZ = 0; chunkZ < TERRAIN_CHUNKS_PER_TILE; chunkZ++) {
            for (int chunkX = 0; chunkX < TERRAIN_CHUNKS_PER_TILE; chunkX++) {
                const size_t chunk = static_cast<size_t>(chunkZ * TERRAIN_CHUNKS_PER_TILE + chunkX);
                //os materiais deste quadradinho: a posição de cada um no shader é a ordem desta lista
                layers[chunk] = data.chunkLayers(tileX, tileZ, chunkX, chunkZ);
                std::array<int, 256> slotOf{};
                slotOf.fill(-1);
                for (size_t i = 0; i < layers[chunk].size(); i++) slotOf[layers[chunk][i]] = static_cast<int>(i);

                for (int z = 0; z < TERRAIN_CHUNK_SAMPLES; z++) {
                    for (int x = 0; x < TERRAIN_CHUNK_SAMPLES; x++) {
                        const int localX = chunkX * TERRAIN_CHUNK_CELLS + x;
                        const int localZ = chunkZ * TERRAIN_CHUNK_CELLS + z;
                        //amostra no terreno inteiro: a normal da borda olha o pedaço vizinho (sem costura na luz)
                        const int sampleX = tileX * TERRAIN_TILE_CELLS + localX;
                        const int sampleZ = tileZ * TERRAIN_TILE_CELLS + localZ;
                        const size_t index = static_cast<size_t>(localZ * TERRAIN_TILE_SAMPLES + localX);
                        TerrainVertex &v = vertices[chunk * CHUNK_VERTEX_COUNT + static_cast<size_t>(z * TERRAIN_CHUNK_SAMPLES + x)];
                        v.position = {static_cast<float>(sampleX) * TERRAIN_CELL_SIZE, tile.heights[index],
                                      static_cast<float>(sampleZ) * TERRAIN_CELL_SIZE};
                        v.normal = data.normalAt(sampleX, sampleZ);
                        std::fill(std::begin(v.weights), std::end(v.weights), uint8_t{0});
                        const LayerMix &m = tile.paint[index];
                        for (size_t i = 0; i < m.ids.size(); i++) {
                            const int slot = slotOf[m.ids[i]];
                            if (m.weights[i] <= 0.f || slot < 0) continue;  //material cortado (mais de 8 no quadradinho)
                            v.weights[slot] = static_cast<uint8_t>(std::lround(std::clamp(m.weights[i], 0.f, 1.f) * 255.f));
                        }
                    }
                }
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

            const std::vector<TerrainLayer> &palette = obj.terrain->layers;
            TerrainPushConstants push{};
            push.modelMatrix = glm::translate(glm::mat4{1.f}, obj.terrain->corner(obj.transform.translation));
            push.brush = glm::vec4{overlay.brushCenter, overlay.brushRadius};
            push.options.x = overlay.contourInterval;

            for (int tileZ = 0; tileZ < data.tilesPerSide(); tileZ++) {
                for (int tileX = 0; tileX < data.tilesPerSide(); tileX++) {
                    TileMesh &tile = mesh.tiles[static_cast<size_t>(tileZ * data.tilesPerSide() + tileX)];
                    std::unique_ptr<CrsBuffer> &buffer = tile.vertices[frame];
                    if (!buffer) {
                        buffer = std::make_unique<CrsBuffer>(crsDevice, sizeof(TerrainVertex), TILE_VERTEX_COUNT,
                            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                        buffer->map();
                    }
                    //o buffer deste frame está atrasado (pedaço esculpido/pintado depois que ele foi escrito)
                    const uint64_t version = data.tile(tileX, tileZ).version;
                    if (!tile.written[frame] || tile.versions[frame] != version) {
                        writeTile(data, tileX, tileZ, *buffer, tile.layers[frame]);
                        tile.versions[frame] = version;
                        tile.written[frame] = true;
                    }
                    VkBuffer vertexBuffer = buffer->getBuffer();
                    VkDeviceSize offset = 0;
                    vkCmdBindVertexBuffers(frameInfo.commandBuffer, 0, 1, &vertexBuffer, &offset);

                    for (uint32_t chunk = 0; chunk < CHUNKS; chunk++) {
                        //só os materiais deste quadradinho: é aqui que o que não aparece nele nem é pedido
                        const std::vector<uint8_t> &ids = tile.layers[frame][chunk];
                        std::vector<std::string> folders;
                        bool anyMaterial = false;
                        for (size_t i = 0; i < ids.size(); i++) {
                            const TerrainLayer *layer = ids[i] < palette.size() ? &palette[ids[i]] : nullptr;
                            folders.push_back(layer ? layer->material : std::string{});
                            push.layerScales[i / 4][i % 4] = 1.f / std::max(layer ? layer->textureSize : 4.f, 0.01f);
                            if (layer && !layer->material.empty() && getMaterial(layer->material).loaded) anyMaterial = true;
                        }
                        //nenhum material de verdade (terreno sem paleta): a cor provisória
                        push.options.y = anyMaterial ? static_cast<float>(ids.size()) : 0.f;
                        VkDescriptorSet layerSet = getLayerSet(folders);
                        vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(),
                            1, 1, &layerSet, 0, nullptr);
                        vkCmdPushConstants(frameInfo.commandBuffer, pipelineLayout.get(),
                            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
                        vkCmdDrawIndexed(frameInfo.commandBuffer, indexCount, 1, 0,
                            static_cast<int32_t>(chunk * CHUNK_VERTEX_COUNT), 0);
                    }
                }
            }
        }

        //terreno apagado da cena: a malha sai quando nenhum frame em voo usa mais ela
        std::erase_if(meshes, [&](auto &kv) { return frameCounter - kv.second.lastUsedFrame > FORGET_AFTER_FRAMES; });
        releaseUnused();
    }
}
