#pragma once

#include "engine/core/lve_window.hpp"

#include <array>

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

        private:
            std::array<bool, GLFW_KEY_LAST + 1> current{};
            std::array<bool, GLFW_KEY_LAST + 1> previous{};
    };
}
