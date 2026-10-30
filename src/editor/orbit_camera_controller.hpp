#pragma once

#include "engine/core/crs_window.hpp"
#include "engine/rendering/crs_camera.hpp"

namespace crsdk {

    //controla a câmera no estilo de navegação do Blender:
    //botão do meio do mouse arrasta = orbita em torno do alvo
    //shift + botão do meio arrasta = pan (desloca o alvo)
    //roda do mouse = zoom (aproxima/afasta do alvo)
    class OrbitCameraController {
        public:
            void update(CrsWindow &crsWindow, CrsCamera &camera);

            //aponta a câmera numa direção (do alvo até a câmera), mantendo o alvo e a distância
            void setOrbitDirection(glm::vec3 direction);

            glm::vec3 target{0.f, 0.f, 0.f};
            float distance{6.f};
            float yaw{0.f};
            float pitch{0.3f};

            float orbitSpeed{0.01f};
            float panSpeed{0.002f};
            float zoomSpeed{0.5f};
            float minDistance{0.5f};
            float maxPitch{1.5f};

        private:
            glm::vec3 orbitDirection() const;

            bool firstMouse{true};
            double lastMouseX{0.0};
            double lastMouseY{0.0};
    };

}
