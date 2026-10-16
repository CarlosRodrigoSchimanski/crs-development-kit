#pragma once

#include "engine/scene/lve_game_object.hpp"
#include <optional>

namespace lve {
    class LveScene{
        public:
            //cria um objeto ja dentro da sena e devolve ele para configurar
            LveGameObject& createGameObject();

            LveGameObject::Map& getGameObjects();
            const LveGameObject::Map& getGameObjects() const;

            void setActiveCamera(LveGameObject::id_t cameraId);
            std::optional<LveGameObject::id_t> getActiveCamera() const;
        
        private:
            LveGameObject::Map gameObjects;
            std::optional<LveGameObject::id_t> activeCameraId{};
    };
}