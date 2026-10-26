#include "engine/core/lve_window.hpp"

#include <stdexcept>

namespace lve {

    LveWindow::LveWindow(int w, int h, std::string name) : width{w}, height{h}, windowName{name} {
        if (!glfwInit()) {
            throw std::runtime_error("failed to initialize GLFW!");
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        window = glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr);
        if (window == nullptr) {
            //o destrutor não roda se o construtor lança exceção: desfaz o glfwInit aqui mesmo
            glfwTerminate();
            throw std::runtime_error("failed to create GLFW window!");
        }
        glfwSetWindowUserPointer(window,this);
        glfwSetFramebufferSizeCallback(window,framebufferResizedCallback);
        glfwSetScrollCallback(window, scrollCallback);
        glfwSetWindowRefreshCallback(window, windowRefreshCallback);
    }

    LveWindow::~LveWindow() {
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    bool LveWindow::shouldClose() const {
        return glfwWindowShouldClose(window);
    }

    bool LveWindow::wasWindowResized() const {
        return framebufferResized;
    }

    void LveWindow::waitWhileMinimized() {
        //glfwWaitEvents dorme até chegar um evento; o framebufferResizedCallback atualiza width/height
        while (width == 0 || height == 0) {
            glfwWaitEvents();
        }
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

    VkExtent2D LveWindow::getExtent() const {
        return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    }

    void LveWindow::createWindowSurface(VkInstance instance, VkSurfaceKHR *surface) {
        if (glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }
    }

    void LveWindow::setRefreshCallback(std::function<void()> callback) {
        onRefresh = std::move(callback);
    }

    void LveWindow::framebufferResizedCallback(GLFWwindow *window,int width, int height){
        //o user pointer é o próprio LveWindow guardado como void* no construtor; static_cast desfaz isso
        auto lveWindow = static_cast<LveWindow*>(glfwGetWindowUserPointer(window));
        lveWindow->framebufferResized = true;
        lveWindow->width = width;
        lveWindow->height = height;
    }

    void LveWindow::scrollCallback(GLFWwindow *window, double /*xoffset*/, double yoffset) {
        auto lveWindow = static_cast<LveWindow*>(glfwGetWindowUserPointer(window));
        lveWindow->scrollDeltaY += static_cast<float>(yoffset);
    }

    void LveWindow::windowRefreshCallback(GLFWwindow *window) {
        auto lveWindow = static_cast<LveWindow*>(glfwGetWindowUserPointer(window));
        if (lveWindow->onRefresh) {
            lveWindow->onRefresh();
        }
    }

    void LveWindow::setCursorMode(CursorMode mode) {
        int glfwMode = GLFW_CURSOR_NORMAL;
        if (mode == CursorMode::Hidden) glfwMode = GLFW_CURSOR_HIDDEN;
        if (mode == CursorMode::Captured) glfwMode = GLFW_CURSOR_DISABLED;
        glfwSetInputMode(window, GLFW_CURSOR, glfwMode);

        if (glfwRawMouseMotionSupported()) {
            glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, mode == CursorMode::Captured ? GLFW_TRUE : GLFW_FALSE);
        }
    }
}