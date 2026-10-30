#include "editor/crs_editor.hpp"

#include <imgui.h>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <iterator>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace crsdk {
    //largura do painel lateral (pixels)
    constexpr float SIDE_PANEL_WIDTH = 320.f;
    //raio do gizmo de navegação (pixels)
    constexpr float NAV_GIZMO_RADIUS = 50.f;

    //centro do gizmo de navegação: canto de cima à direita da cena, com uma margem
    static ImVec2 navGizmoCenter(VkRect2D sceneArea) {
        return ImVec2(
            static_cast<float>(sceneArea.offset.x + static_cast<int32_t>(sceneArea.extent.width)) - NAV_GIZMO_RADIUS - 20.f,
            static_cast<float>(sceneArea.offset.y) + NAV_GIZMO_RADIUS + 20.f
        );
    }

    //ponto do mundo -> pixel na tela, dentro do retângulo da cena; vazio = atrás da câmera
    static std::optional<ImVec2> worldToScreen(glm::vec3 point, const glm::mat4 &viewProjection, VkRect2D sceneArea) {
        glm::vec4 clip = viewProjection * glm::vec4{point, 1.f};
        if (clip.w <= 0.0001f) return std::nullopt;
        glm::vec2 ndc = glm::vec2{clip} / clip.w;
        return ImVec2(
            static_cast<float>(sceneArea.offset.x) + (ndc.x * 0.5f + 0.5f) * static_cast<float>(sceneArea.extent.width),
            static_cast<float>(sceneArea.offset.y) + (ndc.y * 0.5f + 0.5f) * static_cast<float>(sceneArea.extent.height));
    }

    void CrsEditor::beginFrame() {
        ImGuizmo::BeginFrame();
    }

    void CrsEditor::updateCamera(CrsWindow &window, CrsCamera &camera) {
        cameraController.update(window, camera);
    }

    glm::vec3 CrsEditor::getFocusPoint() const {
        return cameraController.target;
    }

    void CrsEditor::saveView(const std::string &filepath) const {
        const OrbitCameraController &view = cameraController;
        nlohmann::ordered_json j = {
            {"target", {view.target.x, view.target.y, view.target.z}},
            {"distance", view.distance},
            {"yaw", view.yaw},
            {"pitch", view.pitch},
        };
        std::ofstream out{filepath};
        if (!out) {
            throw std::runtime_error("não consegui gravar a vista do editor: " + filepath);
        }
        out << j.dump(4);
    }

    bool CrsEditor::loadView(const std::string &filepath) {
        std::ifstream in{filepath};
        if (!in) return false;
        try {
            nlohmann::json j = nlohmann::json::parse(in);
            const nlohmann::json &target = j.at("target");
            OrbitCameraController &view = cameraController;
            view.target = {target.at(0).get<float>(), target.at(1).get<float>(), target.at(2).get<float>()};
            view.distance = j.value("distance", view.distance);
            view.yaw = j.value("yaw", view.yaw);
            view.pitch = j.value("pitch", view.pitch);
        } catch (const nlohmann::json::exception &) {
            return false;
        }
        return true;
    }

    void CrsEditor::updateSelection(CrsScene &scene, const CrsInput &input, const CrsCamera &camera, VkRect2D sceneArea) {
        //clique em janela do ImGui ou na seta do gizmo não seleciona
        if (ImGui::GetIO().WantCaptureMouse || ImGuizmo::IsOver()) return;
        if (!input.wasMousePressed(GLFW_MOUSE_BUTTON_LEFT)) return;
        //clique no gizmo de navegação é dele
        glm::vec2 mouse = input.getMousePosition();
        ImVec2 nav = navGizmoCenter(sceneArea);
        float dx = mouse.x - nav.x;
        float dy = mouse.y - nav.y;
        if (dx * dx + dy * dy <= NAV_GIZMO_RADIUS * NAV_GIZMO_RADIUS) return;

        //o raio é calculado dentro do retângulo da cena
        glm::vec2 areaOffset(static_cast<float>(sceneArea.offset.x), static_cast<float>(sceneArea.offset.y));
        glm::vec2 areaSize(static_cast<float>(sceneArea.extent.width), static_cast<float>(sceneArea.extent.height));

        Ray ray = screenPointToRay(input.getMousePosition() - areaOffset, areaSize, camera.getProjection(), camera.getView());
        //clicar no vazio desmarca, como no Blender
        selectedId = scene.pickObject(ray);
    }

    void CrsEditor::updateShortcuts(const CrsInput &input) {
        //digitando num campo do imgui
        if (ImGui::GetIO().WantCaptureKeyboard) return;
        //Ctrl + tecla é atalho do FirstApp (Ctrl+S salva)
        if (input.isDown(GLFW_KEY_LEFT_CONTROL) || input.isDown(GLFW_KEY_RIGHT_CONTROL)) return;
        if (input.isDown(GLFW_KEY_LEFT_SHIFT) || input.isDown(GLFW_KEY_RIGHT_SHIFT)) {
            if (input.wasPressed(GLFW_KEY_A)) addMenuRequested = true;
            return;
        }
        if (input.wasPressed(GLFW_KEY_G)) gizmoMode = GizmoMode::Move;
        if (input.wasPressed(GLFW_KEY_R)) gizmoMode = GizmoMode::Rotate;
        if (input.wasPressed(GLFW_KEY_S)) gizmoMode = GizmoMode::Scale;
    }

    void CrsEditor::clearSelection() {
        selectedId.reset();
    }

    void CrsEditor::select(CrsGameObject::id_t id) {
        selectedId = id;
    }

    CrsGameObject* CrsEditor::getSelected(CrsScene &scene) const {
        return selectedId ? scene.findGameObjectById(*selectedId) : nullptr;
    }

    void CrsEditor::drawSidePanel(CrsScene &scene) {
        //a direita da janela
        const ImGuiViewport* screen = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(screen->WorkPos.x + screen->WorkSize.x - SIDE_PANEL_WIDTH, screen->WorkPos.y));
        ImGui::SetNextWindowSize(ImVec2(SIDE_PANEL_WIDTH, screen->WorkSize.y));
        ImGui::Begin(
            "Panel",
            nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse
        );

        //lista de objetos da cena
        if (ImGui::CollapsingHeader("Outliner", ImGuiTreeNodeFlags_DefaultOpen)) {
            drawOutLiner(scene);
        }

        //propriedades do selecionado
        if (ImGui::CollapsingHeader("Propriedades", ImGuiTreeNodeFlags_DefaultOpen)) {
            CrsGameObject* selected = getSelected(scene);
            if (selected) {
                ImGui::Text("%s", selected->name.c_str());
                ImGui::Separator();

                TransformComponent& transform = selected->transform;
                ImGui::DragFloat3("Posição:", &transform.translation.x, 0.05f);
                //engine guarda radianos e o painel mostra graus
                glm::vec3 degrees = glm::degrees(transform.rotation);
                if (ImGui::DragFloat3("Rotação:", &degrees.x, 1.f)) {
                    transform.rotation = glm::radians(degrees);
                }
                ImGui::DragFloat3("Escala:", &transform.scale.x, 0.01f);

                if (selected->animator) {
                    ImGui::Separator();
                    drawAnimatorProperties(*selected);
                }
            } else {
                ImGui::TextDisabled("Nada Selecionado");
            }
        }

        ImGui::End();
    }

    void CrsEditor::drawAnimatorProperties(CrsGameObject &obj) {
        AnimatorComponent &animator = *obj.animator;
        //o que dá pra tocar: os arquivos do animator e as animações do próprio modelo
        std::vector<std::string> names;
        for (const std::string &file : animator.animations) {
            names.push_back(std::filesystem::path{file}.stem().string());
        }
        if (obj.model) {
            for (const AnimationClip &clip : obj.model->getAnimations()) names.push_back(clip.name);
        }

        const char *current = animator.clip.empty() ? "(a primeira)" : animator.clip.c_str();
        if (ImGui::BeginCombo("Animação:", current)) {
            for (size_t i = 0; i < names.size(); i++) {
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::Selectable(names[i].c_str(), names[i] == animator.clip)) {
                    animator.clip = names[i];
                    animator.time = 0.f;
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
    }

    VkRect2D CrsEditor::sceneArea(VkExtent2D screen) const {
        uint32_t panel = static_cast<uint32_t>(SIDE_PANEL_WIDTH);
        //altura da barra de menu (a do ImGui: fonte + margem)
        uint32_t menu = static_cast<uint32_t>(ImGui::GetFrameHeight());
        //janela menor que o painel/menu
        uint32_t width = screen.width > panel ? screen.width - panel : 1;
        uint32_t height = screen.height > menu ? screen.height - menu : 1;
        return VkRect2D{{0, static_cast<int32_t>(menu)}, {width, height}};
    }

    void CrsEditor::drawTransformGizmo(CrsScene &scene, const CrsCamera &camera, VkRect2D sceneArea) {
        CrsGameObject* selected = getSelected(scene);
        if (!selected) return;

        //desenha por cima da cena
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
        ImGuizmo::SetRect(
            static_cast<float>(sceneArea.offset.x),
            static_cast<float>(sceneArea.offset.y),
            static_cast<float>(sceneArea.extent.width),
            static_cast<float>(sceneArea.extent.height)
        );

        glm::mat4 view = camera.getView();
        glm::mat4 projection = camera.getProjection();
        projection[1][1] *= -1.f;

        //o modo editor vira a operação da imguizmo
        ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
        if (gizmoMode == GizmoMode::Rotate) operation = ImGuizmo::ROTATE;
        if (gizmoMode == GizmoMode::Scale) operation = ImGuizmo::SCALE;
        //escalar é sempre nos eixos do proprio objeto
        ImGuizmo::MODE space = gizmoMode == GizmoMode::Scale ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

        glm::mat4 world = scene.worldMatrix(*selected);
        if (ImGuizmo::Manipulate(
            glm::value_ptr(view),
            glm::value_ptr(projection),
            operation,
            space,
            glm::value_ptr(world)
        )) {
            glm::mat4 parentWorld{1.f};
            if (selected->parentId) {
                if (CrsGameObject* parent = scene.findGameObjectById(*selected->parentId)) {
                    parentWorld = scene.worldMatrix(*parent);
                }
            }
            glm::mat4 local = glm::inverse(parentWorld) * world;
            selected->transform.setFromMatrix(local);
        }
    }

    void CrsEditor::drawOutLiner(CrsScene &scene) {
        ChildrenMap children;
        std::vector<CrsGameObject::id_t> roots;

        for (auto &kv : scene.getGameObjects()) {
            const CrsGameObject &obj = kv.second;
            if (obj.parentId && scene.findGameObjectById(*obj.parentId)) {
                children[*obj.parentId].push_back(obj.getId());
            } else {
                roots.push_back(obj.getId());
            }
        }

        //orden pelo id
        std::sort(roots.begin(), roots.end());

        for (auto &kv : children) {
            std::sort(kv.second.begin(), kv.second.end());
        }

        for (CrsGameObject::id_t id : roots) {
            drawOutLinerNode(scene, id, children);
        }
    }

    void CrsEditor::drawOutLinerNode(CrsScene &scene, CrsGameObject::id_t id, const ChildrenMap &children) {
        CrsGameObject *obj = scene.findGameObjectById(id);
        if (!obj) return;
        auto found = children.find(id);
        bool hasChildren = found != children.end();

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
        //linha simples para objeto sem filhos
        if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if (selectedId && *selectedId == id) flags |= ImGuiTreeNodeFlags_Selected;

        const char *label = obj->name.empty() ? "(sem nome)" : obj->name.c_str();
        bool open = ImGui::TreeNodeEx(reinterpret_cast<void *>(static_cast<intptr_t>(id)), flags, "%s", label);

        //clicar no nome/setinha
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            selectedId = id;
        }

        if (hasChildren && open) {
            for (CrsGameObject::id_t childId : found->second) {
                drawOutLinerNode(scene, childId, children);
            }
            ImGui::TreePop();
        }
    }

    EditorRequests CrsEditor::drawMenuBar() {
        EditorRequests requests{};
        if (!ImGui::BeginMainMenuBar()) return requests;

        if (ImGui::BeginMenu("Arquivo")) {
            if (ImGui::MenuItem("Abrir...", "Ctrl+O")) requests.openScene = true;
            if (ImGui::MenuItem("Salvar", "Ctrl+S")) requests.saveScene = true;
            if (ImGui::MenuItem("Salvar como...", "Ctrl+Shift+S")) requests.saveSceneAs = true;
            ImGui::Separator();
            if (ImGui::MenuItem("Importar .glb...")) requests.importModel = true;
            ImGui::Separator();
            if (ImGui::MenuItem("Sair")) requests.quit = true;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Adicionar")) {
            drawAddMenuItems(requests);
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
        return requests;
    }

    void CrsEditor::drawAddMenuItems(EditorRequests &requests) {
        if (ImGui::MenuItem("Empty")) requests.add = AddKind::Empty;
        if (ImGui::MenuItem("Câmera")) requests.add = AddKind::Camera;
        if (ImGui::MenuItem("Luz pontual")) requests.add = AddKind::PointLight;
    }

    EditorRequests CrsEditor::drawUi(CrsScene &scene, const CrsCamera &camera, VkRect2D sceneArea) {
        //menu primeiro: o painel lateral começa embaixo dele
        EditorRequests requests = drawMenuBar();
        //Shift+A: o mesmo menu Adicionar, aberto onde está o mouse
        if (addMenuRequested) {
            ImGui::OpenPopup("AddMenu");
            addMenuRequested = false;
        }
        if (ImGui::BeginPopup("AddMenu")) {
            ImGui::SeparatorText("Adicionar");
            drawAddMenuItems(requests);
            ImGui::EndPopup();
        }
        drawSidePanel(scene);
        drawRelationshipLines(scene, camera, sceneArea);
        drawTransformGizmo(scene, camera, sceneArea);
        drawNavigationGizmo(camera, sceneArea);
        return requests;
    }

    void CrsEditor::drawNavigationGizmo(const CrsCamera &camera, VkRect2D sceneArea) {
        //os 6 eixos do mundo e a cor de cada um (X vermelho, Y verde, Z azul, como no Blender);
        //positivo = bolinha cheia com a letra, negativo = só o contorno
        struct Axis { glm::vec3 direction; ImU32 color; const char *label; };
        static const Axis axes[6] = {
            {{1.f, 0.f, 0.f}, IM_COL32(230, 70, 80, 255), "X"},
            {{-1.f, 0.f, 0.f}, IM_COL32(230, 70, 80, 255), nullptr},
            {{0.f, 1.f, 0.f}, IM_COL32(120, 200, 60, 255), "Y"},
            {{0.f, -1.f, 0.f}, IM_COL32(120, 200, 60, 255), nullptr},
            {{0.f, 0.f, 1.f}, IM_COL32(60, 140, 240, 255), "Z"},
            {{0.f, 0.f, -1.f}, IM_COL32(60, 140, 240, 255), nullptr},
        };

        //só a rotação da câmera: leva cada eixo pro espaço da tela do Vulkan
        //(x pra direita, y pra baixo, z pra dentro da tela) — sem conversão nenhuma
        glm::mat3 viewRotation{camera.getView()};
        ImVec2 center = navGizmoCenter(sceneArea);
        const float reach = NAV_GIZMO_RADIUS * 0.75f;
        const float bubble = 9.f;

        struct Projected { const Axis *axis; ImVec2 position; float depth; };
        Projected projected[6];
        for (int i = 0; i < 6; i++) {
            glm::vec3 v = viewRotation * axes[i].direction;
            projected[i] = {&axes[i], ImVec2(center.x + v.x * reach, center.y + v.y * reach), v.z};
        }
        //do mais longe pro mais perto: o que está na frente é desenhado por cima
        std::sort(std::begin(projected), std::end(projected),
            [](const Projected &a, const Projected &b) { return a.depth > b.depth; });

        ImDrawList *draw = ImGui::GetForegroundDrawList();
        draw->AddCircleFilled(center, NAV_GIZMO_RADIUS, IM_COL32(40, 40, 40, 90));
        for (const Projected &p : projected) {
            if (p.axis->label) {
                draw->AddLine(center, p.position, p.axis->color, 2.f);
                draw->AddCircleFilled(p.position, bubble, p.axis->color);
                draw->AddText(ImVec2(p.position.x - 4.f, p.position.y - 7.f), IM_COL32(20, 20, 20, 255), p.axis->label);
            } else {
                draw->AddCircle(p.position, bubble, p.axis->color, 0, 2.f);
            }
        }

        //clique numa bolinha: a câmera vai pra aquele lado, olhando pro alvo (a mais perto ganha)
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            ImVec2 mouse = ImGui::GetIO().MousePos;
            for (int i = 5; i >= 0; i--) {
                float dx = mouse.x - projected[i].position.x;
                float dy = mouse.y - projected[i].position.y;
                if (dx * dx + dy * dy <= bubble * bubble) {
                    cameraController.setOrbitDirection(projected[i].axis->direction);
                    break;
                }
            }
        }
    }

    void CrsEditor::drawRelationshipLines(CrsScene &scene, const CrsCamera &camera, VkRect2D sceneArea) {
        glm::mat4 viewProjection = camera.getProjection() * camera.getView();
        ImDrawList *draw = ImGui::GetBackgroundDrawList();
        const float dash = 6.f;  //tamanho de cada traço e de cada espaço (pixels)
        const ImU32 color = IM_COL32(0, 0, 0, 220);

        for (auto &kv : scene.getGameObjects()) {
            CrsGameObject &child = kv.second;
            if (!child.parentId) continue;
            CrsGameObject *parent = scene.findGameObjectById(*child.parentId);
            if (!parent) continue;

            //origens no mundo (coluna 3) projetadas na tela
            auto from = worldToScreen(glm::vec3{scene.worldMatrix(child)[3]}, viewProjection, sceneArea);
            auto to = worldToScreen(glm::vec3{scene.worldMatrix(*parent)[3]}, viewProjection, sceneArea);
            if (!from || !to) continue;  //alguma ponta atrás da câmera

            //tracejado: o ImGui só desenha linha cheia, então são pedaços com buracos entre eles
            float dx = to->x - from->x;
            float dy = to->y - from->y;
            float length = std::sqrt(dx * dx + dy * dy);
            if (length < 1.f) continue;
            float ux = dx / length;
            float uy = dy / length;
            for (float t = 0.f; t < length; t += dash * 2.f) {
                float end = std::min(t + dash, length);
                draw->AddLine(ImVec2(from->x + ux * t, from->y + uy * t),
                              ImVec2(from->x + ux * end, from->y + uy * end), color, 1.f);
            }
        }
    }
}