#pragma once

#include "engine/scene/lve_scene.hpp"

namespace lve {
    //o que controla(jogador / ia) quer fazer neste frame
    struct CharacterInput {
        glm::vec3 moveDirection{0.f}; //no mundo; só x e z contam (o chao)
        bool jump = false;
        bool run = false;  //corre (runSpeed) em vez de andar (moveSpeed)
    };

    float turnTowards(float current, float target, float maxStep);
    
    //move os personagens pela intenção; gravidade e chao ficam com o PhysicsSystem
    class CharacterControllerSystem {
    public:
        void update(LveScene &scene, const CharacterInput &input);
        void faceMovement(LveScene &scene, float dt);
    };
    
}