#include "engine/physics/lve_physics_system.hpp"

namespace lve {
    void PhysicsSystem::update(LveScene &scene, float dt) {
        auto& objects = scene.getGameObjects();

        for (auto& kv : objects) {
            auto& obj = kv.second;
            if (!obj.rigidBody) continue; //caso seja nulo
            if (obj.rigidBody->type == BodyType::Static) continue; //caso seja estático
            obj.rigidBody->onGround = false;

            //caso seja dinâmico: a gravidade aumenta a velocidade, e a velocidade move o objeto
            obj.rigidBody->velocity += gravity * dt;
            obj.transform.translation += obj.rigidBody->velocity * dt;

            //colisão: confere a caixa deste objeto contra a de cada corpo estático
            glm::vec3 half = obj.rigidBody->halfExtents * obj.transform.scale;
            for (auto& otherKv : objects) {
                auto& other = otherKv.second;
                if (!other.rigidBody) continue;
                if (other.rigidBody->type != BodyType::Static) continue;

                glm::vec3 otherHalf = other.rigidBody->halfExtents * other.transform.scale;
                //distância entre os centros, em cada eixo (abs = sempre positiva)
                glm::vec3 distance = glm::abs(obj.transform.translation - other.transform.translation);
                bool touching = distance.x < half.x + otherHalf.x
                            && distance.y < half.y + otherHalf.y
                            && distance.z < half.z + otherHalf.z;
                if (!touching) continue;

                //estava caindo (+Y é pra baixo): apoia a base em cima do estático e para de cair
                if (obj.rigidBody->velocity.y > 0.f) {
                    obj.transform.translation.y = other.transform.translation.y - otherHalf.y - half.y;
                    obj.rigidBody->velocity.y = 0.f;
                    obj.rigidBody->onGround = true;
                }
            }
        }
    }

}