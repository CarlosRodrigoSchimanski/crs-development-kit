#include "engine/scene/lve_scene.hpp"

namespace lve {
    LveGameObject& LveScene::createGameObject() {
        LveGameObject obj{nextId++};
        auto [it, success] = gameObjects.emplace(obj.getId(), std::move(obj));
        return it->second;
    }

    LveGameObject::Map& LveScene::getGameObjects() {
        return gameObjects;
    }

    const LveGameObject::Map& LveScene::getGameObjects() const {
        return gameObjects;
    }

    void LveScene::setActiveCamera(LveGameObject::id_t cameraId) {
        activeCameraId = cameraId;
    }

    std::optional<LveGameObject::id_t> LveScene::getActiveCamera() const {
        return activeCameraId;
    }

    LveGameObject* LveScene::findGameObjectById(LveGameObject::id_t id) {
        auto it = gameObjects.find(id);
        if (it != gameObjects.end()) {
            return &it->second;
        }
        return nullptr;
    }

    glm::mat4 LveScene::worldMatrix(const LveGameObject &obj) {
        glm::mat4 local = obj.transform.mat4();
        if (!obj.parentId) return local;  //raiz: o transform já é no mundo

        LveGameObject *parent = findGameObjectById(*obj.parentId);
        if (parent == nullptr) return local;  //pai não existe mais: trata como raiz
        return worldMatrix(*parent) * local;
    }

    std::optional<LveGameObject::id_t> LveScene::pickObject(const Ray &ray) {
        std::optional<LveGameObject::id_t> closestId{};
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

    std::optional<RayCastHit> LveScene::raycast(const Ray &ray, float maxDistance, std::optional<LveGameObject::id_t> ignoreId) {
        std::optional<RayCastHit> closest{};
        for (auto &kv : gameObjects) {
            auto &obj = kv.second;
            if (!obj.rigidBody) continue;
            if (ignoreId && obj.getId() == *ignoreId) continue;

            glm::vec3 boxMin = obj.rigidBody->center - obj.rigidBody->halfExtents;
            glm::vec3 boxMax = obj.rigidBody->center + obj.rigidBody->halfExtents;
            auto t = intersectRayObject(ray, worldMatrix(obj), boxMin, boxMax);

            if (t && *t <= maxDistance && (!closest || *t < closest->distance)) {
                closest = RayCastHit{obj.getId(), *t};
            }
        }
        return closest;
    }

}