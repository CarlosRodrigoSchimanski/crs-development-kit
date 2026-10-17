#pragma once

#include "engine/scene/lve_scene.hpp"


namespace lve {
    class PhysicsSystem {
        public:
            void update(LveScene &scene, float dt);

            glm::vec3 gravity{0.f, 9.8f, 0.f};
            
    };
}