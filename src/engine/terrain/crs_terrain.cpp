#include "engine/terrain/crs_terrain.hpp"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace crsdk {

    //começo do arquivo de cada pedaço: confere que é mesmo um pedaço de terreno, e de que tamanho
    constexpr char TILE_MAGIC[4] = {'C', 'R', 'S', 'T'};

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
            }
        }
        //pedaço novo encostado num antigo: a borda compartilhada tem que ter a altura do antigo
        for (int z = 0; z < result.samplesPerSide(); z++) {
            for (int x = 0; x < result.samplesPerSide(); x++) {
                const bool inOld = x < samplesPerSide() && z < samplesPerSide();
                const bool onKeptEdge = x == keep * TERRAIN_TILE_CELLS || z == keep * TERRAIN_TILE_CELLS;
                if (inOld && onKeptEdge) result.setSample(x, z, sample(x, z));
            }
        }
        return result;
    }

    float TerrainData::sample(int x, int z) const {
        const int last = samplesPerSide() - 1;
        x = std::clamp(x, 0, last);
        z = std::clamp(z, 0, last);
        //o pedaço que começa na amostra (o último pedaço fica com a borda final)
        const int tileX = std::min(x / TERRAIN_TILE_CELLS, tiles - 1);
        const int tileZ = std::min(z / TERRAIN_TILE_CELLS, tiles - 1);
        const int localX = x - tileX * TERRAIN_TILE_CELLS;
        const int localZ = z - tileZ * TERRAIN_TILE_CELLS;
        return tile(tileX, tileZ).heights[static_cast<size_t>(localZ * TERRAIN_TILE_SAMPLES + localX)];
    }

    void TerrainData::setSample(int x, int z, float height) {
        const int last = samplesPerSide() - 1;
        if (x < 0 || z < 0 || x > last || z > last) return;
        //na divisa a amostra é a última de um pedaço e a primeira do outro: grava nos dois
        for (int tileZ = (z - 1) / TERRAIN_TILE_CELLS; tileZ <= z / TERRAIN_TILE_CELLS; tileZ++) {
            for (int tileX = (x - 1) / TERRAIN_TILE_CELLS; tileX <= x / TERRAIN_TILE_CELLS; tileX++) {
                if (tileX < 0 || tileZ < 0 || tileX >= tiles || tileZ >= tiles) continue;
                const int localX = x - tileX * TERRAIN_TILE_CELLS;
                const int localZ = z - tileZ * TERRAIN_TILE_CELLS;
                if (localX < 0 || localZ < 0 || localX > TERRAIN_TILE_CELLS || localZ > TERRAIN_TILE_CELLS) continue;
                TerrainTile &t = tileData[static_cast<size_t>(tileZ * tiles + tileX)];
                t.heights[static_cast<size_t>(localZ * TERRAIN_TILE_SAMPLES + localX)] = height;
                t.version++;
            }
        }
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

    static std::filesystem::path tilePath(const std::string &folder, int tileX, int tileZ) {
        return std::filesystem::path{folder} / ("tile_" + std::to_string(tileX) + "_" + std::to_string(tileZ) + ".bin");
    }

    void TerrainData::save(const std::string &folder) const {
        std::filesystem::create_directories(folder);
        //pedaços de quando o terreno era maior não podem voltar ao carregar
        std::vector<std::filesystem::path> old;  //juntados antes: apagar no meio da listagem não é seguro
        for (const auto &entry : std::filesystem::directory_iterator{folder}) {
            const std::string name = entry.path().filename().string();
            if (name.rfind("tile_", 0) == 0 && entry.path().extension() == ".bin") old.push_back(entry.path());
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
                std::vector<float> &heights = terrain.tileData[static_cast<size_t>(tileZ * terrain.tiles + tileX)].heights;
                in.read(reinterpret_cast<char *>(heights.data()), static_cast<std::streamsize>(heights.size() * sizeof(float)));
                if (!in) throw std::runtime_error("arquivo de terreno incompleto: " + tilePath(folder, tileX, tileZ).string());
            }
        }
        return terrain;
    }
}
