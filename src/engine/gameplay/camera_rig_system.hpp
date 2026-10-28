#pragma once

#include "engine/scene/lve_scene.hpp"

namespace lve {
    //gira os pivôs de câmera (CameraRigComponent) com o movimento do mouse
    class CameraRigSystem {
        public:
            void update(LveScene &scene, glm::vec2 mouseDelta);
            //põe a câmera filha de cada pivô atrás dele, parando antes do que estiver no caminho (depois da física)
            void placeCamera(LveScene &scene);
    };
}