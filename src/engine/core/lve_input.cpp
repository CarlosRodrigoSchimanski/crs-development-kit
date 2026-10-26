#include "engine/core/lve_input.hpp"

namespace lve {

    void LveInput::update(const LveWindow &window) {
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

    bool LveInput::isDown(int key) const {
        return current[key];
    }

    bool LveInput::wasPressed(int key) const {
        return current[key] && !previous[key];
    }

    bool LveInput::wasReleased(int key) const {
        return !current[key] && previous[key];
    }

    bool LveInput::isMouseDown(int button) const {
        return currentMouse[button];
    }

    bool LveInput::wasMousePressed(int button) const {
        return currentMouse[button] && !previousMouse[button];
    }

    glm::vec2 LveInput::getMousePosition() const {
        return mousePosition;
    }

    glm::vec2 LveInput::getMouseDelta() const {
        return mouseDelta;
    }

    void LveInput::skipNextMouseDelta() {
        skipMouseDelta = true;
    }
}
