#pragma once

#include "engine/scene/crs_scene.hpp"

namespace crsdk {
    //gira os pivôs de câmera (CameraRigComponent) com o movimento do mouse
    class CameraRigSystem {
        public:
            void update(CrsScene &scene, glm::vec2 mouseDelta);
            //põe a câmera filha de cada pivô atrás dele, parando antes do que estiver no caminho (depois da física)
            void placeCamera(CrsScene &scene);
    };
}