#include "engine/core/lve_window.hpp"

#include <stdexcept>

namespace lve {
    LveWindow::LveWindow(int w, int h, std::string name) : width{w}, height{h}, windowName{name} {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        window = glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr);
        glfwSetWindowUserPointer(window,this);
        glfwSetFramebufferSizeCallback(window,framebufferResizedCallback);
        glfwSetScrollCallback(window, scrollCallback);
    }

    LveWindow::~LveWindow() {
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    bool LveWindow::shouldClose() {
        return glfwWindowShouldClose(window);
    }

    bool LveWindow::wasWindowResized(){
        return framebufferResized;
    }

    void LveWindow::resetWindowResizedFlag(){
        framebufferResized = false;
    }

    GLFWwindow * LveWindow::getGLFWwindow() const{
        return window;
    };

    float LveWindow::consumeScrollDelta() {
        float delta = scrollDeltaY;
        scrollDeltaY = 0.f;
        return delta;
    }

    VkExtent2D LveWindow::getExtent() {
        return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    }

    void LveWindow::createWindowSurface(VkInstance instance, VkSurfaceKHR *surface) {
        if (glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }
    }

    void LveWindow::framebufferResizedCallback(GLFWwindow *window,int width, int height){
        auto lveWindow = reinterpret_cast<LveWindow*>(glfwGetWindowUserPointer(window));
        lveWindow->framebufferResized = true;
        lveWindow->width = width;
        lveWindow->height = height;
    }

    void LveWindow::scrollCallback(GLFWwindow *window, double xoffset, double yoffset) {
        auto lveWindow = reinterpret_cast<LveWindow*>(glfwGetWindowUserPointer(window));
        lveWindow->scrollDeltaY += static_cast<float>(yoffset);
    }
}