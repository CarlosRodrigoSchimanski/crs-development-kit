#pragma once

#include "engine/scene/crs_scene.hpp"


namespace crsdk {
    class PhysicsSystem {
        public:
            void update(CrsScene &scene, float dt);

            glm::vec3 gravity{0.f, -9.8f, 0.f};
            
    };
}