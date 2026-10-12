#pragma once

#include "engine/core/lve_window.hpp"
#include "engine/core/lve_device.hpp"
#include "engine/scene/lve_game_object.hpp"
#include "engine/rendering/lve_renderer.hpp"
#include "engine/rendering/lve_descriptors.hpp"

#include <memory>
#include <vector>

namespace lve {
    // Classe FirstApp esta no mesmo nivel que a classe lvewindow
    class FirstApp {
        public:
            // Constantes de largura e altura da janela
            static constexpr int WIDTH = 800;
            static constexpr int HEIGHT = 600;

            // Construtor e destrutor da classe FirstApp
            FirstApp();
            ~FirstApp();

            //codigo para evitar duplicação de ponteiros
            FirstApp(const FirstApp&) = delete;
            FirstApp& operator=(const FirstApp&) = delete;

            //função run() que será responsável por executar o loop principal do aplicativo
            void run();

        private:
            //função loadModel() que será responsável por carregar o modelo 3D
            void loadGameObjects();

            // Instancia da classe LveWindow
            LveWindow lveWindow{WIDTH, HEIGHT, "Olá Vulkan"};
            // Instancia da classe LveDevice: precisa da janela pra criar a surface
            LveDevice lveDevice{lveWindow};
            // instancia da classe LveRenderer, que é responsável por gerenciar a renderização
            LveRenderer lveRenderer{lveWindow, lveDevice};
            // Instancia da classe LveDescriptorPool, que é responsável por gerenciar os descritores
            std::unique_ptr<LveDescriptorPool> globalPool{};
            // Instancia da classe LveModel, que é responsável pelo modelo 3D a ser renderizado
            LveGameObject::Map gameObjects;

    };
}