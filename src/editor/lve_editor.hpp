#pragma once

#include "engine/core/lve_input.hpp"
#include "engine/rendering/lve_camera.hpp"
#include "engine/scene/lve_scene.hpp"

#include <optional>

namespace lve {
    //ferramentas do editor (seleção; depois painéis e gizmo): o FirstApp só chama
    class LveEditor {
        public:
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
    };
}