#include "engine/core/crs_input.hpp"

namespace crsdk {

    void CrsInput::update(const CrsWindow &window) {
        previous = current;
        //começa no GLFW_KEY_SPACE: o glfwGetKey dá erro pra códigos abaixo dele
        for (int key = GLFW_KEY_SPACE; key <= GLFW_KEY_LAST; key++) {
            current[key] = glfwGetKey(window.getGLFWwindow(), key) == GLFW_PRESS;
        }

        previousMouse = currentMouse;
        for (int button = 0; button <= GLFW_MOUSE_BUTTON_LAST; button++) {
            currentMouse[button] = glfwGetMouseButton(window.getGLFWwindow(), button) == GLFW_PRESS;
        }

        double x = 0.0;
        double y = 0.0;
        glfwGetCursorPos(window.getGLFWwindow(), &x, &y);

        glm::vec2 newPosition{static_cast<float>(x), static_cast<float>(y)};
        mouseDelta = skipMouseDelta ? glm::vec2{0.f} : newPosition - mousePosition;
        skipMouseDelta = false;
        mousePosition = newPosition;
    }

    bool CrsInput::isDown(int key) const {
        return current[key];
    }

    bool CrsInput::wasPressed(int key) const {
        return current[key] && !previous[key];
    }

    bool CrsInput::wasReleased(int key) const {
        return !current[key] && previous[key];
    }

    bool CrsInput::isMouseDown(int button) const {
        return currentMouse[button];
    }

    bool CrsInput::wasMousePressed(int button) const {
        return currentMouse[button] && !previousMouse[button];
    }

    glm::vec2 CrsInput::getMousePosition() const {
        return mousePosition;
    }

    glm::vec2 CrsInput::getMouseDelta() const {
        return mouseDelta;
    }

    void CrsInput::skipNextMouseDelta() {
        skipMouseDelta = true;
    }
}
