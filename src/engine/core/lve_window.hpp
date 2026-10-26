#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <functional>
#include <string>

namespace lve {
    enum class CursorMode {Visible, Hidden, Captured};

    class LveWindow {
        public:
            // Construtor e destrutor de janela
            LveWindow(int w, int h, std::string name);
            ~LveWindow();

            //não copiável: a cópia teria o mesmo GLFWwindow* e as duas destruiriam a mesma janela.
            //apagar a cópia já impede o move também (o compilador não gera move sozinho nesse caso)
            LveWindow(const LveWindow&) = delete;
            LveWindow& operator=(const LveWindow&) = delete;

            // Função para verificar se a janela deve ser fechada
            bool shouldClose() const;

            VkExtent2D getExtent() const;

            bool wasWindowResized() const;
            //bloqueia enquanto a janela estiver minimizada (tamanho 0): não existe swap chain de tamanho 0
            void waitWhileMinimized();
            void resetWindowResizedFlag();
            GLFWwindow * getGLFWwindow() const;

            void setCursorMode(CursorMode mode);

            //devolve o deslocamento acumulado da roda do mouse desde a última chamada, e zera o acumulador
            float consumeScrollDelta();

            //cria a VkSurfaceKHR ligada a esta janela (usado pelo LveDevice)
            void createWindowSurface(VkInstance instance, VkSurfaceKHR *surface);

            //chamado quando o SO pede pra janela ser redesenhada (ex: durante o redimensionamento).
            //A janela não sabe desenhar: só repassa o aviso pra quem registrou. nullptr desregistra
            void setRefreshCallback(std::function<void()> callback);

        private:
            static void framebufferResizedCallback(GLFWwindow *window,int width, int height);
            //GLFW só avisa roda do mouse via callback, não tem como "perguntar" o valor como as teclas
            static void scrollCallback(GLFWwindow *window, double xoffset, double yoffset);
            static void windowRefreshCallback(GLFWwindow *window);

            // Variáveis de membro
            int width;
            int height;
            bool framebufferResized = false;
            float scrollDeltaY = 0.f;
            std::function<void()> onRefresh;

            std::string windowName;
            GLFWwindow* window;
    };
}
