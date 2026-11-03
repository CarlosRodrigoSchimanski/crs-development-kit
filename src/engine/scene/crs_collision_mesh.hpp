#pragma once

#include <glm/glm.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <map>
#include <set>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace crsdk {
    //colisão que vem dentro do modelo (.glb): as malhas que o artista chamou de UCX_... ou COL_...
    //no Blender. Não são desenhadas; viram colisão ESTÁTICA com a forma exata (côncava ou não: parede
    //com escada, porta, vão). No espaço do modelo, já com a transformação de cada malha aplicada
    struct CollisionMesh {
        std::vector<glm::vec3> vertices;
        std::vector<uint32_t> indices;  //de 3 em 3: os triângulos
        std::vector<uint32_t> edges;    //de 2 em 2: cada aresta uma vez só (o contorno verde do editor)

        bool empty() const { return indices.size() < 3; }

        //as arestas de verdade a partir dos triângulos: a borda e onde a superfície dobra. A diagonal
        //no meio de uma face plana (os dois triângulos de um quadrado) fica de fora: só polui o contorno
        void buildEdges() {
            //o .glb repete o vértice em cada face (a quina do cubo são 3): soldados pela posição
            std::map<std::tuple<float, float, float>, uint32_t> byPosition;
            std::vector<uint32_t> weld(vertices.size());
            for (size_t i = 0; i < vertices.size(); i++) {
                const auto key = std::make_tuple(vertices[i].x, vertices[i].y, vertices[i].z);
                weld[i] = byPosition.try_emplace(key, static_cast<uint32_t>(i)).first->second;
            }
            //cada aresta (já soldada) e as normais dos triângulos que usam ela
            std::map<std::pair<uint32_t, uint32_t>, std::vector<glm::vec3>> faces;
            for (size_t i = 0; i + 2 < indices.size(); i += 3) {
                const glm::vec3 a = vertices[indices[i]], b = vertices[indices[i + 1]], c = vertices[indices[i + 2]];
                const glm::vec3 cross = glm::cross(b - a, c - a);
                if (glm::length(cross) <= 0.f) continue;  //triângulo degenerado
                const glm::vec3 normal = glm::normalize(cross);
                for (int k = 0; k < 3; k++) {
                    uint32_t p = weld[indices[i + static_cast<size_t>(k)]];
                    uint32_t q = weld[indices[i + static_cast<size_t>((k + 1) % 3)]];
                    faces[{std::min(p, q), std::max(p, q)}].push_back(normal);
                }
            }
            edges.clear();
            for (const auto &[edge, normals] : faces) {
                //borda (um triângulo só) ou dobra de mais de ~10 graus
                const bool keep = normals.size() != 2 || glm::dot(normals[0], normals[1]) < 0.985f;
                if (!keep) continue;
                edges.push_back(edge.first);
                edges.push_back(edge.second);
            }
        }
    };

    //o nome marca uma malha de colisão: começa com UCX_ (o padrão da Unreal, que os addons do Blender
    //usam) ou COL_, sem diferença de maiúscula
    inline bool isCollisionMeshName(std::string_view name) {
        auto startsWith = [&](std::string_view prefix) {
            if (name.size() < prefix.size()) return false;
            for (size_t i = 0; i < prefix.size(); i++) {
                if (std::toupper(static_cast<unsigned char>(name[i])) != prefix[i]) return false;
            }
            return true;
        };
        return startsWith("UCX_") || startsWith("COL_");
    }
}
