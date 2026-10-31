#pragma once

#include "engine/core/crs_input.hpp"
#include "engine/rendering/crs_camera.hpp"
#include "engine/scene/crs_scene.hpp"
#include "engine/scripting/crs_scripting.hpp"
#include "editor/orbit_camera_controller.hpp"

#include <optional>
#include <string>
#include <vector>
#include <map>

namespace crsdk {
    //oq as setas do gizmo fazem
    enum class GizmoMode {Move, Rotate, Scale};

    //o que dá pra criar pelo menu Adicionar
    enum class AddKind {None, Empty, Camera, PointLight};

    //o que foi pedido pelo menu neste frame (quem executa é o FirstApp)
    struct EditorRequests {
        bool openScene = false;
        bool saveScene = false;
        bool saveSceneAs = false;
        bool importModel = false;
        AddKind add = AddKind::None;
        bool quit = false;
        //ler a pasta dos componentes de novo (o Add Component foi aberto: pode ter arquivo novo ou editado)
        bool refreshComponents = false;
        //Generate Component: o nome da classe nova (vazio = nada pedido)
        std::string generateComponent{};
    };

    //ferramentas do editor (seleção; depois painéis e gizmo): o FirstApp só chama
    class CrsEditor {
        public:
            //começa o frame das ferramentas (logo depois do newFrame do ImGui)
            void beginFrame();
            //câmera do editor: a orbital, controlada pelo mouse
            void updateCamera(CrsWindow &window, CrsCamera &camera);
            //ponto que a câmera do editor olha (a caixa do sol fica em volta dele)
            glm::vec3 getFocusPoint() const;
            //vista da câmera do editor num arquivo à parte (não é parte do jogo, como o imgui.ini)
            void saveView(const std::string &filepath) const;
            //false = sem arquivo ou inválido: a vista fica como está
            bool loadView(const std::string &filepath);
            //atalhos do editor, como no Blender: G move, R gira, S escala, Shift+A menu Adicionar
            void updateShortcuts(const CrsInput &input);
            //clique esquerdo seleciona o objeto sob o cursor (fora das janelas e do gizmo)
            void updateSelection(CrsScene &scene, const CrsInput &input, const CrsCamera &camera, VkRect2D sceneArea);
            //objeto selecionado, buscado pelo id (nullptr = nenhum, ou já não existe)
            CrsGameObject* getSelected(CrsScene &scene) const;
            //nada selecionado (ao trocar de cena)
            void clearSelection();
            //seleciona um objeto (o que acabou de ser criado)
            void select(CrsGameObject::id_t id);
            //pedaço da tela que sobra pra cena, abaixo do menu e ao lado do painel
            VkRect2D sceneArea(VkExtent2D screen) const;
            //toda a interface do editor do frame: menu, painel, setas e gizmo de navegação
            //(components: as classes achadas em assets/components, pro Add Component e os args)
            EditorRequests drawUi(CrsScene &scene, const CrsCamera &camera, VkRect2D sceneArea,
                                  const std::vector<ComponentInfo> &components);

        private:
            //barra de menu no topo da janela (Arquivo...), como no Blender
            EditorRequests drawMenuBar();
            //itens do Adicionar: os mesmos na barra de menu e no Shift+A
            void drawAddMenuItems(EditorRequests &requests);
            //painel lateral preso à direita (outliner e propriedades)
            void drawSidePanel(CrsScene &scene, const std::vector<ComponentInfo> &components, EditorRequests &requests);
            //escolha da animação que o objeto toca
            void drawAnimatorProperties(CrsGameObject &obj);
            //componentes Python do objeto: args, remover, Add Component e Generate Component
            void drawComponentProperties(CrsGameObject &obj, const std::vector<ComponentInfo> &components, EditorRequests &requests);
            //setas de mover/girar/escalar no objeto selecionado (ImGuizmo)
            void drawTransformGizmo(CrsScene &scene, const CrsCamera &camera, VkRect2D sceneArea);
            //bolinhas dos eixos no canto da cena, como no Blender: clicar põe a câmera daquele lado
            void drawNavigationGizmo(const CrsCamera &camera, VkRect2D sceneArea);
            //linha tracejada fina de cada filho até o pai (as "relationship lines" do Blender)
            void drawRelationshipLines(CrsScene &scene, const CrsCamera &camera, VkRect2D sceneArea);
            //card do RigidBody: tipo, forma e tamanhos (sem física: o botão de pôr)
            void drawRigidBodyCard(CrsGameObject &obj);
            //a forma de colisão de cada corpo em arame verde, por cima da cena (só no editor)
            void drawColliders(CrsScene &scene, const CrsCamera &camera, VkRect2D sceneArea);

            //objeto selecionado (vazio = nenhum)
            std::optional<CrsGameObject::id_t> selectedId;
            //modo atual das setas do gizmo
            GizmoMode gizmoMode = GizmoMode::Move;
            //Shift+A apertado: o menu abre no próximo drawUi (só dá pra abrir dentro do frame do ImGui)
            bool addMenuRequested = false;
            //o nome sendo digitado no Generate Component
            std::string newComponentName{};
            //filhos de cada objeto
            using ChildrenMap = std::map<CrsGameObject::id_t, std::vector<CrsGameObject::id_t>>;
            //lista de objetos da cena em árvore
            void drawOutLiner(CrsScene &scene);
            //uma linha do outliner e, se aberta, os filhos dela
            void drawOutLinerNode(CrsScene &scene, CrsGameObject::id_t id, const ChildrenMap &children);
            //camera orbital
            OrbitCameraController cameraController{};
    };
}