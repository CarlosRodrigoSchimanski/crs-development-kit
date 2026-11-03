#include "engine/scene/crs_scene.hpp"

#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace crsdk {
    CrsGameObject& CrsScene::createGameObject() {
        return createGameObject(nextId);
    }

    CrsGameObject& CrsScene::createGameObject(CrsGameObject::id_t id) {
        if (gameObjects.count(id) > 0) {
            throw std::runtime_error("id de objeto repetido na cena: " + std::to_string(id));
        }
        //os próximos ids automáticos começam depois deste
        nextId = std::max(nextId, id + 1);
        auto [it, success] = gameObjects.emplace(id, CrsGameObject{id});
        return it->second;
    }

    CrsGameObject::Map& CrsScene::getGameObjects() {
        return gameObjects;
    }

    const CrsGameObject::Map& CrsScene::getGameObjects() const {
        return gameObjects;
    }

    void CrsScene::setActiveCamera(CrsGameObject::id_t cameraId) {
        activeCameraId = cameraId;
    }

    std::optional<CrsGameObject::id_t> CrsScene::getActiveCamera() const {
        return activeCameraId;
    }

    void CrsScene::destroyGameObject(CrsGameObject::id_t id) {
        //os filhos primeiro (juntados antes: apagar no meio do for invalidaria o laço)
        std::vector<CrsGameObject::id_t> children;
        for (const auto &kv : gameObjects) {
            if (kv.second.parentId == id) children.push_back(kv.first);
        }
        for (CrsGameObject::id_t child : children) destroyGameObject(child);
        gameObjects.erase(id);
        if (activeCameraId == id) activeCameraId.reset();
    }

    std::string CrsScene::uniqueName(const std::string &name) const {
        //"cube.003" -> "cube": o número novo vai em cima do nome sem número
        std::string base = name;
        const size_t dot = name.rfind('.');
        if (dot != std::string::npos && dot + 4 == name.size() &&
            std::all_of(name.begin() + static_cast<std::ptrdiff_t>(dot) + 1, name.end(), [](char c) { return c >= '0' && c <= '9'; })) {
            base = name.substr(0, dot);
        }
        auto used = [&](const std::string &candidate) {
            for (const auto &kv : gameObjects) {
                if (kv.second.name == candidate) return true;
            }
            return false;
        };
        for (int n = 1; n < 100000; n++) {
            char suffix[16];
            std::snprintf(suffix, sizeof(suffix), ".%03d", n);
            if (!used(base + suffix)) return base + suffix;
        }
        return base;
    }

    std::optional<CrsGameObject::id_t> CrsScene::duplicateGameObject(CrsGameObject::id_t id) {
        if (!gameObjects.contains(id)) return std::nullopt;
        //a árvore do objeto: ele e os filhos (pais antes dos filhos, pra remapear o parentId na ordem)
        std::vector<CrsGameObject::id_t> tree{id};
        for (size_t i = 0; i < tree.size(); i++) {
            for (const auto &kv : gameObjects) {
                if (kv.second.parentId == tree[i]) tree.push_back(kv.first);
            }
        }
        std::unordered_map<CrsGameObject::id_t, CrsGameObject::id_t> newIds;
        for (CrsGameObject::id_t old : tree) {
            CrsGameObject copy = gameObjects.at(old);  //cópia antes de inserir (inserir pode mover o mapa)
            copy.id = nextId++;
            newIds[old] = copy.id;
            //o primeiro fica no mesmo pai do original; os filhos passam a ser filhos das cópias
            if (old != id) copy.parentId = newIds.at(*copy.parentId);
            copy.name = uniqueName(copy.name);
            if (copy.terrain) copy.terrain->data = std::make_shared<TerrainData>(*copy.terrain->data);
            gameObjects.emplace(copy.id, std::move(copy));
        }
        return newIds.at(id);
    }

    CrsGameObject* CrsScene::findGameObjectById(CrsGameObject::id_t id) {
        auto it = gameObjects.find(id);
        if (it != gameObjects.end()) {
            return &it->second;
        }
        return nullptr;
    }

    glm::mat4 CrsScene::worldMatrix(const CrsGameObject &obj) {
        glm::mat4 local = obj.transform.mat4();
        if (!obj.parentId) return local;  //raiz: o transform já é no mundo

        CrsGameObject *parent = findGameObjectById(*obj.parentId);
        if (parent == nullptr) return local;  //pai não existe mais: trata como raiz
        return worldMatrix(*parent) * local;
    }

    bool CrsScene::setParent(CrsGameObject::id_t childId, std::optional<CrsGameObject::id_t> parentId) {
        CrsGameObject *child = findGameObjectById(childId);
        if (!child || child->terrain || child->parentId == parentId) return false;
        CrsGameObject *parent = nullptr;
        if (parentId) {
            parent = findGameObjectById(*parentId);
            if (!parent) return false;
            //subindo do pai novo até a raiz: se passar pelo objeto, ele viraria avô de si mesmo
            for (CrsGameObject *p = parent; p; p = p->parentId ? findGameObjectById(*p->parentId) : nullptr) {
                if (p->getId() == childId) return false;
            }
        }
        //onde ele está no mundo agora, visto de dentro do pai novo
        const glm::mat4 world = worldMatrix(*child);
        const glm::mat4 parentWorld = parent ? worldMatrix(*parent) : glm::mat4{1.f};
        child->transform.setFromMatrix(glm::inverse(parentWorld) * world);
        child->parentId = parentId;
        return true;
    }

    std::optional<WorldBounds> CrsScene::worldBounds(CrsGameObject::id_t id) {
        CrsGameObject *root = findGameObjectById(id);
        if (!root) return std::nullopt;
        std::optional<WorldBounds> result;
        auto addPoint = [&](const glm::vec3 &p) {
            if (!result) result = WorldBounds{p, p};
            result->min = glm::min(result->min, p);
            result->max = glm::max(result->max, p);
        };
        //uma caixa local (do modelo ou da física): os 8 cantos levados pro mundo
        auto addBox = [&](const glm::mat4 &world, const glm::vec3 &boxMin, const glm::vec3 &boxMax) {
            for (int i = 0; i < 8; i++) {
                const glm::vec3 local{i & 1 ? boxMax.x : boxMin.x, i & 2 ? boxMax.y : boxMin.y, i & 4 ? boxMax.z : boxMin.z};
                addPoint(glm::vec3{world * glm::vec4{local, 1.f}});
            }
        };

        //o objeto e todos os descendentes (a malha do Player, por exemplo, mora num filho)
        std::vector<CrsGameObject *> pending{root};
        while (!pending.empty()) {
            CrsGameObject &obj = *pending.back();
            pending.pop_back();
            const glm::mat4 world = worldMatrix(obj);
            bool hasShape = false;
            if (obj.model) {
                addBox(world, obj.model->getBoundsMin(), obj.model->getBoundsMax());
                hasShape = true;
            }
            if (obj.rigidBody) {
                addBox(world, obj.rigidBody->boundsMin(), obj.rigidBody->boundsMax());
                hasShape = true;
            }
            if (obj.terrain) {
                //o terreno não gira: do canto até o outro canto, da altura mais baixa à mais alta
                const TerrainData &data = *obj.terrain->data;
                float low = 0.f, high = 0.f;
                bool first = true;
                for (int tz = 0; tz < data.tilesPerSide(); tz++) {
                    for (int tx = 0; tx < data.tilesPerSide(); tx++) {
                        for (float h : data.tile(tx, tz).heights) {
                            low = first ? h : std::min(low, h);
                            high = first ? h : std::max(high, h);
                            first = false;
                        }
                    }
                }
                const glm::vec3 corner = obj.terrain->corner(glm::vec3{world[3]});
                addPoint(corner + glm::vec3{0.f, low, 0.f});
                addPoint(corner + glm::vec3{data.size(), high, data.size()});
                hasShape = true;
            }
            //só o objeto pedido vira ponto: um filho vazio (o pivô da câmera) não aumenta a caixa
            if (!hasShape && &obj == root) addPoint(glm::vec3{world[3]});
            for (auto &kv : gameObjects) {
                if (kv.second.parentId && *kv.second.parentId == obj.getId()) pending.push_back(&kv.second);
            }
        }
        return result;
    }

    std::optional<CrsGameObject::id_t> CrsScene::pickObject(const Ray &ray) {
        std::optional<CrsGameObject::id_t> closestId{};
        float closestT = 0.f;

        for (auto &kv : gameObjects) {
            auto &obj = kv.second;
            if (obj.model == nullptr) continue;  //sem modelo não tem caixa pra clicar

            auto t = intersectRayObject(ray, worldMatrix(obj), obj.model->getBoundsMin(), obj.model->getBoundsMax());
            if (t && (!closestId || *t < closestT)) {
                closestId = obj.getId();
                closestT = *t;
            }
        }
        return closestId;
    }

    std::optional<RayCastHit> CrsScene::raycast(const Ray &ray, float maxDistance, std::optional<CrsGameObject::id_t> ignoreId) {
        std::optional<RayCastHit> closest{};
        for (auto &kv : gameObjects) {
            auto &obj = kv.second;
            if (!obj.rigidBody) continue;
            if (ignoreId && obj.getId() == *ignoreId) continue;

            //esfera e cápsula: pela caixa que envolve a forma (aproximado nas pontas arredondadas)
            auto t = intersectRayObject(ray, worldMatrix(obj), obj.rigidBody->boundsMin(), obj.rigidBody->boundsMax());

            if (t && *t <= maxDistance && (!closest || *t < closest->distance)) {
                closest = RayCastHit{obj.getId(), *t};
            }
        }
        //terrenos: o raio levado pras coordenadas locais (a partir do canto)
        for (auto &kv : gameObjects) {
            auto &obj = kv.second;
            if (!obj.terrain) continue;
            if (ignoreId && obj.getId() == *ignoreId) continue;
            const glm::vec3 origin = ray.origin - obj.terrain->corner(obj.transform.translation);
            auto t = obj.terrain->data->raycast(origin, ray.direction, maxDistance);
            if (t && (!closest || *t < closest->distance)) closest = RayCastHit{obj.getId(), *t};
        }
        return closest;
    }

    std::optional<float> CrsScene::terrainHeightAt(float x, float z) {
        for (auto &kv : gameObjects) {
            auto &obj = kv.second;
            if (!obj.terrain) continue;
            const glm::vec3 corner = obj.terrain->corner(obj.transform.translation);
            const float localX = x - corner.x, localZ = z - corner.z;
            const float size = obj.terrain->data->size();
            if (localX < 0.f || localZ < 0.f || localX > size || localZ > size) continue;
            return corner.y + obj.terrain->data->heightAt(localX, localZ);
        }
        return std::nullopt;
    }

}