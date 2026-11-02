#include "engine/terrain/crs_terrain.hpp"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <stdexcept>

namespace crsdk {

    //começo do arquivo de cada pedaço: confere que é mesmo um pedaço de terreno, e de que tamanho
    constexpr char TILE_MAGIC[4] = {'C', 'R', 'S', 'T'};
    //formatos do paint_x_z.bin (o 3º número do cabeçalho). Só o primeiro é gravado; os outros são de
    //cenas salvas antes (1 ponto por metro) e são lidos e convertidos
    constexpr uint32_t PAINT_FORMAT_RLE = 0x401;     //4 pontos por metro, comprimido (repetição, PaintTexel)
    constexpr uint32_t PAINT_FORMAT_PAIRS8 = 0x208;  //1 por metro: (índice, peso) x 8 por amostra
    constexpr uint32_t PAINT_FORMAT_PAIRS4 = 4;      //1 por metro: (índice, peso) x 4 por amostra
    constexpr uint32_t PAINT_FORMAT_DENSE8 = 8;      //1 por metro: 8 pesos fixos (a camada i = o material i)

    constexpr int CHUNKS = TERRAIN_CHUNKS_PER_TILE * TERRAIN_CHUNKS_PER_TILE;

    size_t TerrainSnapshot::bytes() const {
        size_t total = 0;
        for (const auto &h : heights) total += h.heights.size() * sizeof(float);
        for (const auto &p : paint) total += p.texels.size() * sizeof(PaintTexel);
        return total;
    }

    //pesos somando 1, sem resto de pincelada (abaixo do mínimo vira 0); nada sobrando = chão base
    static void normalizeMix(LayerMix &mix) {
        float sum = 0.f;
        for (float &w : mix.weights) {
            if (w < TERRAIN_MIN_WEIGHT) w = 0.f;
            sum += w;
        }
        if (sum <= 0.f) {
            mix = LayerMix{};
            return;
        }
        for (float &w : mix.weights) w /= sum;
    }

    LayerMix unpackTexel(const PaintTexel &texel) {
        LayerMix mix{};
        for (size_t i = 0; i < mix.ids.size(); i++) {
            mix.ids[i] = texel.ids[i];
            mix.weights[i] = static_cast<float>(texel.weights[i]) / 255.f;
        }
        return mix;
    }

    PaintTexel packTexel(const LayerMix &mix) {
        PaintTexel texel{};
        texel.weights[0] = 0;
        int total = 0;
        size_t largest = 0;
        for (size_t i = 0; i < mix.ids.size(); i++) {
            texel.ids[i] = mix.ids[i];
            const int value = static_cast<int>(std::lround(std::clamp(mix.weights[i], 0.f, 1.f) * 255.f));
            texel.weights[i] = static_cast<uint8_t>(value);
            total += value;
            if (mix.weights[i] > mix.weights[largest]) largest = i;
        }
        if (total == 0) return PaintTexel{};  //nada: chão base
        //o arredondamento pode sobrar ou faltar 1 ou 2: vai pro maior (a soma fica sempre 255)
        texel.weights[largest] = static_cast<uint8_t>(std::clamp(static_cast<int>(texel.weights[largest]) + 255 - total, 0, 255));
        return texel;
    }

    //a posição do material no ponto (-1 = não tem)
    static int findSlot(const LayerMix &mix, int layer) {
        for (int i = 0; i < TERRAIN_POINT_SLOTS; i++) {
            if (mix.weights[static_cast<size_t>(i)] > 0.f && mix.ids[static_cast<size_t>(i)] == layer) return i;
        }
        return -1;
    }

    static int findSlot(const PaintTexel &texel, int layer) {
        for (int i = 0; i < TERRAIN_POINT_SLOTS; i++) {
            if (texel.weights[static_cast<size_t>(i)] > 0 && texel.ids[static_cast<size_t>(i)] == layer) return i;
        }
        return -1;
    }

    //o índice de um ponto de pintura dentro do pedaço
    static size_t texelIndex(int localX, int localZ) {
        return static_cast<size_t>(localZ * TERRAIN_PAINT_SAMPLES + localX);
    }

    void TerrainTile::touchPaint(int localX, int localZ) {
        paintVersion++;
        const int last = TERRAIN_CHUNKS_PER_TILE - 1;
        for (int chunkZ = std::max((localZ - 1) / TERRAIN_CHUNK_PAINT_CELLS, 0); chunkZ <= std::min(localZ / TERRAIN_CHUNK_PAINT_CELLS, last); chunkZ++) {
            for (int chunkX = std::max((localX - 1) / TERRAIN_CHUNK_PAINT_CELLS, 0); chunkX <= std::min(localX / TERRAIN_CHUNK_PAINT_CELLS, last); chunkX++) {
                chunkPaintVersions[static_cast<size_t>(chunkZ * TERRAIN_CHUNKS_PER_TILE + chunkX)]++;
            }
        }
    }

    void TerrainTile::touchAllPaint() {
        paintVersion++;
        for (uint64_t &v : chunkPaintVersions) v++;
    }

    TerrainData::TerrainData(int tilesPerSide) : tiles{std::clamp(tilesPerSide, 1, TERRAIN_MAX_TILES)} {
        tileData.resize(static_cast<size_t>(tiles * tiles));
    }

    const TerrainTile &TerrainData::tile(int tileX, int tileZ) const {
        return tileData[static_cast<size_t>(tileZ * tiles + tileX)];
    }

