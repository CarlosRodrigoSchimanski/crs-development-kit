#pragma once

#include "engine/scene/lve_game_object.hpp"

namespace lve {
    class LveScene{
        public:
            //cria um objeto ja dentro da sena e devolve ele para configurar
            LveGameObject& createGameObject();

            LveGameObject::Map& getGameObjects();
            const LveGameObject::Map& getGameObjects() const;
        
        private:
            LveGameObject::Map gameObjects;
    };
}