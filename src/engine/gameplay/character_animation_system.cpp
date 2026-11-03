#include "engine/gameplay/character_animation_system.hpp"

namespace crsdk {
    //abaixo disso conta como parado (unidades por segundo)
    constexpr float STOPPED_SPEED = 0.1f;

    void CharacterAnimationSystem::update(CrsScene &scene, float dt) {
        for (auto &kv : scene.getGameObjects()) {
            auto &obj = kv.second;
            if (!obj.characterAnimation || !obj.animator || !obj.parentId) continue;
            CrsGameObject *body = scene.findGameObjectById(*obj.parentId);
            if (body == nullptr || !body->rigidBody) continue;

            CharacterAnimationComponent &anims = *obj.characterAnimation;
            const RigidBodyComponent &rigidBody = *body->rigidBody;
            float speed = glm::length(glm::vec2{rigidBody.velocity.x, rigidBody.velocity.z});
            //correndo: mais rápido que o andar do personagem
            bool running = body->character && speed > body->character->moveSpeed * 1.1f;

            //no ar: conta o tempo. A queda entra no pulo (subindo) ou depois da espera; antes disso
            //continua a animação do chão (um tropeço de alguns frames não troca nada)
            anims.airTime = rigidBody.onGround ? 0.f : anims.airTime + dt;
            const bool jumped = rigidBody.velocity.y > 0.f;
            const bool falling = !rigidBody.onGround && (jumped || anims.airTime >= anims.fallDelay);

            if (falling) obj.animator->play(anims.fall, anims.fadeTime);
            else if (running) obj.animator->play(anims.run, anims.fadeTime);
            else if (speed > STOPPED_SPEED) obj.animator->play(anims.walk, anims.fadeTime);
            else obj.animator->play(anims.idle, anims.fadeTime);
        }
    }
}