    TerrainData TerrainData::resized(int newTilesPerSide) const {
        TerrainData result{newTilesPerSide};
        const int keep = std::min(tiles, result.tiles);
        for (int tileZ = 0; tileZ < keep; tileZ++) {
            for (int tileX = 0; tileX < keep; tileX++) {
                TerrainTile &target = result.tileData[static_cast<size_t>(tileZ * result.tiles + tileX)];
                target.heights = tile(tileX, tileZ).heights;
                target.paint = tile(tileX, tileZ).paint;
            }
        }
        //pedaço novo encostado num antigo: a borda compartilhada tem que ter a altura e a pintura do antigo
        //(só as duas linhas da divisa, não a grade inteira)
        const int heightEdge = keep * TERRAIN_TILE_CELLS;
        for (int i = 0; i <= heightEdge; i++) {
            result.setSample(heightEdge, i, sample(heightEdge, i));
            result.setSample(i, heightEdge, sample(i, heightEdge));
        }
        const int paintEdge = keep * TERRAIN_PAINT_CELLS;
        for (int i = 0; i <= paintEdge; i++) {
            result.setMix(paintEdge, i, mix(paintEdge, i));
            result.setMix(i, paintEdge, mix(i, paintEdge));
        }
        return result;
    }

    size_t TerrainData::sampleIndex(int &x, int &z, int &tileIndex) const {
        const int last = samplesPerSide() - 1;
        x = std::clamp(x, 0, last);
        z = std::clamp(z, 0, last);
        const int tileX = std::min(x / TERRAIN_TILE_CELLS, tiles - 1);
        const int tileZ = std::min(z / TERRAIN_TILE_CELLS, tiles - 1);
        tileIndex = tileZ * tiles + tileX;
        return static_cast<size_t>((z - tileZ * TERRAIN_TILE_CELLS) * TERRAIN_TILE_SAMPLES + (x - tileX * TERRAIN_TILE_CELLS));
    }

    size_t TerrainData::paintIndex(int &px, int &pz, int &tileIndex) const {
        const int last = paintSamplesPerSide() - 1;
        px = std::clamp(px, 0, last);
        pz = std::clamp(pz, 0, last);
        const int tileX = std::min(px / TERRAIN_PAINT_CELLS, tiles - 1);
        const int tileZ = std::min(pz / TERRAIN_PAINT_CELLS, tiles - 1);
        tileIndex = tileZ * tiles + tileX;
        return texelIndex(px - tileX * TERRAIN_PAINT_CELLS, pz - tileZ * TERRAIN_PAINT_CELLS);
    }

    template <typename Write>
    void TerrainData::forEachCopy(int x, int z, Write write) {
        const int last = samplesPerSide() - 1;
        if (x < 0 || z < 0 || x > last || z > last) return;
        //na divisa a amostra é a última de um pedaço e a primeira do outro: grava nos dois (ou 4 no canto)
        for (int tileZ = (z - 1) / TERRAIN_TILE_CELLS; tileZ <= z / TERRAIN_TILE_CELLS; tileZ++) {
            for (int tileX = (x - 1) / TERRAIN_TILE_CELLS; tileX <= x / TERRAIN_TILE_CELLS; tileX++) {
                if (tileX < 0 || tileZ < 0 || tileX >= tiles || tileZ >= tiles) continue;
                const int localX = x - tileX * TERRAIN_TILE_CELLS;
                const int localZ = z - tileZ * TERRAIN_TILE_CELLS;
                if (localX < 0 || localZ < 0 || localX > TERRAIN_TILE_CELLS || localZ > TERRAIN_TILE_CELLS) continue;
                const size_t tileIndex = static_cast<size_t>(tileZ * tiles + tileX);
                TerrainTile &t = tileData[tileIndex];
                //histórico: a primeira mudança das alturas do pedaço guarda como estavam
                if (journaling && !journaledTiles[tileIndex]) {
                    journaledTiles[tileIndex] = true;
                    journal.heights.push_back({static_cast<int>(tileIndex), t.heights});
                }
                write(t, static_cast<size_t>(localZ * TERRAIN_TILE_SAMPLES + localX));
                t.version++;
            }
        }
    }

    void TerrainData::journalPaint(size_t tileIndex, int localX, int localZ) {
        //todo trecho que contém o ponto: o da divisa está nos dois (ou 4) e cada um devolve a borda dele
        //no Ctrl+Z, então todos têm que ter guardado o valor de ANTES
        const int last = TERRAIN_CHUNKS_PER_TILE - 1;
        for (int chunkZ = std::max((localZ - 1) / TERRAIN_CHUNK_PAINT_CELLS, 0); chunkZ <= std::min(localZ / TERRAIN_CHUNK_PAINT_CELLS, last); chunkZ++) {
            for (int chunkX = std::max((localX - 1) / TERRAIN_CHUNK_PAINT_CELLS, 0); chunkX <= std::min(localX / TERRAIN_CHUNK_PAINT_CELLS, last); chunkX++) {
                const size_t key = tileIndex * CHUNKS + static_cast<size_t>(chunkZ * TERRAIN_CHUNKS_PER_TILE + chunkX);
                if (journaledChunks[key]) continue;
                journaledChunks[key] = true;
                PaintChunkSnapshot snapshot{static_cast<int>(tileIndex), chunkX, chunkZ, {}};
                snapshot.texels.reserve(TERRAIN_CHUNK_PAINT_SAMPLES * TERRAIN_CHUNK_PAINT_SAMPLES);
                const TerrainTile &t = tileData[tileIndex];
                for (int z = 0; z < TERRAIN_CHUNK_PAINT_SAMPLES; z++) {
                    for (int x = 0; x < TERRAIN_CHUNK_PAINT_SAMPLES; x++) {
                        snapshot.texels.push_back(t.paint[texelIndex(chunkX * TERRAIN_CHUNK_PAINT_CELLS + x, chunkZ * TERRAIN_CHUNK_PAINT_CELLS + z)]);
                    }
                }
                journal.paint.push_back(std::move(snapshot));
            }
        }
    }

