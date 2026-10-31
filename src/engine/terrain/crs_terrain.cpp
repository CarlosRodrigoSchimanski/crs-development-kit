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
    //formatos do paint_x_z.bin (o 3º número do cabeçalho): 4 = (índice, peso) x 4 por amostra;
    //8 = o antigo, 8 pesos fixos por amostra (lido e convertido, nunca mais gravado)
    constexpr uint32_t PAINT_FORMAT_PAIRS = TERRAIN_LAYERS_PER_POINT;
    constexpr uint32_t PAINT_FORMAT_OLD_DENSE = 8;

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

    //a posição do material no ponto (-1 = não tem)
    static int findSlot(const LayerMix &mix, int layer) {
        for (int i = 0; i < TERRAIN_LAYERS_PER_POINT; i++) {
            if (mix.weights[static_cast<size_t>(i)] > 0.f && mix.ids[static_cast<size_t>(i)] == layer) return i;
        }
        return -1;
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
        for (int z = 0; z < result.samplesPerSide(); z++) {
            for (int x = 0; x < result.samplesPerSide(); x++) {
                const bool inOld = x < samplesPerSide() && z < samplesPerSide();
                const bool onKeptEdge = x == keep * TERRAIN_TILE_CELLS || z == keep * TERRAIN_TILE_CELLS;
                if (inOld && onKeptEdge) {
                    result.setSample(x, z, sample(x, z));
                    result.setMix(x, z, mix(x, z));
                }
            }
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
                TerrainTile &t = tileData[static_cast<size_t>(tileZ * tiles + tileX)];
                write(t, static_cast<size_t>(localZ * TERRAIN_TILE_SAMPLES + localX));
                t.version++;
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

    LayerMix TerrainData::mix(int x, int z) const {
        int tileIndex = 0;
        const size_t index = sampleIndex(x, z, tileIndex);
        return tileData[static_cast<size_t>(tileIndex)].paint[index];
    }

    void TerrainData::setMix(int x, int z, const LayerMix &value) {
        forEachCopy(x, z, [&](TerrainTile &t, size_t index) { t.paint[index] = value; });
    }

    float TerrainData::weight(int x, int z, int layer) const {
        const LayerMix m = mix(x, z);
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
                //1 no meio, 0 na borda, sem degrau (meia volta de cosseno)
                const float weight = 0.5f * (1.f + std::cos(glm::pi<float>() * distance));
                results.push_back({x, z, change(x, z, sample(x, z), weight)});
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
            //material novo no ponto: numa posição vazia, ou no lugar do mais fraco (máximo 4 por ponto)
            for (int i = 0; i < TERRAIN_LAYERS_PER_POINT && slot < 0; i++) {
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

    void TerrainData::paint(glm::vec2 center, float radius, int layer, float amount) {
        if (layer < 0 || layer >= TERRAIN_MAX_PALETTE) return;
        struct Result { int x, z; LayerMix value; };
        std::vector<Result> results;
        applyBrush(center, radius, [&](int x, int z, float height, float brush) {
            const LayerMix before = mix(x, z);
            const LayerMix after = paintMix(before, layer, amount * brush);
            if (after.ids != before.ids || after.weights != before.weights) results.push_back({x, z, after});
            return height;  //a altura não muda
        });
        for (const Result &r : results) setMix(r.x, r.z, r.value);
    }

    void TerrainData::removeLayer(int layer) {
        if (layer <= 0 || layer >= TERRAIN_MAX_PALETTE) return;  //o chão base não sai
        for (TerrainTile &t : tileData) {
            for (LayerMix &m : t.paint) {
                for (size_t i = 0; i < m.ids.size(); i++) {
                    if (m.weights[i] <= 0.f) continue;
                    if (m.ids[i] == layer) {
                        //o que era dele vai pro chão base (somado, se o ponto já tinha chão base)
                        const int base = findSlot(m, 0);
                        if (base >= 0) {
                            m.weights[static_cast<size_t>(base)] += m.weights[i];
                            m.weights[i] = 0.f;
                        } else {
                            m.ids[i] = 0;
                        }
                    } else if (m.ids[i] > layer) {
                        m.ids[i]--;
                    }
                }
            }
            t.version++;
        }
    }

    std::vector<float> TerrainData::layerUsage(int paletteSize) const {
        std::vector<double> sums(static_cast<size_t>(std::max(paletteSize, 1)), 0.0);
        for (const TerrainTile &t : tileData) {
            for (const LayerMix &m : t.paint) {
                for (size_t i = 0; i < m.ids.size(); i++) {
                    if (m.weights[i] > 0.f && m.ids[i] < sums.size()) sums[m.ids[i]] += m.weights[i];
                }
            }
        }
        const double total = std::accumulate(sums.begin(), sums.end(), 0.0);
        std::vector<float> usage(sums.size(), 0.f);
        for (size_t i = 0; i < sums.size(); i++) usage[i] = total > 0.0 ? static_cast<float>(sums[i] / total) : 0.f;
        return usage;
    }

    std::vector<uint8_t> TerrainData::chunkLayers(int tileX, int tileZ, int chunkX, int chunkZ) const {
        std::array<float, 256> sums{};
        const TerrainTile &t = tile(tileX, tileZ);
        for (int z = 0; z < TERRAIN_CHUNK_SAMPLES; z++) {
            for (int x = 0; x < TERRAIN_CHUNK_SAMPLES; x++) {
                const int localX = chunkX * TERRAIN_CHUNK_CELLS + x;
                const int localZ = chunkZ * TERRAIN_CHUNK_CELLS + z;
                const LayerMix &m = t.paint[static_cast<size_t>(localZ * TERRAIN_TILE_SAMPLES + localX)];
                for (size_t i = 0; i < m.ids.size(); i++) {
                    if (m.weights[i] > 0.f) sums[m.ids[i]] += m.weights[i];
                }
            }
        }
        std::vector<uint8_t> layers;
        for (int i = 0; i < 256; i++) {
            if (sums[static_cast<size_t>(i)] > 0.f) layers.push_back(static_cast<uint8_t>(i));
        }
        std::sort(layers.begin(), layers.end(), [&](uint8_t a, uint8_t b) { return sums[a] > sums[b]; });
        if (layers.size() > static_cast<size_t>(TERRAIN_CHUNK_LAYERS)) layers.resize(TERRAIN_CHUNK_LAYERS);
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

        const uint32_t samples = TERRAIN_TILE_SAMPLES;
        for (int tileZ = 0; tileZ < tiles; tileZ++) {
            for (int tileX = 0; tileX < tiles; tileX++) {
                std::ofstream out{tilePath(folder, tileX, tileZ), std::ios::binary};
                if (!out) throw std::runtime_error("não deu pra gravar o terreno em " + folder);
                const std::vector<float> &heights = tile(tileX, tileZ).heights;
                out.write(TILE_MAGIC, sizeof(TILE_MAGIC));
                out.write(reinterpret_cast<const char *>(&samples), sizeof(samples));
                out.write(reinterpret_cast<const char *>(heights.data()), static_cast<std::streamsize>(heights.size() * sizeof(float)));

                //pintura: por amostra, os 4 índices e depois os 4 pesos, 1 byte cada (0..255)
                const std::vector<LayerMix> &paint = tile(tileX, tileZ).paint;
                std::vector<uint8_t> bytes;
                bytes.reserve(paint.size() * TERRAIN_LAYERS_PER_POINT * 2);
                for (const LayerMix &m : paint) {
                    bytes.insert(bytes.end(), m.ids.begin(), m.ids.end());
                    for (float w : m.weights) bytes.push_back(static_cast<uint8_t>(std::lround(std::clamp(w, 0.f, 1.f) * 255.f)));
                }
                std::ofstream paintOut{paintPath(folder, tileX, tileZ), std::ios::binary};
                if (!paintOut) throw std::runtime_error("não deu pra gravar a pintura do terreno em " + folder);
                const uint32_t format = PAINT_FORMAT_PAIRS;
                paintOut.write(TILE_MAGIC, sizeof(TILE_MAGIC));
                paintOut.write(reinterpret_cast<const char *>(&samples), sizeof(samples));
                paintOut.write(reinterpret_cast<const char *>(&format), sizeof(format));
                paintOut.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            }
        }
    }

    //a pintura salva por um pedaço (os dois formatos); arquivo que não existe = sem pintura
    static void loadPaint(const std::filesystem::path &path, std::vector<LayerMix> &paint) {
        std::ifstream in{path, std::ios::binary};
        if (!in) return;
        char magic[4]{};
        uint32_t samples = 0, format = 0;
        in.read(magic, sizeof(magic));
        in.read(reinterpret_cast<char *>(&samples), sizeof(samples));
        in.read(reinterpret_cast<char *>(&format), sizeof(format));
        const bool known = format == PAINT_FORMAT_PAIRS || format == PAINT_FORMAT_OLD_DENSE;
        if (std::memcmp(magic, TILE_MAGIC, sizeof(magic)) != 0 || samples != TERRAIN_TILE_SAMPLES || !known) {
            throw std::runtime_error("pintura de terreno inválida: " + path.string());
        }
        const size_t perSample = format == PAINT_FORMAT_PAIRS ? TERRAIN_LAYERS_PER_POINT * 2 : PAINT_FORMAT_OLD_DENSE;
        std::vector<uint8_t> bytes(paint.size() * perSample);
        in.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!in) throw std::runtime_error("pintura de terreno incompleta: " + path.string());

        for (size_t s = 0; s < paint.size(); s++) {
            const uint8_t *b = &bytes[s * perSample];
            LayerMix m{};
            if (format == PAINT_FORMAT_PAIRS) {
                for (size_t i = 0; i < TERRAIN_LAYERS_PER_POINT; i++) {
                    m.ids[i] = b[i];
                    m.weights[i] = static_cast<float>(b[TERRAIN_LAYERS_PER_POINT + i]) / 255.f;
                }
            } else {
                //antigo: 8 pesos fixos (a camada i = o material i); ficam os 4 mais fortes
                std::array<uint8_t, PAINT_FORMAT_OLD_DENSE> order{0, 1, 2, 3, 4, 5, 6, 7};
                std::sort(order.begin(), order.end(), [&](uint8_t a, uint8_t c) { return b[a] > b[c]; });
                for (size_t i = 0; i < TERRAIN_LAYERS_PER_POINT; i++) {
                    m.ids[i] = order[i];
                    m.weights[i] = static_cast<float>(b[order[i]]) / 255.f;
                }
            }
            normalizeMix(m);
            paint[s] = m;
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
