#pragma once

#include "engine/scene/lve_scene.hpp"

namespace lve {
    //avança o relógio das animações (AnimatorComponent); quem calcula a pose desse instante é o SkinningSystem
    class AnimatorSystem {
        public:
            void update(LveScene &scene, float dt);
    };
}