    template <typename Write>
    void TerrainData::forEachPaintCopy(int px, int pz, Write write) {
        const int last = paintSamplesPerSide() - 1;
        if (px < 0 || pz < 0 || px > last || pz > last) return;
        //mesma regra das alturas: na divisa dos pedaços o ponto está nos dois (ou nos 4 do canto)
        for (int tileZ = (pz - 1) / TERRAIN_PAINT_CELLS; tileZ <= pz / TERRAIN_PAINT_CELLS; tileZ++) {
            for (int tileX = (px - 1) / TERRAIN_PAINT_CELLS; tileX <= px / TERRAIN_PAINT_CELLS; tileX++) {
                if (tileX < 0 || tileZ < 0 || tileX >= tiles || tileZ >= tiles) continue;
                const int localX = px - tileX * TERRAIN_PAINT_CELLS;
                const int localZ = pz - tileZ * TERRAIN_PAINT_CELLS;
                if (localX < 0 || localZ < 0 || localX > TERRAIN_PAINT_CELLS || localZ > TERRAIN_PAINT_CELLS) continue;
                const size_t tileIndex = static_cast<size_t>(tileZ * tiles + tileX);
                if (journaling) journalPaint(tileIndex, localX, localZ);
                TerrainTile &t = tileData[tileIndex];
                write(t, texelIndex(localX, localZ));
                t.touchPaint(localX, localZ);
            }
        }
    }

    float TerrainData::sample(int x, int z) const {
        int tileIndex = 0;
        const size_t index = sampleIndex(x, z, tileIndex);
        return tileData[static_cast<size_t>(tileIndex)].heights[index];
    }

    void TerrainData::setSample(int x, int z, float height) {
        forEachCopy(x, z, [&](TerrainTile &t, size_t index) { t.heights[index] = height; });
    }

    LayerMix TerrainData::mix(int px, int pz) const {
        int tileIndex = 0;
        const size_t index = paintIndex(px, pz, tileIndex);
        return unpackTexel(tileData[static_cast<size_t>(tileIndex)].paint[index]);
    }

    void TerrainData::setMix(int px, int pz, const LayerMix &value) {
        const PaintTexel texel = packTexel(value);
        forEachPaintCopy(px, pz, [&](TerrainTile &t, size_t index) { t.paint[index] = texel; });
    }

    float TerrainData::weight(int px, int pz, int layer) const {
        const LayerMix m = mix(px, pz);
        const int slot = findSlot(m, layer);
        return slot < 0 ? 0.f : m.weights[static_cast<size_t>(slot)];
    }

    glm::vec3 TerrainData::normalAt(int x, int z) const {
        //inclinação pelos vizinhos dos dois lados (diferença central)
        const float dx = sample(x + 1, z) - sample(x - 1, z);
        const float dz = sample(x, z + 1) - sample(x, z - 1);
        return glm::normalize(glm::vec3{-dx, 2.f * TERRAIN_CELL_SIZE, -dz});
    }

    float TerrainData::heightAt(float x, float z) const {
        const float gridX = std::clamp(x / TERRAIN_CELL_SIZE, 0.f, static_cast<float>(samplesPerSide() - 1));
        const float gridZ = std::clamp(z / TERRAIN_CELL_SIZE, 0.f, static_cast<float>(samplesPerSide() - 1));
        const int cellX = std::min(static_cast<int>(gridX), samplesPerSide() - 2);
        const int cellZ = std::min(static_cast<int>(gridZ), samplesPerSide() - 2);
        const float fx = gridX - static_cast<float>(cellX);
        const float fz = gridZ - static_cast<float>(cellZ);
        const float h00 = sample(cellX, cellZ);
        const float h10 = sample(cellX + 1, cellZ);
        const float h01 = sample(cellX, cellZ + 1);
        const float h11 = sample(cellX + 1, cellZ + 1);
        //cada quadrado são dois triângulos cortados na diagonal (0,0)-(1,1), como na malha e na Jolt
        if (fx >= fz) return h00 + fx * (h10 - h00) + fz * (h11 - h10);
        return h00 + fz * (h01 - h00) + fx * (h11 - h01);
    }

    std::optional<float> TerrainData::raycast(const glm::vec3 &origin, const glm::vec3 &direction, float maxDistance) const {
        const glm::vec3 dir = glm::normalize(direction);
        auto above = [&](float t) {
            const glm::vec3 p = origin + dir * t;
            return p.y >= heightAt(p.x, p.z);
        };
        auto inside = [&](float t) {
            const glm::vec3 p = origin + dir * t;
            return p.x >= 0.f && p.z >= 0.f && p.x <= size() && p.z <= size();
        };
        if (!above(0.f)) return std::nullopt;  //começou embaixo do chão

        //anda em passos de meio quadrado até passar pra baixo do chão; depois acha o ponto exato pela metade
        const float step = TERRAIN_CELL_SIZE * 0.5f;
        for (float t = step; t <= maxDistance + step; t += step) {
            const float end = std::min(t, maxDistance);
            if (above(end)) continue;
            float low = end - step, high = end;
            for (int i = 0; i < 16; i++) {
                const float mid = (low + high) * 0.5f;
                if (above(mid)) low = mid; else high = mid;
            }
            if (inside(high)) return high;
        }
        return std::nullopt;
    }

    //1 no meio, 0 na borda, sem degrau (meia volta de cosseno); distance = 0..1 do raio
    static float brushFalloff(float distance) {
        return 0.5f * (1.f + std::cos(glm::pi<float>() * distance));
    }

