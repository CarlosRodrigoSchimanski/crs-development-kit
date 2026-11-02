#include "engine/rendering/systems/terrain_render_system.hpp"

#include "engine/rendering/crs_image_barrier.hpp"
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
        glm::vec2 splatUv;  //onde o vértice cai na imagem de pintura do pedaço (na região do quadradinho dele)
    };

    //cada quadradinho tem os próprios vértices (a borda repetida nos dois vizinhos): o splatUv de um
    //vértice aponta pra região do quadradinho dele na imagem de pintura, que é diferente da do vizinho
    constexpr uint32_t CHUNK_VERTEX_COUNT = TERRAIN_CHUNK_SAMPLES * TERRAIN_CHUNK_SAMPLES;
    constexpr uint32_t TILE_VERTEX_COUNT = TERRAIN_CHUNKS_PER_TILE * TERRAIN_CHUNKS_PER_TILE * CHUNK_VERTEX_COUNT;
    //malha que ninguém desenhou por esses frames é apagada (já saiu de todo frame em voo)
    constexpr uint64_t FORGET_AFTER_FRAMES = CrsSwapChain::MAX_FRAMES_IN_FLIGHT + 1;
    //material que nenhum quadradinho desenhou por esses frames sai da memória (~2 s a 60 fps: pintar e
    //apagar logo em seguida não fica carregando e descarregando)
    constexpr uint64_t MATERIAL_FORGET_FRAMES = 120;
    //quantas combinações diferentes de materiais cabem no pool ao mesmo tempo
    constexpr uint32_t MAX_LAYER_SETS = 512;
    //os mapas de cada posição, um binding por tipo, cada um um array de 8 (um por posição)
    constexpr uint32_t MAP_KINDS = 5;  //cor, normal, roughness, AO, displacement
    //a imagem de pintura de um pedaço: 8 x 8 regiões de 129 x 129 (uma por quadradinho, com a borda),
    //2 camadas RGBA (os 8 pesos)
    constexpr uint32_t SPLAT_REGION = TERRAIN_CHUNK_PAINT_SAMPLES;
    constexpr uint32_t SPLAT_SIZE = TERRAIN_CHUNKS_PER_TILE * SPLAT_REGION;
    constexpr uint32_t SPLAT_LAYERS = 2;
    constexpr VkDeviceSize SPLAT_LAYER_BYTES = SPLAT_REGION * SPLAT_REGION * 4;
    constexpr VkDeviceSize CHUNK_SPLAT_BYTES = SPLAT_LAYER_BYTES * SPLAT_LAYERS;  //133 KB
    //quantos quadradinhos dá pra enviar num frame (um pedaço inteiro: 8,5 MB); o resto vai no próximo
    constexpr uint32_t STAGING_CHUNKS = TERRAIN_CHUNKS_PER_TILE * TERRAIN_CHUNKS_PER_TILE;
    //pedaços com imagem de pintura ao mesmo tempo (um terreno de 8 x 8 + sobra pra trocar de cena)
    constexpr uint32_t MAX_SPLAT_SETS = 256;

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

        //set 2: a imagem de pintura do pedaço (com filtro: entre dois pontos de pintura o peso se mistura)
        splatSetLayout = CrsDescriptorSetLayout::Builder{crsDevice}
            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();
        splatPool = CrsDescriptorPool::Builder{crsDevice}
            .setMaxSets(MAX_SPLAT_SETS)
            .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_SPLAT_SETS)
            .allowFreeingSets()
            .build();
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.maxLod = 0.f;
        VkSampler sampler = VK_NULL_HANDLE;
        if (vkCreateSampler(crsDevice.device(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
            throw std::runtime_error("não deu pra criar o sampler da pintura do terreno");
        }
        splatSampler = CrsSampler{crsDevice.device(), sampler};

        //set 0 global (câmera, luzes, sombra) + set 1 materiais + set 2 pintura; a push constant é lida pelos dois shaders
        pipelineLayout = CrsPipeline::createPipelineLayout(
            crsDevice, {globalSetLayout, materialSetLayout->getDescriptorSetLayout(), splatSetLayout->getDescriptorSetLayout()},
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
            {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(TerrainVertex, splatUv)},
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

    void TerrainRenderSystem::writeTile(const TerrainData &data, int tileX, int tileZ, CrsBuffer &buffer) {
        std::vector<TerrainVertex> vertices(TILE_VERTEX_COUNT);
        const TerrainTile &tile = data.tile(tileX, tileZ);
        for (int chunkZ = 0; chunkZ < TERRAIN_CHUNKS_PER_TILE; chunkZ++) {
            for (int chunkX = 0; chunkX < TERRAIN_CHUNKS_PER_TILE; chunkX++) {
                const size_t chunk = static_cast<size_t>(chunkZ * TERRAIN_CHUNKS_PER_TILE + chunkX);
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
                        //o centro do ponto de pintura embaixo do vértice (4 por metro), dentro da região do
                        //quadradinho: entre os vértices a GPU anda pelos pontos do meio, sem sair da região
                        const float pixelX = static_cast<float>(chunkX * SPLAT_REGION + x * TERRAIN_PAINT_PER_METER) + 0.5f;
                        const float pixelZ = static_cast<float>(chunkZ * SPLAT_REGION + z * TERRAIN_PAINT_PER_METER) + 0.5f;
                        v.splatUv = glm::vec2{pixelX, pixelZ} / static_cast<float>(SPLAT_SIZE);
                    }
                }
            }
        }
        buffer.writeToBuffer(vertices.data());
    }

    TerrainRenderSystem::TerrainMesh &TerrainRenderSystem::meshFor(const std::shared_ptr<TerrainData> &data) {
        TerrainMesh &mesh = meshes[data.get()];
        if (!mesh.data) {
            mesh.data = data;
            mesh.tiles.resize(static_cast<size_t>(data->tilesPerSide() * data->tilesPerSide()));
        }
        return mesh;
    }

    void TerrainRenderSystem::createSplat(TileMesh &tile) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = {SPLAT_SIZE, SPLAT_SIZE, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = SPLAT_LAYERS;
        imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;  //peso 0..255 -> o shader recebe 0..1
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        tile.splat = crsDevice.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = tile.splat.get();
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        viewInfo.format = imageInfo.format;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, SPLAT_LAYERS};
        VkImageView view = VK_NULL_HANDLE;
        if (vkCreateImageView(crsDevice.device(), &viewInfo, nullptr, &view) != VK_SUCCESS) {
            throw std::runtime_error("não deu pra criar a imagem da pintura do terreno");
        }
        tile.splatView = CrsImageView{crsDevice.device(), view};

        VkDescriptorImageInfo info{splatSampler.get(), tile.splatView.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        CrsDescriptorWriter writer{*splatSetLayout, *splatPool};
        writer.writeImage(0, &info);
        if (!writer.build(tile.splatSet)) throw std::runtime_error("pool de pintura do terreno cheio");
    }

    void TerrainRenderSystem::releaseMesh(TerrainMesh &mesh) {
        for (TileMesh &tile : mesh.tiles) {
            if (tile.splatSet != VK_NULL_HANDLE) splatPool->freeDescriptor(tile.splatSet);
            tile.splatSet = VK_NULL_HANDLE;
        }
    }

    void TerrainRenderSystem::prepare(FrameInfo &frameInfo, const TerrainOverlay &overlay) {
        const int frame = frameInfo.frameIndex;
        const int maxLayers = overlay.editor ? TERRAIN_CHUNK_LAYERS : TERRAIN_CHUNK_LIMIT;
        std::unique_ptr<CrsBuffer> &stage = staging[static_cast<size_t>(frame)];
        uint32_t used = 0;  //quadradinhos já escritos no staging deste frame
        std::vector<uint8_t> region(CHUNK_SPLAT_BYTES);

        for (auto &kv : frameInfo.crsScene.getGameObjects()) {
            CrsGameObject &obj = kv.second;
            if (!obj.terrain) continue;
            const TerrainData &data = *obj.terrain->data;
            TerrainMesh &mesh = meshFor(obj.terrain->data);

            for (int tileZ = 0; tileZ < data.tilesPerSide(); tileZ++) {
                for (int tileX = 0; tileX < data.tilesPerSide(); tileX++) {
                    TileMesh &tile = mesh.tiles[static_cast<size_t>(tileZ * data.tilesPerSide() + tileX)];
                    const TerrainTile &source = data.tile(tileX, tileZ);
                    if (!tile.splat.get()) createSplat(tile);
                    //trocou editor/Play: a lista de cada quadradinho muda de tamanho, reenvia tudo
                    if (tile.maxLayers != maxLayers) {
                        tile.uploaded.fill(false);
                        tile.maxLayers = maxLayers;
                    }

                    std::vector<VkBufferImageCopy> copies;
                    for (int chunkZ = 0; chunkZ < TERRAIN_CHUNKS_PER_TILE; chunkZ++) {
                        for (int chunkX = 0; chunkX < TERRAIN_CHUNKS_PER_TILE; chunkX++) {
                            const size_t chunk = static_cast<size_t>(chunkZ * TERRAIN_CHUNKS_PER_TILE + chunkX);
                            if (tile.uploaded[chunk] && tile.chunkVersions[chunk] == source.chunkPaintVersions[chunk]) continue;
                            if (used >= STAGING_CHUNKS) continue;  //sem espaço neste frame: vai no próximo
                            if (!stage) {
                                stage = std::make_unique<CrsBuffer>(crsDevice, CHUNK_SPLAT_BYTES, STAGING_CHUNKS,
                                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                                stage->map();
                            }

                            //os materiais deste trecho (os que mais cobrem, até maxLayers): a posição de cada um no
                            //shader é a ordem desta lista; o que passou do limite fica de fora
                            std::vector<uint8_t> &layers = tile.layers[chunk];
                            layers = data.chunkLayers(tileX, tileZ, chunkX, chunkZ, maxLayers);
                            tile.overLimit[chunk] = data.chunkLayerCount(tileX, tileZ, chunkX, chunkZ) > TERRAIN_CHUNK_LIMIT;
                            std::array<int, 256> slotOf{};
                            slotOf.fill(-1);
                            for (size_t i = 0; i < layers.size(); i++) slotOf[layers[i]] = static_cast<int>(i);

                            //os pesos de cada ponto na posição do material dele: camada 0 = posições 0..3, camada 1 = 4..7
                            std::fill(region.begin(), region.end(), uint8_t{0});
                            for (uint32_t z = 0; z < SPLAT_REGION; z++) {
                                for (uint32_t x = 0; x < SPLAT_REGION; x++) {
                                    const size_t localX = static_cast<size_t>(chunkX * TERRAIN_CHUNK_PAINT_CELLS) + x;
                                    const size_t localZ = static_cast<size_t>(chunkZ * TERRAIN_CHUNK_PAINT_CELLS) + z;
                                    const PaintTexel &texel = source.paint[localZ * TERRAIN_PAINT_SAMPLES + localX];
                                    const size_t pixel = (z * SPLAT_REGION + x) * 4;
                                    for (size_t i = 0; i < texel.ids.size(); i++) {
                                        const int slot = slotOf[texel.ids[i]];
                                        if (texel.weights[i] == 0 || slot < 0) continue;  //ignorado (passou do limite do trecho)
                                        region[static_cast<size_t>(slot / 4) * SPLAT_LAYER_BYTES + pixel + static_cast<size_t>(slot % 4)] = texel.weights[i];
                                    }
                                }
                            }
                            const VkDeviceSize offset = used * CHUNK_SPLAT_BYTES;
                            stage->writeToBuffer(region.data(), CHUNK_SPLAT_BYTES, offset);
                            for (uint32_t layer = 0; layer < SPLAT_LAYERS; layer++) {
                                VkBufferImageCopy copy{};
                                copy.bufferOffset = offset + layer * SPLAT_LAYER_BYTES;
                                copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, layer, 1};
                                copy.imageOffset = {static_cast<int32_t>(chunkX * SPLAT_REGION), static_cast<int32_t>(chunkZ * SPLAT_REGION), 0};
                                copy.imageExtent = {SPLAT_REGION, SPLAT_REGION, 1};
                                copies.push_back(copy);
                            }
                            tile.chunkVersions[chunk] = source.chunkPaintVersions[chunk];
                            tile.uploaded[chunk] = true;
                            used++;
                        }
                    }
                    if (copies.empty()) continue;

                    //a cópia espera os frames anteriores pararem de ler a imagem (na primeira vez: nada pra esperar)
                    VkCommandBuffer cmd = frameInfo.commandBuffer;
                    imageBarrier(cmd, tile.splat.get(), VK_IMAGE_ASPECT_COLOR_BIT,
                        tile.splatReady ? VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_2_NONE,
                        tile.splatReady ? VK_ACCESS_2_SHADER_SAMPLED_READ_BIT : VK_ACCESS_2_NONE,
                        VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                        tile.splatReady ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
                    vkCmdCopyBufferToImage(cmd, stage->getBuffer(), tile.splat.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        static_cast<uint32_t>(copies.size()), copies.data());
                    imageBarrier(cmd, tile.splat.get(), VK_IMAGE_ASPECT_COLOR_BIT,
                        VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    tile.splatReady = true;
                }
            }
        }
    }

    void TerrainRenderSystem::render(FrameInfo &frameInfo, const TerrainOverlay &overlay) {
        frameCounter++;
        const int frame = frameInfo.frameIndex;
        bool bound = false;

        for (auto &kv : frameInfo.crsScene.getGameObjects()) {
            CrsGameObject &obj = kv.second;
            if (!obj.terrain) continue;
            const TerrainData &data = *obj.terrain->data;

            //sem prepare neste frame (terreno que acabou de aparecer): fica pro próximo
            auto found = meshes.find(&data);
            if (found == meshes.end()) continue;
            TerrainMesh &mesh = found->second;
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
                    if (!tile.splatReady) continue;  //a pintura ainda não chegou na GPU
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
                    vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(),
                        2, 1, &tile.splatSet, 0, nullptr);

                    for (uint32_t chunk = 0; chunk < CHUNKS; chunk++) {
                        if (!tile.uploaded[chunk]) continue;  //ainda na fila de envio
                        //só os materiais deste quadradinho: é aqui que o que não aparece nele nem é pedido
                        const std::vector<uint8_t> &ids = tile.layers[chunk];
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
                        //aura de alerta: só no editor, no trecho com mais de 4 materiais
                        push.options.z = overlay.editor && tile.overLimit[chunk] ? 1.f : 0.f;
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
        std::erase_if(meshes, [&](auto &kv) {
            if (frameCounter - kv.second.lastUsedFrame <= FORGET_AFTER_FRAMES) return false;
            releaseMesh(kv.second);
            return true;
        });
        releaseUnused();
    }
}
