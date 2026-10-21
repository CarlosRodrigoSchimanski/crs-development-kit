#include "engine/gameplay/character_controller_system.hpp"

namespace lve {

    void CharacterControllerSystem::update(LveScene &scene, const CharacterInput &input) {
        //direção so no chão, e no maximo comprimento 1 (normalinando o vetor)
        glm::vec3 direction{input.moveDirection.x, 0.f, input.moveDirection.z};
        float length = glm::length(direction);

        if (length > 1.f) direction /= length;

        for (auto &kv : scene.getGameObjects()) {
            auto &obj = kv.second;
            if (!obj.character || !obj.rigidBody) continue;

            //so a velocidade horizontal: a vertical é a gravidade
            obj.rigidBody->velocity.x = direction.x * obj.character->moveSpeed;
            obj.rigidBody->velocity.z = direction.z * obj.character->moveSpeed;

        }
        
    }
}