    template <typename Change>
    void TerrainData::applyBrush(glm::vec2 center, float radius, Change change) {
        if (radius <= 0.f) return;
        const int minX = static_cast<int>(std::floor((center.x - radius) / TERRAIN_CELL_SIZE));
        const int maxX = static_cast<int>(std::ceil((center.x + radius) / TERRAIN_CELL_SIZE));
        const int minZ = static_cast<int>(std::floor((center.y - radius) / TERRAIN_CELL_SIZE));
        const int maxZ = static_cast<int>(std::ceil((center.y + radius) / TERRAIN_CELL_SIZE));

        //calcula tudo antes de gravar: o suavizar lê os vizinhos, que não podem já estar mudados
        struct Result { int x, z; float height; };
        std::vector<Result> results;
        for (int z = std::max(minZ, 0); z <= std::min(maxZ, samplesPerSide() - 1); z++) {
            for (int x = std::max(minX, 0); x <= std::min(maxX, samplesPerSide() - 1); x++) {
                const glm::vec2 point{static_cast<float>(x) * TERRAIN_CELL_SIZE, static_cast<float>(z) * TERRAIN_CELL_SIZE};
                const float distance = glm::length(point - center) / radius;
                if (distance >= 1.f) continue;
                results.push_back({x, z, change(x, z, sample(x, z), brushFalloff(distance))});
            }
        }
        for (const Result &r : results) setSample(r.x, r.z, r.height);
    }

    void TerrainData::raise(glm::vec2 center, float radius, float amount) {
        applyBrush(center, radius, [&](int, int, float height, float weight) { return height + amount * weight; });
    }

    void TerrainData::smooth(glm::vec2 center, float radius, float strength) {
        strength = std::clamp(strength, 0.f, 1.f);
        applyBrush(center, radius, [&](int x, int z, float height, float weight) {
            //média com os 8 vizinhos
            float sum = 0.f;
            for (int dz = -1; dz <= 1; dz++) {
                for (int dx = -1; dx <= 1; dx++) sum += sample(x + dx, z + dz);
            }
            return height + (sum / 9.f - height) * strength * weight;
        });
    }

    void TerrainData::flatten(glm::vec2 center, float radius, float height, float strength) {
        strength = std::clamp(strength, 0.f, 1.f);
        applyBrush(center, radius, [&](int, int, float current, float weight) {
            return current + (height - current) * strength * weight;
        });
    }

    //uma pincelada num ponto: o material ganha (ou perde) 'delta'; os outros se ajustam pra somar 1
    static LayerMix paintMix(LayerMix m, int layer, float delta) {
        int slot = findSlot(m, layer);
        if (delta > 0.f && slot < 0) {
            //material novo no ponto: numa posição vazia (só sem nenhuma, no lugar do mais fraco)
            for (int i = 0; i < TERRAIN_POINT_SLOTS && slot < 0; i++) {
                if (m.weights[static_cast<size_t>(i)] <= 0.f) slot = i;
            }
            if (slot < 0) {
                slot = static_cast<int>(std::min_element(m.weights.begin(), m.weights.end()) - m.weights.begin());
            }
            m.ids[static_cast<size_t>(slot)] = static_cast<uint8_t>(layer);
            m.weights[static_cast<size_t>(slot)] = 0.f;
        }
        if (slot < 0) return m;  //apagar um material que o ponto não tem

        const size_t s = static_cast<size_t>(slot);
        const float before = m.weights[s];
        const float after = std::clamp(before + delta, 0.f, 1.f);
        float others = 0.f;
        for (size_t i = 0; i < m.weights.size(); i++) {
            if (i != s) others += m.weights[i];
        }
        if (others > 1e-4f) {
            //os outros encolhem/crescem juntos, na mesma proporção que tinham entre si
            const float scale = (1.f - after) / others;
            for (size_t i = 0; i < m.weights.size(); i++) {
                if (i != s) m.weights[i] *= scale;
            }
        } else if (after < 1.f) {
            if (layer == 0) return m;  //apagar o chão base onde só tem ele: não tem pra onde ir
            //só tinha ele: o que sai vira chão base, numa posição vazia
            for (size_t i = 0; i < m.weights.size(); i++) {
                if (i != s) {
                    m.ids[i] = 0;
                    m.weights[i] = 1.f - after;
                    break;
                }
            }
        }
        m.weights[s] = after;
        normalizeMix(m);
        return m;
    }

    //um número "aleatório" de 0 a 1 fixo pra cada ponto e seed (o mesmo ponto sempre dá o mesmo)
    static float pointNoise(int x, int z, uint32_t seed) {
        uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(z) * 668265263u + seed * 2246822519u;
        h = (h ^ (h >> 13)) * 1274126177u;
        h ^= h >> 16;
        return static_cast<float>(h & 0xFFFFFFu) / static_cast<float>(0x1000000u);
    }

