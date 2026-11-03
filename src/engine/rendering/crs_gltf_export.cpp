#include "engine/rendering/crs_gltf_export.hpp"

#include <nlohmann/json.hpp>

#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace crsdk {
    using json = nlohmann::json;

    //códigos do formato .glb (especificação do glTF 2.0)
    constexpr uint32_t GLB_MAGIC = 0x46546C67;       //"glTF"
    constexpr uint32_t GLB_VERSION = 2;
    constexpr uint32_t CHUNK_JSON = 0x4E4F534A;      //"JSON"
    constexpr uint32_t CHUNK_BIN = 0x004E4942;       //"BIN"
    constexpr int TARGET_VERTICES = 34962;           //ARRAY_BUFFER
    constexpr int TARGET_INDICES = 34963;            //ELEMENT_ARRAY_BUFFER
    constexpr int COMPONENT_FLOAT = 5126;
    constexpr int COMPONENT_UINT = 5125;
    constexpr int MODE_TRIANGLES = 4;

    void writeGlb(const std::string &filepath, const std::vector<ExportMesh> &meshes) {
        json document;
        document["asset"] = {{"version", "2.0"}, {"generator", "CRS Development Kit"}};
        json nodes = json::array(), jsonMeshes = json::array(), accessors = json::array(), views = json::array();
        std::vector<uint8_t> binary;

        //um bloco de dados no binário + o bufferView que aponta pra ele (tudo de 4 em 4 bytes: alinhado)
        auto addView = [&](const void *data, size_t bytes, int target) {
            const size_t offset = binary.size();
            binary.resize(offset + bytes);
            std::memcpy(binary.data() + offset, data, bytes);
            views.push_back({{"buffer", 0}, {"byteOffset", offset}, {"byteLength", bytes}, {"target", target}});
            return static_cast<int>(views.size() - 1);
        };

        for (const ExportMesh &mesh : meshes) {
            if (mesh.positions.empty() || mesh.indices.size() < 3) continue;
            //o POSITION do glTF exige o menor e o maior ponto
            glm::vec3 low{std::numeric_limits<float>::max()}, high{std::numeric_limits<float>::lowest()};
            for (const glm::vec3 &p : mesh.positions) {
                low = glm::min(low, p);
                high = glm::max(high, p);
            }
            json attributes;
            const int positionView = addView(mesh.positions.data(), mesh.positions.size() * sizeof(glm::vec3), TARGET_VERTICES);
            accessors.push_back({{"bufferView", positionView}, {"componentType", COMPONENT_FLOAT}, {"count", mesh.positions.size()},
                                 {"type", "VEC3"}, {"min", {low.x, low.y, low.z}}, {"max", {high.x, high.y, high.z}}});
            attributes["POSITION"] = accessors.size() - 1;
            if (mesh.normals.size() == mesh.positions.size()) {
                const int normalView = addView(mesh.normals.data(), mesh.normals.size() * sizeof(glm::vec3), TARGET_VERTICES);
                accessors.push_back({{"bufferView", normalView}, {"componentType", COMPONENT_FLOAT}, {"count", mesh.normals.size()},
                                     {"type", "VEC3"}});
                attributes["NORMAL"] = accessors.size() - 1;
            }
            const int indexView = addView(mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t), TARGET_INDICES);
            accessors.push_back({{"bufferView", indexView}, {"componentType", COMPONENT_UINT}, {"count", mesh.indices.size()},
                                 {"type", "SCALAR"}});
            const size_t indices = accessors.size() - 1;

            jsonMeshes.push_back({{"name", mesh.name},
                                  {"primitives", json::array({{{"attributes", attributes}, {"indices", indices}, {"mode", MODE_TRIANGLES}}})}});
            nodes.push_back({{"name", mesh.name}, {"mesh", jsonMeshes.size() - 1}});
        }

        json sceneNodes = json::array();
        for (size_t i = 0; i < nodes.size(); i++) sceneNodes.push_back(i);
        document["scene"] = 0;
        document["scenes"] = json::array({{{"nodes", sceneNodes}}});
        document["nodes"] = nodes;
        document["meshes"] = jsonMeshes;
        document["accessors"] = accessors;
        document["bufferViews"] = views;
        document["buffers"] = json::array({{{"byteLength", binary.size()}}});

        //os dois pedaços do .glb têm que ter tamanho múltiplo de 4: o JSON completa com espaço, o binário com 0
        std::string text = document.dump();
        while (text.size() % 4 != 0) text.push_back(' ');
        while (binary.size() % 4 != 0) binary.push_back(0);

        std::ofstream out{filepath, std::ios::binary};
        if (!out) throw std::runtime_error("não deu pra gravar " + filepath);
        auto write32 = [&](uint32_t value) { out.write(reinterpret_cast<const char *>(&value), sizeof(value)); };
        const uint32_t total = 12 + 8 + static_cast<uint32_t>(text.size()) + 8 + static_cast<uint32_t>(binary.size());
        write32(GLB_MAGIC);
        write32(GLB_VERSION);
        write32(total);
        write32(static_cast<uint32_t>(text.size()));
        write32(CHUNK_JSON);
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        write32(static_cast<uint32_t>(binary.size()));
        write32(CHUNK_BIN);
        out.write(reinterpret_cast<const char *>(binary.data()), static_cast<std::streamsize>(binary.size()));
        if (!out) throw std::runtime_error("não deu pra gravar " + filepath);
    }

    std::vector<ExportMesh> collectGroupMeshes(CrsScene &scene, CrsGameObject::id_t rootId, const GeometryOf &geometryOf) {
        std::vector<ExportMesh> meshes;
        CrsGameObject *root = scene.findGameObjectById(rootId);
        if (!root) return meshes;
        //tudo relativo ao objeto do grupo: ele fica na origem do arquivo
        const glm::mat4 toGroup = glm::inverse(scene.worldMatrix(*root));

        std::vector<CrsGameObject *> pending{root};
        while (!pending.empty()) {
            CrsGameObject &obj = *pending.back();
            pending.pop_back();
            for (auto &kv : scene.getGameObjects()) {
                if (kv.second.parentId && *kv.second.parentId == obj.getId()) pending.push_back(&kv.second);
            }
            if (obj.editorOnly || obj.terrain) continue;  //gizmo do editor (Empty, câmera) e terreno: fora
            const CrsModel::Builder *geometry = geometryOf(obj);
            if (!geometry) continue;

            const glm::mat4 matrix = toGroup * scene.worldMatrix(obj);
            const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3{matrix}));
            const std::string name = obj.name.empty() ? "objeto_" + std::to_string(obj.getId()) : obj.name;

            if (!geometry->vertices.empty()) {
                ExportMesh mesh{name, {}, {}, geometry->indices};
                for (const CrsModel::Vertex &v : geometry->vertices) {
                    mesh.positions.push_back(glm::vec3{matrix * glm::vec4{v.position, 1.f}});
                    const glm::vec3 normal = normalMatrix * v.normal;
                    mesh.normals.push_back(glm::length(normal) > 0.f ? glm::normalize(normal) : glm::vec3{0.f, 1.f, 0.f});
                }
                //sem índices no modelo: os vértices já vêm de 3 em 3
                if (mesh.indices.empty()) {
                    for (uint32_t i = 0; i < mesh.positions.size(); i++) mesh.indices.push_back(i);
                }
                meshes.push_back(std::move(mesh));
            }
            if (!geometry->collision.empty()) {
                ExportMesh mesh{"UCX_" + name, {}, {}, geometry->collision.indices};
                for (const glm::vec3 &v : geometry->collision.vertices) mesh.positions.push_back(glm::vec3{matrix * glm::vec4{v, 1.f}});
                meshes.push_back(std::move(mesh));
            }
        }
        return meshes;
    }
}
