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
            
            LveGameObject* findGameObjectById(LveGameObject::id_t id);
            //matriz do objeto no mundo: a dos pais (do mais distante ao mais perto) x a dele
            glm::mat4 worldMatrix(const LveGameObject &obj);

        
        private:
            LveGameObject::Map gameObjects;
            std::optional<LveGameObject::id_t> activeCameraId{};
    };
}