#include "editor/orbit_camera_controller.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace crsdk {

    void OrbitCameraController::update(CrsWindow &crsWindow, CrsCamera &camera) {
        GLFWwindow *window = crsWindow.getGLFWwindow();

        //se o mouse está interagindo com alguma janela/botão do ImGui, a câmera não
        //deve orbitar/panear ao mesmo tempo — senão arrastar um slider da UI também giraria a cena
        bool uiWantsMouse = ImGui::GetIO().WantCaptureMouse;

        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        //primeiro frame: só guarda a posição, sem calcular delta (senão o salto do (0,0) até
        //a posição real do mouse seria lido como um arrasto gigante)
        if (firstMouse) {
            lastMouseX = mouseX;
            lastMouseY = mouseY;
            firstMouse = false;
        }

        double deltaX = mouseX - lastMouseX;
        double deltaY = mouseY - lastMouseY;
        lastMouseX = mouseX;
        lastMouseY = mouseY;

        bool middleDown = !uiWantsMouse && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
        bool shiftDown = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;

        //direção do alvo até a câmera, a partir dos ângulos atuais (antes de atualizar) —
        //usada pra calcular os eixos direita/cima da câmera, pro pan
        const glm::vec3 worldUp{0.f, 1.f, 0.f};
        glm::vec3 orbitDir = orbitDirection();
        glm::vec3 right = glm::normalize(glm::cross(worldUp, orbitDir));
        glm::vec3 camUp = glm::cross(orbitDir, right);

        if (middleDown && shiftDown) {
            //pan: desloca o alvo no plano direita/cima da câmera; escala pela distância,
            //pra sentir igual (não mais rápido/lento) em qualquer nível de zoom
            target -= right * static_cast<float>(deltaX) * panSpeed * distance;
            target += camUp * static_cast<float>(deltaY) * panSpeed * distance;
        } else if (middleDown) {
            //orbita: arrastar o mouse gira a câmera em torno do alvo
            yaw -= static_cast<float>(deltaX) * orbitSpeed;
            pitch += static_cast<float>(deltaY) * orbitSpeed;
            pitch = glm::clamp(pitch, -maxPitch, maxPitch);
        }

        //zoom: roda do mouse aproxima/afasta a câmera do alvo
        //sempre consome o delta (senão ele se acumula e "explode" quando a UI libera o mouse),
        //mas só aplica no zoom se o scroll não era destinado à UI
        float scrollDelta = crsWindow.consumeScrollDelta();
        if (!uiWantsMouse) {
            distance -= scrollDelta * zoomSpeed;
            distance = glm::max(distance, minDistance);
        }

        //enquadrar em andamento: o usuário mexeu na câmera, ele manda (a animação para onde está)
        if (middleDown || (!uiWantsMouse && scrollDelta != 0.f)) framingProgress = 1.f;
        const double now = glfwGetTime();
        const float dt = lastTime < 0.0 ? 0.f : static_cast<float>(now - lastTime);
        lastTime = now;
        advanceFrame(dt);

        //recalcula a direção já com os ângulos atualizados, pra posicionar a câmera de fato
        glm::vec3 position = target + distance * orbitDirection();
        camera.setViewTarget(position, target);
    }

    //direção do alvo até a câmera, a partir de yaw/pitch (vetor unitário na esfera em volta do alvo)
    glm::vec3 OrbitCameraController::orbitDirection() const {
        return {
            glm::cos(pitch) * glm::sin(yaw),
            glm::sin(pitch),
            glm::cos(pitch) * glm::cos(yaw)
        };
    }

    //quanto dura a ida até o objeto enquadrado
    constexpr float FRAME_SECONDS = 0.3f;
    //folga: a esfera ocupa ~90% da altura da tela
    constexpr float FRAME_MARGIN = 1.1f;

    float OrbitCameraController::frameDistance(float radius, float verticalFov) {
        //a esfera encosta nas bordas de cima e de baixo quando distância = raio / seno(meio campo de visão)
        return std::max(radius * FRAME_MARGIN / std::sin(verticalFov * 0.5f), 1.f);
    }

    void OrbitCameraController::frame(glm::vec3 center, float radius, float verticalFov) {
        frameFromTarget = target;
        frameFromDistance = distance;
        frameToTarget = center;
        frameToDistance = std::max(frameDistance(radius, verticalFov), minDistance);
        framingProgress = 0.f;
    }

    void OrbitCameraController::advanceFrame(float dt) {
        if (framingProgress >= 1.f) return;
        framingProgress = std::min(framingProgress + dt / FRAME_SECONDS, 1.f);
        //sai rápido e chega devagar (sem tranco no fim)
        const float t = 1.f - (1.f - framingProgress) * (1.f - framingProgress) * (1.f - framingProgress);
        target = glm::mix(frameFromTarget, frameToTarget, t);
        distance = glm::mix(frameFromDistance, frameToDistance, t);
    }

    void OrbitCameraController::setOrbitDirection(glm::vec3 direction) {
        direction = glm::normalize(direction);
        //o inverso do orbitDirection: o pitch sai do y, o yaw do x e do z
        pitch = glm::clamp(glm::asin(glm::clamp(direction.y, -1.f, 1.f)), -maxPitch, maxPitch);
        yaw = glm::atan(direction.x, direction.z);
    }

}
