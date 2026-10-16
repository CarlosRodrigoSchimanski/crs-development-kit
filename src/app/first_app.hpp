#pragma once

#include "engine/core/lve_window.hpp"
#include "engine/core/lve_device.hpp"
#include "engine/core/lve_input.hpp"
#include "engine/scene/lve_scene.hpp"
#include "engine/rendering/lve_renderer.hpp"
#include "engine/rendering/lve_descriptors.hpp"

#include <memory>
#include <vector>
#include <optional>

namespace lve {
    //aplicação: dona da janela, do device e da cena, e roda o loop principal
    class FirstApp {
        public:
            //tamanho inicial da janela
            static constexpr int WIDTH = 800;
            static constexpr int HEIGHT = 600;

            FirstApp();

            //o app é dono de recursos Vulkan: copiar duplicaria os handles
            FirstApp(const FirstApp&) = delete;
            FirstApp& operator=(const FirstApp&) = delete;

            //loop principal: roda até a janela ser fechada
            void run();

        private:
            //monta a cena inicial (objetos, modelos)
            void loadGameObjects();
            //play: copia a cena do editor e passa a rodar a copia
            void startPlay();
            //stop: descarta a cena de jogo e volta a rodar a cena do editor
            void stopPlay();
            //le P = play / ' (tecla à esquerda do 1) = stop
            void updatePlayMode();
            //a sena que esta ativa
            LveScene& activeScene();

            //ordem importa: membros são criados de cima pra baixo e destruídos de baixo pra cima,
            //e cada um depende dos de cima (o device precisa da janela pra criar a surface, etc.)
            LveWindow lveWindow{WIDTH, HEIGHT, "Olá Vulkan"};
            LveDevice lveDevice{lveWindow};
            LveRenderer lveRenderer{lveWindow, lveDevice};
            //descriptors globais (UBO + shadow map), um set por frame em voo
            std::unique_ptr<LveDescriptorPool> globalPool{};
            //a cena: todos os objetos, indexados pelo id
            LveScene lveScene;
            //modo start/stop: do modo jogo
            std::optional<LveScene> playScene;
            //teclado do frame atual e do anterior (atualizado no começo de cada frame)
            LveInput lveInput;
    };
}