    void TerrainData::paint(glm::vec2 center, float radius, int layer, float amount, float spray, uint32_t seed) {
        if (layer < 0 || layer >= TERRAIN_MAX_PALETTE || radius <= 0.f) return;
        spray = std::clamp(spray, 0.f, 1.f);
        //os pontos de pintura dentro do quadrado do círculo (4 por metro)
        const int minX = std::max(static_cast<int>(std::floor((center.x - radius) / TERRAIN_PAINT_SPACING)), 0);
        const int maxX = std::min(static_cast<int>(std::ceil((center.x + radius) / TERRAIN_PAINT_SPACING)), paintSamplesPerSide() - 1);
        const int minZ = std::max(static_cast<int>(std::floor((center.y - radius) / TERRAIN_PAINT_SPACING)), 0);
        const int maxZ = std::min(static_cast<int>(std::ceil((center.y + radius) / TERRAIN_PAINT_SPACING)), paintSamplesPerSide() - 1);

        struct Result { int x, z; PaintTexel texel; };
        std::vector<Result> results;
        for (int pz = minZ; pz <= maxZ; pz++) {
            for (int px = minX; px <= maxX; px++) {
                const glm::vec2 point{static_cast<float>(px) * TERRAIN_PAINT_SPACING, static_cast<float>(pz) * TERRAIN_PAINT_SPACING};
                const float distance = glm::length(point - center) / radius;
                if (distance >= 1.f) continue;
                float brush = brushFalloff(distance);
                //spray: o ponto entra (força cheia) se o sorteio dele cair abaixo do peso do pincel ali —
                //no meio quase todos entram, na borda quase nenhum; misturado com o pincel suave pelo spray
                if (spray > 0.f) {
                    const float hit = pointNoise(px, pz, seed) < brush ? 1.f : 0.f;
                    brush = brush + (hit - brush) * spray;
                }
                if (brush <= 0.f) continue;
                int x = px, z = pz, tileIndex = 0;
                const size_t index = paintIndex(x, z, tileIndex);
                const PaintTexel &before = tileData[static_cast<size_t>(tileIndex)].paint[index];
                PaintTexel after;
                if (journaling) {
                    //pincelada do editor: soma no acumulado do ponto e refaz a partir de como ele estava
                    const uint64_t key = static_cast<uint64_t>(pz) * static_cast<uint64_t>(paintSamplesPerSide()) + static_cast<uint64_t>(px);
                    StrokePoint &stroke = strokePoints.try_emplace(key, StrokePoint{before, 0.f}).first->second;
                    stroke.amount = std::clamp(stroke.amount + amount * brush, -1.f, 1.f);
                    after = packTexel(paintMix(unpackTexel(stroke.original), layer, stroke.amount));
                } else {
                    after = packTexel(paintMix(unpackTexel(before), layer, amount * brush));
                }
                if (!(after == before)) results.push_back({px, pz, after});
            }
        }
        for (const Result &r : results) {
            forEachPaintCopy(r.x, r.z, [&](TerrainTile &t, size_t index) { t.paint[index] = r.texel; });
        }
    }

    void TerrainData::removeLayer(int layer) {
        if (layer <= 0 || layer >= TERRAIN_MAX_PALETTE) return;  //o chão base não sai
        for (TerrainTile &t : tileData) {
            for (PaintTexel &texel : t.paint) {
                for (size_t i = 0; i < texel.ids.size(); i++) {
                    if (texel.weights[i] == 0) continue;
                    if (texel.ids[i] == layer) {
                        //o que era dele vai pro chão base (somado, se o ponto já tinha chão base)
                        const int base = findSlot(texel, 0);
                        if (base >= 0) {
                            texel.weights[static_cast<size_t>(base)] = static_cast<uint8_t>(texel.weights[static_cast<size_t>(base)] + texel.weights[i]);
                            texel.weights[i] = 0;
                        } else {
                            texel.ids[i] = 0;
                        }
                    } else if (texel.ids[i] > layer) {
                        texel.ids[i]--;
                    }
                }
            }
            t.touchAllPaint();
        }
    }

    void TerrainData::mergeIntoBase(int layer) {
        if (layer <= 0 || layer >= TERRAIN_MAX_PALETTE) return;
        for (size_t tileIndex = 0; tileIndex < tileData.size(); tileIndex++) {
            TerrainTile &t = tileData[tileIndex];
            bool changed = false;
            for (int z = 0; z < TERRAIN_PAINT_SAMPLES; z++) {
                for (int x = 0; x < TERRAIN_PAINT_SAMPLES; x++) {
                    PaintTexel &texel = t.paint[texelIndex(x, z)];
                    const int slot = findSlot(texel, layer);
                    if (slot < 0) continue;
                    if (journaling) journalPaint(tileIndex, x, z);
                    changed = true;
                    const int base = findSlot(texel, 0);
                    if (base >= 0) {
                        texel.weights[static_cast<size_t>(base)] = static_cast<uint8_t>(texel.weights[static_cast<size_t>(base)] + texel.weights[static_cast<size_t>(slot)]);
                        texel.weights[static_cast<size_t>(slot)] = 0;
                    } else {
                        texel.ids[static_cast<size_t>(slot)] = 0;
                    }
                }
            }
            if (changed) t.touchAllPaint();
        }
    }

    void TerrainData::beginJournal() {
        journaling = true;
        journal = TerrainSnapshot{};
        journaledTiles.assign(tileData.size(), false);
        journaledChunks.assign(tileData.size() * CHUNKS, false);
        strokePoints.clear();
    }

    TerrainSnapshot TerrainData::endJournal() {
        journaling = false;
        journaledTiles.clear();
        journaledChunks.clear();
        strokePoints = {};  //solta a memória da pincelada
        TerrainSnapshot result = std::move(journal);
        journal = TerrainSnapshot{};
        return result;
    }

    TerrainSnapshot TerrainData::snapshotLike(const TerrainSnapshot &which) const {
        TerrainSnapshot result;
        for (const TileHeightsSnapshot &h : which.heights) {
            if (h.tile >= 0 && static_cast<size_t>(h.tile) < tileData.size()) {
                result.heights.push_back({h.tile, tileData[static_cast<size_t>(h.tile)].heights});
            }
        }
        for (const PaintChunkSnapshot &p : which.paint) {
            if (p.tile < 0 || static_cast<size_t>(p.tile) >= tileData.size()) continue;
            PaintChunkSnapshot copy{p.tile, p.chunkX, p.chunkZ, {}};
            const TerrainTile &t = tileData[static_cast<size_t>(p.tile)];
            copy.texels.reserve(p.texels.size());
            for (int z = 0; z < TERRAIN_CHUNK_PAINT_SAMPLES; z++) {
                for (int x = 0; x < TERRAIN_CHUNK_PAINT_SAMPLES; x++) {
                    copy.texels.push_back(t.paint[texelIndex(p.chunkX * TERRAIN_CHUNK_PAINT_CELLS + x, p.chunkZ * TERRAIN_CHUNK_PAINT_CELLS + z)]);
                }
            }
            result.paint.push_back(std::move(copy));
        }
        return result;
    }

