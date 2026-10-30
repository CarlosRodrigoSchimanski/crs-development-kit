#include "engine/core/crs_window.hpp"

#include <stdexcept>

namespace crsdk {

    CrsWindow::CrsWindow(int w, int h, std::string name) : width{w}, height{h}, windowName{name} {
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

    CrsWindow::~CrsWindow() {
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    bool CrsWindow::shouldClose() const {
        return glfwWindowShouldClose(window);
    }

    bool CrsWindow::wasWindowResized() const {
        return framebufferResized;
    }

    void CrsWindow::waitWhileMinimized() {
        //glfwWaitEvents dorme até chegar um evento; o framebufferResizedCallback atualiza width/height
        while (width == 0 || height == 0) {
            glfwWaitEvents();
        }
    }

    void CrsWindow::resetWindowResizedFlag(){
        framebufferResized = false;
    }

    GLFWwindow * CrsWindow::getGLFWwindow() const{
        return window;
    };

    float CrsWindow::consumeScrollDelta() {
        float delta = scrollDeltaY;
        scrollDeltaY = 0.f;
        return delta;
    }

    VkExtent2D CrsWindow::getExtent() const {
        return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    }

    void CrsWindow::createWindowSurface(VkInstance instance, VkSurfaceKHR *surface) {
        if (glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }
    }

    void CrsWindow::setRefreshCallback(std::function<void()> callback) {
        onRefresh = std::move(callback);
    }

    void CrsWindow::framebufferResizedCallback(GLFWwindow *window,int width, int height){
        //o user pointer é o próprio CrsWindow guardado como void* no construtor; static_cast desfaz isso
        auto crsWindow = static_cast<CrsWindow*>(glfwGetWindowUserPointer(window));
        crsWindow->framebufferResized = true;
        crsWindow->width = width;
        crsWindow->height = height;
    }

    void CrsWindow::scrollCallback(GLFWwindow *window, double /*xoffset*/, double yoffset) {
        auto crsWindow = static_cast<CrsWindow*>(glfwGetWindowUserPointer(window));
        crsWindow->scrollDeltaY += static_cast<float>(yoffset);
    }

    void CrsWindow::windowRefreshCallback(GLFWwindow *window) {
        auto crsWindow = static_cast<CrsWindow*>(glfwGetWindowUserPointer(window));
        if (crsWindow->onRefresh) {
            crsWindow->onRefresh();
        }
    }

    void CrsWindow::setCursorMode(CursorMode mode) {
        int glfwMode = GLFW_CURSOR_NORMAL;
        if (mode == CursorMode::Hidden) glfwMode = GLFW_CURSOR_HIDDEN;
        if (mode == CursorMode::Captured) glfwMode = GLFW_CURSOR_DISABLED;
        glfwSetInputMode(window, GLFW_CURSOR, glfwMode);

        if (glfwRawMouseMotionSupported()) {
            glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, mode == CursorMode::Captured ? GLFW_TRUE : GLFW_FALSE);
        }
    }
}