#include "engine/scene/lve_scene.hpp"

namespace lve {
    LveGameObject& LveScene::createGameObject() {
        auto obj = LveGameObject::createGameObject();
        auto id = obj.getId();
        auto [it, success] = gameObjects.emplace(id, std::move(obj));
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

}