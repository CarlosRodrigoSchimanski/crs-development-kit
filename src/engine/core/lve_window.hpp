#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <string>

namespace lve {

    class LveWindow {
        public:
            // Construtor e destrutor de janela
            LveWindow(int w, int h, std::string name);
            ~LveWindow();

            //codigo para evitar duplicação de ponteiros
            LveWindow(const LveWindow&) = delete;
            LveWindow& operator=(const LveWindow&) = delete;

            // Função para verificar se a janela deve ser fechada
            bool shouldClose();

            VkExtent2D getExtent();

            bool wasWindowResized();
            void resetWindowResizedFlag();
            GLFWwindow * getGLFWwindow() const;

            //devolve o deslocamento acumulado da roda do mouse desde a última chamada, e zera o acumulador
            float consumeScrollDelta();

            //cria a VkSurfaceKHR ligada a esta janela (usado pelo LveDevice)
            void createWindowSurface(VkInstance instance, VkSurfaceKHR *surface);

        private:
            static void framebufferResizedCallback(GLFWwindow *window,int width, int heigth);
            //GLFW só avisa roda do mouse via callback, não tem como "perguntar" o valor como as teclas
            static void scrollCallback(GLFWwindow *window, double xoffset, double yoffset);

            // Variáveis de membro
            int width;
            int height;
            bool framebufferResized = false;
            float scrollDeltaY = 0.f;

            std::string windowName;
            GLFWwindow* window;
    };
}
