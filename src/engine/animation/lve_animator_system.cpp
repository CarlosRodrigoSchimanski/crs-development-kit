#include "engine/animation/lve_animator_system.hpp"

namespace lve {

    void AnimatorSystem::update(LveScene &scene, float dt) {
        for (auto &kv : scene.getGameObjects()) {
            auto &obj = kv.second;
            if (!obj.animator) continue;
            AnimatorComponent &animator = *obj.animator;
            //o loop (voltar ao começo) é feito na hora de amostrar a pose, pela duração da animação
            animator.time += dt * animator.speed;

            if (animator.isBlending()) {
                animator.previousTime += dt * animator.speed;
                animator.fadeElapsed += dt;
                //transição acabou: fica só a nova
                if (animator.fadeElapsed >= animator.fadeDuration) {
                    animator.fadeDuration = 0.f;
                    animator.previousClip.clear();
                }
            }
        }
    }
}
