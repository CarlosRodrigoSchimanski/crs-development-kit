#include "engine/gameplay/character_controller_system.hpp"

#include <glm/gtc/constants.hpp>
#include <cmath>

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

            //pulo: só apoiado no chão (velocidade pra cima = y negativo, o Y aponta pra baixo)
            if (input.jump && obj.rigidBody->onGround) {
                obj.rigidBody->velocity.y = -obj.character->jumpSpeed;
                obj.rigidBody->onGround = false;
            }
        }   
    }

    float turnTowards(float current, float target, float maxStep) {
        float difference = glm::mod(target - current + glm::pi<float>(), glm::two_pi<float>()) - glm::pi<float>();
        if (std::abs(difference) <= maxStep) return current + difference;
        return current + (difference >0.f ? maxStep : -maxStep);
    }

    void CharacterControllerSystem::faceMovement(LveScene &scene, float dt) {
        for (auto &kv : scene.getGameObjects()) {
            auto &obj = kv.second;
            if (!obj.faceMovement || !obj.parentId) continue;
            LveGameObject *parent = scene.findGameObjectById(*obj.parentId);
            if (!parent || !parent->rigidBody) continue;

            glm::vec2 velocity{parent->rigidBody->velocity.x, parent->rigidBody->velocity.z};
            if (glm::length(velocity) < 0.01f) continue;

            float target = std::atan2(velocity.x, velocity.y) + obj.faceMovement->yawOffset;
            obj.transform.rotation.y = turnTowards(obj.transform.rotation.y, target, obj.faceMovement->turnSpeed * dt);
        }
    }
}