    void TerrainData::restore(const TerrainSnapshot &snapshot) {
        for (const TileHeightsSnapshot &h : snapshot.heights) {
            if (h.tile < 0 || static_cast<size_t>(h.tile) >= tileData.size()) continue;
            TerrainTile &t = tileData[static_cast<size_t>(h.tile)];
            t.heights = h.heights;
            t.version++;  //nunca volta pra trás: a malha e a física têm que ver que mudou
        }
        for (const PaintChunkSnapshot &p : snapshot.paint) {
            if (p.tile < 0 || static_cast<size_t>(p.tile) >= tileData.size()) continue;
            TerrainTile &t = tileData[static_cast<size_t>(p.tile)];
            size_t i = 0;
            for (int z = 0; z < TERRAIN_CHUNK_PAINT_SAMPLES; z++) {
                for (int x = 0; x < TERRAIN_CHUNK_PAINT_SAMPLES; x++) {
                    t.paint[texelIndex(p.chunkX * TERRAIN_CHUNK_PAINT_CELLS + x, p.chunkZ * TERRAIN_CHUNK_PAINT_CELLS + z)] = p.texels[i++];
                }
            }
            //a borda do trecho também é dos vizinhos: os 4 cantos cobrem ele e os 8 em volta
            const int x0 = p.chunkX * TERRAIN_CHUNK_PAINT_CELLS, z0 = p.chunkZ * TERRAIN_CHUNK_PAINT_CELLS;
            t.touchPaint(x0, z0);
            t.touchPaint(x0 + TERRAIN_CHUNK_PAINT_CELLS, z0);
            t.touchPaint(x0, z0 + TERRAIN_CHUNK_PAINT_CELLS);
            t.touchPaint(x0 + TERRAIN_CHUNK_PAINT_CELLS, z0 + TERRAIN_CHUNK_PAINT_CELLS);
        }
    }

    std::vector<float> TerrainData::layerUsage(int paletteSize) const {
        std::vector<double> sums(static_cast<size_t>(std::max(paletteSize, 1)), 0.0);
        for (const TerrainTile &t : tileData) {
            for (const PaintTexel &texel : t.paint) {
                for (size_t i = 0; i < texel.ids.size(); i++) {
                    if (texel.weights[i] > 0 && texel.ids[i] < sums.size()) sums[texel.ids[i]] += texel.weights[i];
                }
            }
        }
        const double total = std::accumulate(sums.begin(), sums.end(), 0.0);
        std::vector<float> usage(sums.size(), 0.f);
        for (size_t i = 0; i < sums.size(); i++) usage[i] = total > 0.0 ? static_cast<float>(sums[i] / total) : 0.f;
        return usage;
    }

    //quanto cada material cobre num trecho (soma dos pesos dos pontos de pintura dele, com a borda)
    static std::array<float, 256> chunkSums(const TerrainTile &t, int chunkX, int chunkZ) {
        std::array<uint32_t, 256> sums{};
        for (int z = 0; z < TERRAIN_CHUNK_PAINT_SAMPLES; z++) {
            for (int x = 0; x < TERRAIN_CHUNK_PAINT_SAMPLES; x++) {
                const PaintTexel &texel = t.paint[texelIndex(chunkX * TERRAIN_CHUNK_PAINT_CELLS + x, chunkZ * TERRAIN_CHUNK_PAINT_CELLS + z)];
                for (size_t i = 0; i < texel.ids.size(); i++) sums[texel.ids[i]] += texel.weights[i];
            }
        }
        std::array<float, 256> result{};
        for (size_t i = 0; i < result.size(); i++) result[i] = static_cast<float>(sums[i]) / 255.f;
        return result;
    }

    int TerrainData::chunkLayerCount(int tileX, int tileZ, int chunkX, int chunkZ) const {
        const std::array<float, 256> sums = chunkSums(tile(tileX, tileZ), chunkX, chunkZ);
        return static_cast<int>(std::count_if(sums.begin(), sums.end(), [](float s) { return s > 0.f; }));
    }

    std::vector<LayerShare> TerrainData::brushLayers(glm::vec2 center, float radius) const {
        //os pontos de pintura dentro do círculo, todos valendo igual (é uma contagem, não o pincel suave)
        std::array<double, 256> sums{};
        double total = 0.0;
        const int minX = std::max(static_cast<int>(std::floor((center.x - radius) / TERRAIN_PAINT_SPACING)), 0);
        const int maxX = std::min(static_cast<int>(std::ceil((center.x + radius) / TERRAIN_PAINT_SPACING)), paintSamplesPerSide() - 1);
        const int minZ = std::max(static_cast<int>(std::floor((center.y - radius) / TERRAIN_PAINT_SPACING)), 0);
        const int maxZ = std::min(static_cast<int>(std::ceil((center.y + radius) / TERRAIN_PAINT_SPACING)), paintSamplesPerSide() - 1);
        for (int pz = minZ; pz <= maxZ; pz++) {
            for (int px = minX; px <= maxX; px++) {
                const glm::vec2 point{static_cast<float>(px) * TERRAIN_PAINT_SPACING, static_cast<float>(pz) * TERRAIN_PAINT_SPACING};
                if (glm::length(point - center) > radius) continue;
                const LayerMix m = mix(px, pz);
                for (size_t i = 0; i < m.ids.size(); i++) {
                    if (m.weights[i] > 0.f) sums[m.ids[i]] += m.weights[i];
                }
                total += 1.0;
            }
        }
        std::vector<LayerShare> result;
        for (int i = 0; i < 256; i++) {
            if (sums[static_cast<size_t>(i)] > 0.0) result.push_back({i, static_cast<float>(sums[static_cast<size_t>(i)] / total)});
        }
        std::sort(result.begin(), result.end(), [](const LayerShare &a, const LayerShare &b) { return a.share > b.share; });
        return result;
    }

