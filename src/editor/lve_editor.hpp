#pragma once

#include "engine/core/lve_input.hpp"
#include "engine/rendering/lve_camera.hpp"
#include "engine/scene/lve_scene.hpp"
#include "editor/orbit_camera_controller.hpp"

#include <optional>
#include <vector>
#include <map>

namespace lve {
    //ferramentas do editor (seleção; depois painéis e gizmo): o FirstApp só chama
    class LveEditor {
        public:
            //comesa o frame das feramentas
            void beginFrame();
            //camera do editor 
            void updateCamera(LveWindow &window, LveCamera &camera);
            //ponto de vista da camera do editor
            glm::vec3 getFocusPoint() const;
            //clique esquerdo seleciona o objeto sob o cursor (fora das janelas e do gizmo)
            void updateSelection(LveScene &scene, const LveInput &input, const LveCamera &camera, VkRect2D sceneArea);
            //objeto selecionado, buscado pelo id (nullptr = nenhum, ou já não existe)
            LveGameObject* getSelected(LveScene &scene) const;
            //painel lateral preso a direita
            void drawSidePanel(LveScene &scene);
            //pedaço da tela que sobra para cena ao lado do painel
            VkRect2D sceneArea(VkExtent2D screen) const;
            //setas de mover objetos
            void drawTransformGizmo(LveScene & scene, const LveCamera &camera, VkRect2D sceneArea);
        
        private:
            //objeto selecionado (vazio = nenhum)
            std::optional<LveGameObject::id_t> selectedId;
            //filhos de cada objeto
            using ChildrenMap = std::map<LveGameObject::id_t, std::vector<LveGameObject::id_t>>;
            //lista de objetos da cena em árvore
            void drawOutLiner(LveScene &scene);
            //uma linha do outliner e, se aberta, os filhos dela
            void drawOutLinerNode(LveScene &scene, LveGameObject::id_t id, const ChildrenMap &children);
            //camera orbital
            OrbitCameraController cameraController{};
            //janela de demonstração do ImGui (todos os componentes), aberta pelo painel
            bool showImGuiDemo = false;
    };
}