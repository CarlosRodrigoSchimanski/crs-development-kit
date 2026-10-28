#include "engine/gameplay/camera_rig_system.hpp"

namespace lve {
    void CameraRigSystem::update(LveScene &scene, glm::vec2 mouseDelta) {
        for (auto &kv : scene.getGameObjects()) {
            auto &obj = kv.second;
            if (!obj.cameraRig) continue;
            const CameraRigComponent &rig = *obj.cameraRig;

            //mouse pra direita gira a vista pra direita (yaw); mouse pra cima olha pra cima (pitch)
            obj.transform.rotation.y += mouseDelta.x * rig.sensitivity;
            obj.transform.rotation.x -= mouseDelta.y * rig.sensitivity;

            //limite de inclinação: não passa por cima nem por baixo
            obj.transform.rotation.x = glm::clamp(obj.transform.rotation.x, rig.minPitch, rig.maxPitch);
        }
    }
}