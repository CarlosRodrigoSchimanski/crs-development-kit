#include "editor/crs_editor.hpp"
#include "editor/crs_editor_theme.hpp"
#include "engine/rendering/crs_material_files.hpp"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <iterator>
#include <ImGuizmo.h>
#include <glm/gtc/constants.hpp>
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
        //esculpindo o terreno: o clique é do pincel
        if (sculptMode(scene)) return;
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

    void CrsEditor::drawSidePanel(CrsScene &scene, const std::vector<ComponentInfo> &components, EditorRequests &requests) {
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

        //lista de objetos da cena: um card só, que rola por dentro quando a cena é grande
        theme::beginCard("OutlinerCard", screen->WorkSize.y * 0.4f);
        if (theme::cardHeader("Outliner", "", true)) drawOutLiner(scene);
        theme::endCard();

        //propriedades do selecionado: um card pra cada parte (transform, animação, cada componente),
        //fechados de início: só o título; a setinha abre (e o card lembra se estava aberto)
        CrsGameObject* selected = getSelected(scene);
        if (selected) {
            theme::beginCard("TransformCard");
            if (theme::cardHeader(selected->name, "Transform")) {
                TransformComponent& transform = selected->transform;
                theme::fieldLabel("Posição");
                ImGui::DragFloat3("##posicao", &transform.translation.x, 0.05f);
                //engine guarda radianos e o painel mostra graus
                glm::vec3 degrees = glm::degrees(transform.rotation);
                theme::fieldLabel("Rotação");
                if (ImGui::DragFloat3("##rotacao", &degrees.x, 1.f)) {
                    transform.rotation = glm::radians(degrees);
                }
                theme::fieldLabel("Escala");
                ImGui::DragFloat3("##escala", &transform.scale.x, 0.01f);
            }
            theme::endCard();

            if (selected->animator) {
                theme::beginCard("AnimatorCard");
                if (theme::cardHeader("Animação")) drawAnimatorProperties(*selected);
                theme::endCard();
            }

            if (selected->terrain) drawTerrainCard(*selected);
            drawRigidBodyCard(*selected);

            drawComponentProperties(*selected, components, requests);
        } else {
            theme::beginCard("NothingSelectedCard");
            ImGui::TextDisabled("Nada selecionado");
            theme::endCard();
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
        theme::fieldLabel("Clipe");
        if (ImGui::BeginCombo("##clipe", current)) {
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

    static const char *shapeLabel(ColliderShape shape) {
        switch (shape) {
            case ColliderShape::Sphere: return "Esfera";
            case ColliderShape::Capsule: return "Cápsula";
            case ColliderShape::Box: break;
        }
        return "Caixa";
    }

    void CrsEditor::drawRigidBodyCard(CrsGameObject &obj) {
        //sem física: só o botão de pôr (o card aparece quando tem)
        if (!obj.rigidBody) {
            if (ImGui::Button("Adicionar física", ImVec2(-1.f, 0.f))) obj.rigidBody.emplace();
            return;
        }

        RigidBodyComponent &body = *obj.rigidBody;
        theme::beginCard("RigidBodyCard");
        bool open = theme::cardHeader("Física", shapeLabel(body.shape));
        float removeWidth = ImGui::CalcTextSize("Remover").x + ImGui::GetStyle().FramePadding.x * 2.f;
        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - removeWidth);
        bool remove = ImGui::SmallButton("Remover##fisica");

        if (open) {
            int type = body.type == BodyType::Static ? 0 : 1;
            theme::fieldLabel("Tipo");
            if (ImGui::Combo("##tipo", &type, "Estático\0Dinâmico\0")) {
                body.type = type == 0 ? BodyType::Static : BodyType::Dynamic;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Estático: parado (chão, parede)\nDinâmico: cai, tomba e é empurrado");

            int shape = static_cast<int>(body.shape);
            theme::fieldLabel("Forma");
            if (ImGui::Combo("##forma", &shape, "Caixa\0Esfera\0Cápsula\0")) {
                body.shape = static_cast<ColliderShape>(shape);
            }

            //só os tamanhos da forma escolhida (antes da escala do objeto)
            if (body.shape == ColliderShape::Box) {
                theme::fieldLabel("Meio tam.");
                ImGui::DragFloat3("##meio", &body.halfExtents.x, 0.01f, 0.f, 1000.f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Metade da largura, altura e profundidade");
            } else {
                theme::fieldLabel("Raio");
                ImGui::DragFloat("##raio", &body.radius, 0.01f, 0.01f, 1000.f);
            }
            if (body.shape == ColliderShape::Capsule) {
                theme::fieldLabel("Altura");
                ImGui::DragFloat("##altura", &body.height, 0.01f, 0.01f, 1000.f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Altura total, de ponta a ponta");
            }
            theme::fieldLabel("Centro");
            ImGui::DragFloat3("##centro", &body.center.x, 0.01f);
            if (body.type == BodyType::Dynamic) {
                theme::fieldLabel("Massa (kg)");
                ImGui::DragFloat("##massa", &body.mass, 0.1f, 0.01f, 100000.f);
            }
        }
        theme::endCard();
        if (remove) obj.rigidBody.reset();
    }

    void CrsEditor::drawColliders(CrsScene &scene, const CrsCamera &camera, VkRect2D sceneArea) {
        glm::mat4 viewProjection = camera.getProjection() * camera.getView();
        ImDrawList *draw = ImGui::GetBackgroundDrawList();
        const CrsGameObject *selected = getSelected(scene);

        for (auto &kv : scene.getGameObjects()) {
            CrsGameObject &obj = kv.second;
            if (!obj.rigidBody) continue;
            const RigidBodyComponent &body = *obj.rigidBody;
            //verde como nas outras engines; o selecionado mais forte
            const ImU32 color = &obj == selected ? IM_COL32(80, 255, 120, 255) : IM_COL32(80, 220, 120, 110);

            //a física usa posição e rotação do objeto, e a escala só nos tamanhos (sem distorcer a forma)
            const glm::mat4 world = scene.worldMatrix(obj);
            const glm::mat3 rotation{glm::normalize(glm::vec3{world[0]}), glm::normalize(glm::vec3{world[1]}),
                                     glm::normalize(glm::vec3{world[2]})};
            const glm::vec3 scale = glm::abs(obj.transform.scale);
            const glm::vec3 center = glm::vec3{world[3]} + rotation * (body.center * scale);
            auto line = [&](glm::vec3 a, glm::vec3 b) {
                auto from = worldToScreen(center + rotation * a, viewProjection, sceneArea);
                auto to = worldToScreen(center + rotation * b, viewProjection, sceneArea);
                if (from && to) draw->AddLine(*from, *to, color, 1.f);
            };
            //arco de um círculo no plano de dois eixos (de 0 a 'turn' voltas), em volta de offset
            auto arc = [&](glm::vec3 axisA, glm::vec3 axisB, float radius, glm::vec3 offset, float turn) {
                constexpr int SIDES = 24;
                for (int i = 0; i < SIDES; i++) {
                    float a0 = glm::two_pi<float>() * turn * static_cast<float>(i) / SIDES;
                    float a1 = glm::two_pi<float>() * turn * static_cast<float>(i + 1) / SIDES;
                    line(offset + (axisA * std::cos(a0) + axisB * std::sin(a0)) * radius,
                         offset + (axisA * std::cos(a1) + axisB * std::sin(a1)) * radius);
                }
            };
            const glm::vec3 X{1.f, 0.f, 0.f}, Y{0.f, 1.f, 0.f}, Z{0.f, 0.f, 1.f};

            switch (body.shape) {
                case ColliderShape::Box: {
                    const glm::vec3 h = body.halfExtents * scale;
                    for (int i = 0; i < 4; i++) {
                        //as 4 arestas de cada direção: os sinais das outras duas medidas
                        float s1 = (i & 1) ? 1.f : -1.f, s2 = (i & 2) ? 1.f : -1.f;
                        line({-h.x, s1 * h.y, s2 * h.z}, {h.x, s1 * h.y, s2 * h.z});
                        line({s1 * h.x, -h.y, s2 * h.z}, {s1 * h.x, h.y, s2 * h.z});
                        line({s1 * h.x, s2 * h.y, -h.z}, {s1 * h.x, s2 * h.y, h.z});
                    }
                    break;
                }
                case ColliderShape::Sphere: {
                    const float r = body.radius * std::max({scale.x, scale.y, scale.z});
                    arc(X, Y, r, {}, 1.f);
                    arc(Y, Z, r, {}, 1.f);
                    arc(X, Z, r, {}, 1.f);
                    break;
                }
                case ColliderShape::Capsule: {
                    const float r = body.radius * std::max(scale.x, scale.z);
                    const float halfStraight = std::max(body.height * 0.5f * scale.y - r, 0.f);
                    const glm::vec3 top = Y * halfStraight;
                    arc(X, Z, r, top, 1.f);
                    arc(X, Z, r, -top, 1.f);
                    //as laterais e as duas pontas redondas (meia volta em cada plano vertical)
                    for (glm::vec3 side : {X, Z}) {
                        line(top + side * r, -top + side * r);
                        line(top - side * r, -top - side * r);
                        arc(side, Y, r, top, 0.5f);
                        arc(side, -Y, r, -top, 0.5f);
                    }
                    break;
                }
            }
        }
    }

    bool CrsEditor::sculptMode(CrsScene &scene) const {
        CrsGameObject *selected = getSelected(scene);
        return selected && selected->terrain && terrainTool != TerrainTool::None;
    }

    void CrsEditor::updateTerrainBrush(CrsScene &scene, const CrsInput &input, const CrsCamera &camera, VkRect2D sceneArea, float dt) {
        brushHit.reset();
        if (!input.isMouseDown(GLFW_MOUSE_BUTTON_LEFT)) sculpting = false;
        if (!sculptMode(scene)) {
            sculpting = false;
            return;
        }
        //mouse em cima do painel: nem pincel nem esculpir
        if (ImGui::GetIO().WantCaptureMouse) return;

        CrsGameObject &obj = *getSelected(scene);
        TerrainData &data = *obj.terrain->data;
        const glm::vec3 corner = obj.terrain->corner(obj.transform.translation);

        glm::vec2 areaOffset(static_cast<float>(sceneArea.offset.x), static_cast<float>(sceneArea.offset.y));
        glm::vec2 areaSize(static_cast<float>(sceneArea.extent.width), static_cast<float>(sceneArea.extent.height));
        Ray ray = screenPointToRay(input.getMousePosition() - areaOffset, areaSize, camera.getProjection(), camera.getView());
        //o terreno trabalha em coordenadas locais (a partir do canto)
        const glm::vec3 origin = ray.origin - corner;
        auto distance = data.raycast(origin, ray.direction, 5000.f);
        if (!distance) return;
        const glm::vec3 hit = origin + glm::normalize(ray.direction) * *distance;
        brushHit = hit + corner;

        if (input.wasMousePressed(GLFW_MOUSE_BUTTON_LEFT)) {
            sculpting = true;
            flattenHeight = hit.y;
        }
        if (!sculpting) return;

        //força 1 = 5 m por segundo no meio do pincel (subir); suavizar/nivelar: quanto anda por frame
        const float rate = brushStrength * 5.f * dt;
        const glm::vec2 center{hit.x, hit.z};
        const bool shift = input.isDown(GLFW_KEY_LEFT_SHIFT) || input.isDown(GLFW_KEY_RIGHT_SHIFT);
        switch (terrainTool) {
            case TerrainTool::Raise: data.raise(center, brushRadius, shift ? -rate : rate); break;
            case TerrainTool::Smooth: data.smooth(center, brushRadius, std::min(rate * 2.f, 1.f)); break;
            case TerrainTool::Flatten: data.flatten(center, brushRadius, flattenHeight, std::min(rate * 2.f, 1.f)); break;
            case TerrainTool::None: break;
        }
    }

    TerrainOverlay CrsEditor::terrainOverlay() const {
        TerrainOverlay overlay{};
        if (contourLines) overlay.contourInterval = contourInterval;
        if (brushHit) {
            overlay.brushCenter = *brushHit;
            overlay.brushRadius = brushRadius;
        }
        return overlay;
    }

    //muda o tamanho do terreno mantendo o canto no lugar: o que já foi esculpido não sai do lugar no
    //mundo, e os pedaços novos entram pra +X e +Z (o objeto fica no meio, então ele anda junto)
    static void resizeTerrain(CrsGameObject &obj, int tiles) {
        const glm::vec3 corner = obj.terrain->corner(obj.transform.translation);
        obj.terrain->data = std::make_shared<TerrainData>(obj.terrain->data->resized(tiles));
        const float half = obj.terrain->data->size() * 0.5f;
        obj.transform.translation = corner + glm::vec3{half, 0.f, half};
    }

    void CrsEditor::drawTerrainCard(CrsGameObject &obj) {
        const int tilesPerSide = obj.terrain->data->tilesPerSide();
        const int size = static_cast<int>(obj.terrain->data->size());
        const std::string sizeText = std::to_string(size) + " x " + std::to_string(size) + " m";
        theme::beginCard("TerrainCard");
        if (theme::cardHeader("Terreno", sizeText, true)) {
            //tamanho: sempre um múltiplo de 256 m; − e + tiram/põem um pedaço por lado
            const float buttonWidth = ImGui::GetFrameHeight();
            theme::fieldLabel("Tamanho");
            ImGui::BeginDisabled(tilesPerSide <= 1);
            if (ImGui::Button("-", ImVec2(buttonWidth, 0.f))) resizeTerrain(obj, tilesPerSide - 1);
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Tira um pedaço por lado (as bordas +X e +Z somem com o que tinha nelas)");
            }
            ImGui::SameLine();
            const float textWidth = ImGui::GetContentRegionAvail().x - buttonWidth - ImGui::GetStyle().ItemSpacing.x;
            const float textX = ImGui::GetCursorPosX() + (textWidth - ImGui::CalcTextSize(sizeText.c_str()).x) * 0.5f;
            ImGui::SetCursorPosX(textX);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(sizeText.c_str());
            ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - buttonWidth);
            ImGui::BeginDisabled(tilesPerSide >= TERRAIN_MAX_TILES);
            if (ImGui::Button("+", ImVec2(buttonWidth, 0.f))) resizeTerrain(obj, tilesPerSide + 1);
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Põe um pedaço de 256 m por lado (entra plano, pra +X e +Z); máximo %d m",
                                  TERRAIN_MAX_TILES * static_cast<int>(TERRAIN_TILE_SIZE));
            }
            const int tiles = tilesPerSide * tilesPerSide;
            ImGui::TextDisabled("%d %s de 256 m", tiles, tiles == 1 ? "pedaço" : "pedaços");

            //material: as pastas de assets/materials que têm pelo menos a cor (lidas ao abrir a lista)
            std::string &material = obj.terrain->material;
            const std::string current = material.empty() ? "(cor provisória)" : std::filesystem::path{material}.filename().string();
            theme::fieldLabel("Material");
            if (ImGui::BeginCombo("##material_terreno", current.c_str())) {
                if (ImGui::Selectable("(cor provisória)", material.empty())) material.clear();
                for (const std::string &folder : listMaterials()) {
                    const std::string name = std::filesystem::path{folder}.filename().string();
                    if (ImGui::Selectable(name.c_str(), folder == material)) material = folder;
                }
                ImGui::EndCombo();
            }
            if (!material.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", material.c_str());
            if (!material.empty()) {
                theme::fieldLabel("Repetir");
                ImGui::DragFloat("##repetir_textura", &obj.terrain->textureSize, 0.05f, 0.25f, 256.f, "a cada %.2f m");
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Tamanho de uma foto do material no chão, em metros");
            }

            //as ferramentas lado a lado; a escolhida fica com a cor de destaque
            struct Option { TerrainTool tool; const char *label; const char *tip; };
            const Option options[] = {
                {TerrainTool::None, "Nenhum", "O clique volta a selecionar objetos"},
                {TerrainTool::Raise, "Subir", "Segure o botão esquerdo pra subir; com Shift, abaixa"},
                {TerrainTool::Smooth, "Suavizar", "Arredonda o relevo (tira os degraus)"},
                {TerrainTool::Flatten, "Nivelar", "Leva tudo pra altura onde o clique começou"},
            };
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float width = (ImGui::GetContentRegionAvail().x - spacing * 3.f) / 4.f;
            for (int i = 0; i < 4; i++) {
                if (i > 0) ImGui::SameLine();
                const bool active = terrainTool == options[i].tool;
                if (active) ImGui::PushStyleColor(ImGuiCol_Button, theme::accent());
                if (ImGui::Button(options[i].label, ImVec2(width, 0.f))) terrainTool = options[i].tool;
                if (active) ImGui::PopStyleColor();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", options[i].tip);
            }

            theme::fieldLabel("Raio");
            ImGui::SliderFloat("##raio_pincel", &brushRadius, 1.f, 64.f, "%.0f m");
            theme::fieldLabel("Força");
            ImGui::SliderFloat("##forca_pincel", &brushStrength, 0.05f, 1.f, "%.2f");

            ImGui::Checkbox("Curvas de nível", &contourLines);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Linhas de altura, só no editor (a mais forte a cada 5)");
            if (contourLines) {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(-1.f);
                ImGui::DragFloat("##intervalo", &contourInterval, 0.05f, 0.25f, 50.f, "a cada %.2f m");
            }
        }
        theme::endCard();
    }

    //a classe do catálogo com esse arquivo e nome (nullptr = o .py sumiu, foi renomeado ou está com erro)
    static const ComponentInfo* findComponentInfo(const std::vector<ComponentInfo> &components, const ScriptComponent &script) {
        for (const ComponentInfo &info : components) {
            if (info.module == script.module && info.className == script.className) return &info;
        }
        return nullptr;
    }

    //o campo de um arg, do tipo do valor (id: um "##nome" escondido); true = mudou neste frame
    static bool drawArgField(const std::string &id, ScriptArg &value) {
        if (auto *b = std::get_if<bool>(&value)) return ImGui::Checkbox(id.c_str(), b);
        if (auto *i = std::get_if<int>(&value)) return ImGui::DragInt(id.c_str(), i);
        if (auto *f = std::get_if<float>(&value)) return ImGui::DragFloat(id.c_str(), f, 0.05f);
        return ImGui::InputText(id.c_str(), &std::get<std::string>(value));
    }

    void CrsEditor::drawComponentProperties(CrsGameObject &obj, const std::vector<ComponentInfo> &components, EditorRequests &requests) {
        std::optional<size_t> toRemove{};
        for (size_t i = 0; i < obj.scripts.size(); i++) {
            ScriptComponent &script = obj.scripts[i];
            ImGui::PushID(static_cast<int>(i));
            //um card por componente: a classe no título e o arquivo apagado ao lado
            theme::beginCard("ComponentCard");
            bool open = theme::cardHeader(script.className, script.module);
            //Remover encostado na direita da linha do título (aparece com o card fechado também)
            float removeWidth = ImGui::CalcTextSize("Remover").x + ImGui::GetStyle().FramePadding.x * 2.f;
            ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - removeWidth);
            if (ImGui::SmallButton("Remover")) toRemove = i;

            const ComponentInfo *info = findComponentInfo(components, script);
            if (!open) {
                //fechado: só o título
            } else if (info == nullptr) {
                ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "não achado em assets/components");
            } else {
                if (!info->description.empty()) ImGui::TextDisabled("%s", info->description.c_str());
                for (const ComponentArgInfo &arg : info->args) {
                    //o valor mudado neste objeto, ou o padrão da classe
                    //(tipo diferente do padrão = o .py mudou o tipo do arg: vale o padrão)
                    auto it = script.args.find(arg.name);
                    bool changed = it != script.args.end() && it->second.index() == arg.defaultValue.index();
                    ScriptArg value = changed ? it->second : arg.defaultValue;
                    //mudado: o campo deixa lugar pro botão "padrão" na mesma linha
                    float resetWidth = ImGui::CalcTextSize("padrão").x + ImGui::GetStyle().FramePadding.x * 2.f;
                    theme::fieldLabel(arg.name, changed ? resetWidth : 0.f);
                    if (drawArgField("##" + arg.name, value)) script.args[arg.name] = value;
                    if (changed) {
                        ImGui::SameLine();
                        if (ImGui::SmallButton(("padrão##" + arg.name).c_str())) script.args.erase(arg.name);
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Voltar ao valor do .py");
                    }
                }
            }
            theme::endCard();
            ImGui::PopID();
        }
        //depois do for: apagar no meio dele pularia o próximo componente
        if (toRemove) obj.scripts.erase(obj.scripts.begin() + static_cast<std::ptrdiff_t>(*toRemove));

        //os dois botões dividem a largura do painel
        float halfWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        if (ImGui::Button("Add Component", ImVec2(halfWidth, 0.f))) {
            requests.refreshComponents = true;
            ImGui::OpenPopup("AddComponent");
        }
        ImGui::SameLine();
        if (ImGui::Button("Generate Component", ImVec2(halfWidth, 0.f))) {
            newComponentName.clear();
            ImGui::OpenPopup("GenerateComponent");
        }

        if (ImGui::BeginPopup("AddComponent")) {
            if (components.empty()) ImGui::TextDisabled("nenhum componente em assets/components");
            for (const ComponentInfo &info : components) {
                std::string label = info.className + " (" + info.module + ")";
                if (ImGui::Selectable(label.c_str())) obj.scripts.push_back({info.module, info.className});
                if (!info.description.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", info.description.c_str());
            }
            ImGui::EndPopup();
        }

        if (ImGui::BeginPopup("GenerateComponent")) {
            ImGui::Text("Nome da classe (ex: PuloDuplo):");
            if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            bool enter = ImGui::InputText("##nome", &newComponentName, ImGuiInputTextFlags_EnterReturnsTrue);
            if ((ImGui::Button("Criar") || enter) && !newComponentName.empty()) {
                requests.generateComponent = newComponentName;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
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
        //esculpindo: sem setas no meio do terreno
        if (sculptMode(scene)) return;

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
        //nasce com 1 pedaço (256 m); o tamanho muda no card dele, de pedaço em pedaço
        if (ImGui::MenuItem("Terreno")) requests.addTerrainTiles = 1;
    }

    EditorRequests CrsEditor::drawUi(CrsScene &scene, const CrsCamera &camera, VkRect2D sceneArea,
                                     const std::vector<ComponentInfo> &components) {
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
        drawSidePanel(scene, components, requests);
        drawRelationshipLines(scene, camera, sceneArea);
        drawColliders(scene, camera, sceneArea);
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