#pragma once

#include "engine/scene/lve_scene.hpp"

namespace lve {
    //o que controla(jogador / ia) quer fazer neste frame
    struct CharacterInput
    {
        glm::vec3 moveDirection{0.f}; //no mundo; só x e z contam (o chao)
        bool jump = false;
    };
    
    //move os personagens pela intenção; gravidade e chao ficam com o PhysicsSystem
    class CharacterControllerSystem
    {
    public:
        void update(LveScene &scene, const CharacterInput &input);
    };
    
}