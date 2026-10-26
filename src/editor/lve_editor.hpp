#pragma once

#include "engine/core/lve_input.hpp"
#include "engine/rendering/lve_camera.hpp"
#include "engine/scene/lve_scene.hpp"
#include "editor/orbit_camera_controller.hpp"

#include <optional>
#include <vector>
#include <map>

namespace lve {
    //oq as setas do gizmo fazem
    enum class GizmoMode {Move, Rotate, Scale};

    //ferramentas do editor (seleção; depois painéis e gizmo): o FirstApp só chama
    class LveEditor {
        public:
            //começa o frame das ferramentas (logo depois do newFrame do ImGui)
            void beginFrame();
            //câmera do editor: a orbital, controlada pelo mouse
            void updateCamera(LveWindow &window, LveCamera &camera);
            //ponto que a câmera do editor olha (a caixa do sol fica em volta dele)
            glm::vec3 getFocusPoint() const;
            //G move, R gira, S escala (como no Blender), fora dos campos de texto
            void updateGizmoMode(const LveInput &input);
            //clique esquerdo seleciona o objeto sob o cursor (fora das janelas e do gizmo)
            void updateSelection(LveScene &scene, const LveInput &input, const LveCamera &camera, VkRect2D sceneArea);
            //objeto selecionado, buscado pelo id (nullptr = nenhum, ou já não existe)
            LveGameObject* getSelected(LveScene &scene) const;
            //pedaço da tela que sobra pra cena ao lado do painel
            VkRect2D sceneArea(VkExtent2D screen) const;
            //toda a interface do editor do frame: painel, setas e gizmo de navegação
            void drawUi(LveScene &scene, const LveCamera &camera, VkRect2D sceneArea);

        private:
            //painel lateral preso à direita (outliner, propriedades, ferramentas)
            void drawSidePanel(LveScene &scene);
            //setas de mover/girar/escalar no objeto selecionado (ImGuizmo)
            void drawTransformGizmo(LveScene &scene, const LveCamera &camera, VkRect2D sceneArea);
            //bolinhas dos eixos no canto da cena, como no Blender: clicar põe a câmera daquele lado
            void drawNavigationGizmo(const LveCamera &camera, VkRect2D sceneArea);

            //objeto selecionado (vazio = nenhum)
            std::optional<LveGameObject::id_t> selectedId;
            //modo atual das setas do gizmo
            GizmoMode gizmoMode = GizmoMode::Move;
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