#include "engine/scene/crs_scene.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

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