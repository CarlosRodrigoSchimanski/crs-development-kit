#include "engine/physics/lve_physics_system.hpp"

namespace lve {
    void PhysicsSystem::update(LveScene &scene, float dt) {
        for (auto& kv : scene.getGameObjects()) {
            auto& obj = kv.second;
            if(!obj.rigidBody) continue; //caso seja nulo
            if(obj.rigidBody->type == BodyType::Static)continue;//caso seja statico

            //caso seja dinamico
            obj.rigidBody->velocity += gravity * dt;
            obj.transform.translation += obj.rigidBody->velocity * dt;
        }
    }
}