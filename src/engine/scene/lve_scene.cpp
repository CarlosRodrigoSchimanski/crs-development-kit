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
}