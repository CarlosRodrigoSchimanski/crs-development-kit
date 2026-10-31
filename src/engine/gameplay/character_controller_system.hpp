#pragma once

#include "engine/scene/crs_scene.hpp"

namespace crsdk {
    float turnTowards(float current, float target, float maxStep);

    //move cada personagem pela intenção dele (CharacterComponent::input, que os componentes Python
    //preenchem antes); gravidade e chao ficam com o PhysicsSystem
    class CharacterControllerSystem {
    public:
        void update(CrsScene &scene);
        void faceMovement(CrsScene &scene, float dt);
    };

}
