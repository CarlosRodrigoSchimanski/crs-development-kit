#pragma once

#include "engine/scene/lve_scene.hpp"

namespace lve {
    //escolhe a animação do visual do personagem pelo movimento do pai: parado, andando, correndo ou no ar.
    //provisório: quando tiver scripts, essa regra vira Python chamando animator.play()
    class CharacterAnimationSystem {
        public:
            //depois da física (usa a velocidade e o onGround do frame)
            void update(LveScene &scene);
    };
}
