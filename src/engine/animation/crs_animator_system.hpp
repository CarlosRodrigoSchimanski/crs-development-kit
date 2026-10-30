#pragma once

#include "engine/scene/crs_scene.hpp"

namespace crsdk {
    //avança o relógio das animações (AnimatorComponent); quem calcula a pose desse instante é o SkinningSystem
    class AnimatorSystem {
        public:
            void update(CrsScene &scene, float dt);
    };
}
