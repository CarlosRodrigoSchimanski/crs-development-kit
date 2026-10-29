#include "engine/gameplay/character_animation_system.hpp"

namespace lve {
    //abaixo disso conta como parado (unidades por segundo)
    constexpr float STOPPED_SPEED = 0.1f;

    void CharacterAnimationSystem::update(LveScene &scene) {
        for (auto &kv : scene.getGameObjects()) {
            auto &obj = kv.second;
            if (!obj.characterAnimation || !obj.animator || !obj.parentId) continue;
            LveGameObject *body = scene.findGameObjectById(*obj.parentId);
            if (body == nullptr || !body->rigidBody) continue;

            const CharacterAnimationComponent &anims = *obj.characterAnimation;
            const RigidBodyComponent &rigidBody = *body->rigidBody;
            float speed = glm::length(glm::vec2{rigidBody.velocity.x, rigidBody.velocity.z});
            //correndo: mais rápido que o andar do personagem
            bool running = body->character && speed > body->character->moveSpeed * 1.1f;

            if (!rigidBody.onGround) obj.animator->play(anims.fall, anims.fadeTime);
            else if (running) obj.animator->play(anims.run, anims.fadeTime);
            else if (speed > STOPPED_SPEED) obj.animator->play(anims.walk, anims.fadeTime);
            else obj.animator->play(anims.idle, anims.fadeTime);
        }
    }
}