    std::vector<uint8_t> TerrainData::chunkLayers(int tileX, int tileZ, int chunkX, int chunkZ, int maxCount) const {
        const std::array<float, 256> sums = chunkSums(tile(tileX, tileZ), chunkX, chunkZ);
        std::vector<uint8_t> layers;
        for (int i = 0; i < 256; i++) {
            if (sums[static_cast<size_t>(i)] > 0.f) layers.push_back(static_cast<uint8_t>(i));
        }
        std::sort(layers.begin(), layers.end(), [&](uint8_t a, uint8_t b) { return sums[a] > sums[b]; });
        maxCount = std::clamp(maxCount, 1, TERRAIN_CHUNK_LAYERS);
        if (layers.size() > static_cast<size_t>(maxCount)) layers.resize(static_cast<size_t>(maxCount));
        if (layers.empty()) layers.push_back(0);
        return layers;
    }

    static std::filesystem::path tilePath(const std::string &folder, int tileX, int tileZ) {
        return std::filesystem::path{folder} / ("tile_" + std::to_string(tileX) + "_" + std::to_string(tileZ) + ".bin");
    }

    static std::filesystem::path paintPath(const std::string &folder, int tileX, int tileZ) {
        return std::filesystem::path{folder} / ("paint_" + std::to_string(tileX) + "_" + std::to_string(tileZ) + ".bin");
    }

    void TerrainData::save(const std::string &folder) const {
        std::filesystem::create_directories(folder);
        //pedaços de quando o terreno era maior não podem voltar ao carregar
        std::vector<std::filesystem::path> old;  //juntados antes: apagar no meio da listagem não é seguro
        for (const auto &entry : std::filesystem::directory_iterator{folder}) {
            const std::string name = entry.path().filename().string();
            const bool tileFile = name.rfind("tile_", 0) == 0 || name.rfind("paint_", 0) == 0;
            if (tileFile && entry.path().extension() == ".bin") old.push_back(entry.path());
        }
        for (const auto &path : old) std::filesystem::remove(path);

        for (int tileZ = 0; tileZ < tiles; tileZ++) {
            for (int tileX = 0; tileX < tiles; tileX++) {
                std::ofstream out{tilePath(folder, tileX, tileZ), std::ios::binary};
                if (!out) throw std::runtime_error("não deu pra gravar o terreno em " + folder);
                const uint32_t samples = TERRAIN_TILE_SAMPLES;
                const std::vector<float> &heights = tile(tileX, tileZ).heights;
                out.write(TILE_MAGIC, sizeof(TILE_MAGIC));
                out.write(reinterpret_cast<const char *>(&samples), sizeof(samples));
                out.write(reinterpret_cast<const char *>(heights.data()), static_cast<std::streamsize>(heights.size() * sizeof(float)));

                //pintura comprimida: cada sequência de pontos iguais vira (quantos, o ponto). Chão sem
                //pintura é um registro só; o pedaço inteiro sem compressão teria 17 MB
                std::ofstream paintOut{paintPath(folder, tileX, tileZ), std::ios::binary};
                if (!paintOut) throw std::runtime_error("não deu pra gravar a pintura do terreno em " + folder);
                const uint32_t paintSamples = TERRAIN_PAINT_SAMPLES;
                const uint32_t format = PAINT_FORMAT_RLE;
                paintOut.write(TILE_MAGIC, sizeof(TILE_MAGIC));
                paintOut.write(reinterpret_cast<const char *>(&paintSamples), sizeof(paintSamples));
                paintOut.write(reinterpret_cast<const char *>(&format), sizeof(format));
                const std::vector<PaintTexel> &paint = tile(tileX, tileZ).paint;
                for (size_t i = 0; i < paint.size();) {
                    uint32_t run = 1;
                    while (i + run < paint.size() && paint[i + run] == paint[i]) run++;
                    paintOut.write(reinterpret_cast<const char *>(&run), sizeof(run));
                    paintOut.write(reinterpret_cast<const char *>(&paint[i]), sizeof(PaintTexel));
                    i += run;
                }
            }
        }
    }

    //a pintura salva por um pedaço; arquivo que não existe = sem pintura
    static void loadPaint(const std::filesystem::path &path, std::vector<PaintTexel> &paint) {
        std::ifstream in{path, std::ios::binary};
        if (!in) return;
        char magic[4]{};
        uint32_t samples = 0, format = 0;
        in.read(magic, sizeof(magic));
        in.read(reinterpret_cast<char *>(&samples), sizeof(samples));
        in.read(reinterpret_cast<char *>(&format), sizeof(format));
        if (std::memcmp(magic, TILE_MAGIC, sizeof(magic)) != 0) throw std::runtime_error("pintura de terreno inválida: " + path.string());

        //o formato atual: 4 pontos por metro, comprimido
        if (format == PAINT_FORMAT_RLE) {
            if (samples != TERRAIN_PAINT_SAMPLES) throw std::runtime_error("pintura de terreno inválida: " + path.string());
            size_t filled = 0;
            while (filled < paint.size()) {
                uint32_t run = 0;
                PaintTexel texel{};
                in.read(reinterpret_cast<char *>(&run), sizeof(run));
                in.read(reinterpret_cast<char *>(&texel), sizeof(texel));
                if (!in || run == 0 || filled + run > paint.size()) throw std::runtime_error("pintura de terreno incompleta: " + path.string());
                std::fill(paint.begin() + static_cast<std::ptrdiff_t>(filled), paint.begin() + static_cast<std::ptrdiff_t>(filled + run), texel);
                filled += run;
            }
            return;
        }

        //formatos antigos: 1 ponto por metro (257 x 257). Cada ponto antigo vira os 4 x 4 pontos novos em volta
        const bool known = format == PAINT_FORMAT_PAIRS8 || format == PAINT_FORMAT_PAIRS4 || format == PAINT_FORMAT_DENSE8;
        if (samples != TERRAIN_TILE_SAMPLES || !known) throw std::runtime_error("pintura de terreno inválida: " + path.string());
        const size_t perSample = format == PAINT_FORMAT_PAIRS8 ? TERRAIN_POINT_SLOTS * 2
                               : format == PAINT_FORMAT_PAIRS4 ? 4 * 2 : PAINT_FORMAT_DENSE8;
        std::vector<uint8_t> bytes(static_cast<size_t>(TERRAIN_TILE_SAMPLES * TERRAIN_TILE_SAMPLES) * perSample);
        in.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!in) throw std::runtime_error("pintura de terreno incompleta: " + path.string());

