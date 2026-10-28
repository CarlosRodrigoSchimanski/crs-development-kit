#pragma once

#include "engine/scene/lve_scene.hpp"

namespace lve {
    //gira os pivôs de câmera (CameraRigComponent) com o movimento do mouse
    class CameraRigSystem {
        public:
            void update(LveScene &scene, glm::vec2 mouseDelta);
    };
}