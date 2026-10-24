#include "editor/lve_editor.hpp"

#include <imgui.h>
#include <ImGuizmo.h>

namespace lve {
    void LveEditor::updateSelection(LveScene &scene, const LveInput &input, const LveCamera &camera, VkRect2D sceneArea) {
        if (ImGui::GetIO().WantCaptureMouse || ImGuizmo::IsOver()) return;
        if (!input.wasMousePressed(GLFW_MOUSE_BUTTON_LEFT)) return;

        glm::vec2 areaOffset(static_cast<float>(sceneArea.offset.x), static_cast<float>(sceneArea.offset.y));
        glm::vec2 areaSize(static_cast<float>(sceneArea.extent.width), static_cast<float>(sceneArea.extent.height));

        Ray ray = screenPointToRay(input.getMousePosition() - areaOffset, areaSize, camera.getProjection(), camera.getView());
        selectedId = scene.pickObject(ray);
    }

    LveGameObject* LveEditor::getSelected(LveScene &scene) const {
        return selectedId ? scene.findGameObjectById(*selectedId) : nullptr;
    }
}