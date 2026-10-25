#include "editor/lve_editor.hpp"

#include <imgui.h>
#include <ImGuizmo.h>

namespace lve {
    //largura do painel lateral
    constexpr float SIDE_PANEL_WIDTH = 320.f;

    void LveEditor::updateSelection(LveScene &scene, const LveInput &input, const LveCamera &camera, VkRect2D sceneArea) {
        //clique em janela do ImGui ou na seta do gizmo não seleciona
        if (ImGui::GetIO().WantCaptureMouse || ImGuizmo::IsOver()) return;
        if (!input.wasMousePressed(GLFW_MOUSE_BUTTON_LEFT)) return;

        //o raio é calculado dentro do retângulo da cena
        glm::vec2 areaOffset(static_cast<float>(sceneArea.offset.x), static_cast<float>(sceneArea.offset.y));
        glm::vec2 areaSize(static_cast<float>(sceneArea.extent.width), static_cast<float>(sceneArea.extent.height));

        Ray ray = screenPointToRay(input.getMousePosition() - areaOffset, areaSize, camera.getProjection(), camera.getView());
        //clicar no vazio desmarca, como no Blender
        selectedId = scene.pickObject(ray);
    }

    LveGameObject* LveEditor::getSelected(LveScene &scene) const {
        return selectedId ? scene.findGameObjectById(*selectedId) : nullptr;
    }

    void LveEditor::drawSidePanel(LveScene &scene) {
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

        //propriedades do celecionado
        if (ImGui::CollapsingHeader("Propriedades", ImGuiTreeNodeFlags_DefaultOpen)) {
            LveGameObject* selected = getSelected(scene);
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
            } else {
                ImGui::TextDisabled("Nada Selecionado");
            }
        }

        ImGui::End();
    }

    VkRect2D LveEditor::sceneArea(VkExtent2D screen) const {
        uint32_t panel = static_cast<uint32_t>(SIDE_PANEL_WIDTH);
        //janela mais estreita
        uint32_t width = screen.width > panel ? screen.width - panel : 1;
        return VkRect2D{{0,0}, {width, screen.height}};
    }
}