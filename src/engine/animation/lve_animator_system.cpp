#include "engine/animation/lve_animator_system.hpp"

namespace lve {

    void AnimatorSystem::update(LveScene &scene, float dt) {
        for (auto &kv : scene.getGameObjects()) {
            auto &obj = kv.second;
            if (!obj.animator) continue;
            //o loop (voltar ao começo) é feito na hora de amostrar a pose, pela duração da animação
            obj.animator->time += dt * obj.animator->speed;
        }
    }
}