        std::vector<PaintTexel> oldTexels(static_cast<size_t>(TERRAIN_TILE_SAMPLES * TERRAIN_TILE_SAMPLES));
        for (size_t s = 0; s < oldTexels.size(); s++) {
            const uint8_t *b = &bytes[s * perSample];
            LayerMix m{};
            m.weights[0] = 0.f;
            if (format == PAINT_FORMAT_PAIRS8 || format == PAINT_FORMAT_PAIRS4) {
                //os índices primeiro, depois os pesos
                const size_t pairs = perSample / 2;
                for (size_t i = 0; i < pairs; i++) {
                    m.ids[i] = b[i];
                    m.weights[i] = static_cast<float>(b[pairs + i]) / 255.f;
                }
            } else {
                //8 pesos fixos: a camada i é o material i da paleta
                for (size_t i = 0; i < PAINT_FORMAT_DENSE8; i++) {
                    m.ids[i] = static_cast<uint8_t>(i);
                    m.weights[i] = static_cast<float>(b[i]) / 255.f;
                }
            }
            normalizeMix(m);
            oldTexels[s] = packTexel(m);
        }
        //cada ponto novo mistura os 4 antigos em volta pela distância (como a GPU fazia entre os vértices:
        //a cena fica com a mesma cara, sem degrau de 1 m)
        for (int z = 0; z < TERRAIN_PAINT_SAMPLES; z++) {
            for (int x = 0; x < TERRAIN_PAINT_SAMPLES; x++) {
                const int oldX = std::min(x / TERRAIN_PAINT_PER_METER, TERRAIN_TILE_CELLS - 1);
                const int oldZ = std::min(z / TERRAIN_PAINT_PER_METER, TERRAIN_TILE_CELLS - 1);
                const float fx = static_cast<float>(x - oldX * TERRAIN_PAINT_PER_METER) / TERRAIN_PAINT_PER_METER;
                const float fz = static_cast<float>(z - oldZ * TERRAIN_PAINT_PER_METER) / TERRAIN_PAINT_PER_METER;
                std::array<float, 256> sums{};
                auto add = [&](int sx, int sz, float share) {
                    if (share <= 0.f) return;
                    const PaintTexel &t = oldTexels[static_cast<size_t>(sz * TERRAIN_TILE_SAMPLES + sx)];
                    for (size_t i = 0; i < t.ids.size(); i++) sums[t.ids[i]] += share * static_cast<float>(t.weights[i]);
                };
                add(oldX, oldZ, (1.f - fx) * (1.f - fz));
                add(oldX + 1, oldZ, fx * (1.f - fz));
                add(oldX, oldZ + 1, (1.f - fx) * fz);
                add(oldX + 1, oldZ + 1, fx * fz);
                //os 8 materiais que mais pesam ali (4 pontos de até 8 cada podem passar de 8)
                LayerMix m{};
                m.weights[0] = 0.f;
                for (size_t slot = 0; slot < m.ids.size(); slot++) {
                    const auto top = std::max_element(sums.begin(), sums.end());
                    if (*top <= 0.f) break;
                    m.ids[slot] = static_cast<uint8_t>(top - sums.begin());
                    m.weights[slot] = *top;
                    *top = 0.f;
                }
                normalizeMix(m);
                paint[texelIndex(x, z)] = packTexel(m);
            }
        }
    }

    TerrainData TerrainData::load(const std::string &folder, int tilesPerSide) {
        TerrainData terrain{tilesPerSide};
        for (int tileZ = 0; tileZ < terrain.tiles; tileZ++) {
            for (int tileX = 0; tileX < terrain.tiles; tileX++) {
                std::ifstream in{tilePath(folder, tileX, tileZ), std::ios::binary};
                if (!in) continue;  //pedaço que ainda não foi salvo: plano
                char magic[4]{};
                uint32_t samples = 0;
                in.read(magic, sizeof(magic));
                in.read(reinterpret_cast<char *>(&samples), sizeof(samples));
                if (std::memcmp(magic, TILE_MAGIC, sizeof(magic)) != 0 || samples != TERRAIN_TILE_SAMPLES) {
                    throw std::runtime_error("arquivo de terreno inválido: " + tilePath(folder, tileX, tileZ).string());
                }
                TerrainTile &t = terrain.tileData[static_cast<size_t>(tileZ * terrain.tiles + tileX)];
                in.read(reinterpret_cast<char *>(t.heights.data()), static_cast<std::streamsize>(t.heights.size() * sizeof(float)));
                if (!in) throw std::runtime_error("arquivo de terreno incompleto: " + tilePath(folder, tileX, tileZ).string());
                //pintura (opcional: terreno salvo antes das camadas não tem)
                loadPaint(paintPath(folder, tileX, tileZ), t.paint);
            }
        }
        return terrain;
    }
}
