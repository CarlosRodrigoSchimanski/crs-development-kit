#include "engine/physics/crs_physics_system.hpp"

namespace crsdk {
    void PhysicsSystem::update(CrsScene &scene, float dt) {
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
            glm::vec3 boxCenter = obj.transform.translation + obj.rigidBody->center * obj.transform.scale;

            for (auto& otherKv : objects) {
                auto& other = otherKv.second;
                if (!other.rigidBody) continue;
                if (other.rigidBody->type != BodyType::Static) continue;

                glm::vec3 otherHalf = other.rigidBody->halfExtents * other.transform.scale;
                glm::vec3 otherCenter = other.transform.translation + other.rigidBody->center * other.transform.scale;
                //distância entre os centros, em cada eixo (abs = sempre positiva)
                glm::vec3 distance = glm::abs(boxCenter - otherCenter);
                bool touching = distance.x < half.x + otherHalf.x
                            && distance.y < half.y + otherHalf.y
                            && distance.z < half.z + otherHalf.z;
                if (!touching) continue;

                //estava caindo: apoia a base em cima do estático e para de cair
                if (obj.rigidBody->velocity.y < 0.f) {
                    //centro da caixa apoiado em cima do estático, devolvido pra origem do objeto
                    float boxCenterY = otherCenter.y + otherHalf.y + half.y;
                    obj.transform.translation.y = boxCenterY - obj.rigidBody->center.y * obj.transform.scale.y;
                    boxCenter.y = boxCenterY;
                    obj.rigidBody->velocity.y = 0.f;
                    obj.rigidBody->onGround = true;
                }
            }
        }
    }

}