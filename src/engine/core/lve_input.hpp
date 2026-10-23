#pragma once

#include "engine/core/lve_window.hpp"

#include <array>
#include <glm/glm.hpp>

namespace lve {

    //estado do teclado num frame: guarda o frame atual e o anterior, pra quem usa poder
    //perguntar "apertou agora" sem cada um guardar o próprio "...WasDown"
    //(as teclas são as constantes GLFW_KEY_*)
    class LveInput {
        public:
            //chamar UMA vez por frame: o atual vira o anterior e lê o novo atual
            void update(const LveWindow &window);

            //está apertada agora (segurar conta em todo frame)
            bool isDown(int key) const;
            //apertou neste frame (solta -> apertada): só um frame por aperto
            bool wasPressed(int key) const;
            //soltou neste frame (apertada -> solta)
            bool wasReleased(int key) const;
            //botões do mouse (GLFW_MOUSE_BUTTON_*): mesma ideia das teclas
            bool isMouseDown(int button) const;
            bool wasMousePressed(int button) const;
            //posição do cursor na janela, em pixels (origem no canto de cima, à esquerda)
            glm::vec2 getMousePosition() const;

        private:
            std::array<bool, GLFW_KEY_LAST + 1> current{};
            std::array<bool, GLFW_KEY_LAST + 1> previous{};
            std::array<bool, GLFW_MOUSE_BUTTON_LAST + 1> currentMouse{};
            std::array<bool, GLFW_MOUSE_BUTTON_LAST + 1> previousMouse{};
            glm::vec2 mousePosition{0.f};
    };
}
