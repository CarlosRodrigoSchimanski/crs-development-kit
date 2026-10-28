#include "engine/gameplay/camera_rig_system.hpp"

namespace lve {
    void CameraRigSystem::update(LveScene &scene, glm::vec2 mouseDelta) {
        for (auto &kv : scene.getGameObjects()) {
            auto &obj = kv.second;
            if (!obj.cameraRig) continue;
            const CameraRigComponent &rig = *obj.cameraRig;

            //mouse pra direita gira a vista pra direita (yaw); mouse pra cima olha pra cima (pitch)
            obj.transform.rotation.y -= mouseDelta.x * rig.sensitivity;
            obj.transform.rotation.x -= mouseDelta.y * rig.sensitivity;

            //limite de inclinação: não passa por cima nem por baixo
            obj.transform.rotation.x = glm::clamp(obj.transform.rotation.x, rig.minPitch, rig.maxPitch);
        }
    }

    void CameraRigSystem::placeCamera(LveScene &scene) {
        for (auto &kv : scene.getGameObjects()) {
            auto &pivot = kv.second;
            if (!pivot.cameraRig) continue;
            const CameraRigComponent &rig = *pivot.cameraRig;

            glm::mat4 pivotWorld = scene.worldMatrix(pivot);
            //o raio sai do pivô e vai pra trás dele (o +Z no mundo, normalizado: a distância sai em metros)
            Ray ray{glm::vec3{pivotWorld[3]}, glm::normalize(glm::vec3{pivotWorld[2]})};

            //bateu em algo antes da distância cheia: a câmera fica antes do ponto (ignora o corpo do pai)
            float distance = rig.distance;
            if (auto hit = scene.raycast(ray, rig.distance, pivot.parentId)) {
                distance = glm::max(hit->distance - rig.wallMargin, 0.f);
            }

            //as câmeras filhas do pivô ficam nessa distância, no +Z local dele
            for (auto &childKv : scene.getGameObjects()) {
                auto &child = childKv.second;
                if (child.parentId == pivot.getId() && child.camera) {
                    child.transform.translation = {0.f, 0.f, distance};
                }
            }
        }
    }
}