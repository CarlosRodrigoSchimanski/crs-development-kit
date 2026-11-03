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

            //enquadrar (Del do teclado numérico, como o "View Selected" do Blender): leva o alvo pro
            //centro de uma esfera e afasta o suficiente pra ela caber na tela, mantendo o ângulo de
            //vista. A ida é animada; mexer na câmera no meio do caminho cancela
            void frame(glm::vec3 center, float radius, float verticalFov);
            //a distância pra uma esfera de 'radius' caber na tela (com uma folga em volta)
            static float frameDistance(float radius, float verticalFov);
            //avança a animação do enquadrar (o update já chama; separado pros testes)
            void advanceFrame(float dt);
            bool framing() const { return framingProgress < 1.f; }

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

            //animação do enquadrar: de onde saiu, pra onde vai, quanto já andou (0..1; 1 = parado)
            glm::vec3 frameFromTarget{0.f}, frameToTarget{0.f};
            float frameFromDistance{0.f}, frameToDistance{0.f};
            float framingProgress{1.f};
            double lastTime{-1.0};

            bool firstMouse{true};
            double lastMouseX{0.0};
            double lastMouseY{0.0};
    };

}
