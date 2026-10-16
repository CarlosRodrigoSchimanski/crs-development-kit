#include "engine/core/lve_input.hpp"

namespace lve {

    void LveInput::update(const LveWindow &window) {
        previous = current;
        //começa no GLFW_KEY_SPACE: o glfwGetKey dá erro pra códigos abaixo dele
        for (int key = GLFW_KEY_SPACE; key <= GLFW_KEY_LAST; key++) {
            current[key] = glfwGetKey(window.getGLFWwindow(), key) == GLFW_PRESS;
        }
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
}
