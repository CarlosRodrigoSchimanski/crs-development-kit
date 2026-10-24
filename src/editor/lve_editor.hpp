#pragma once

#include "engine/core/lve_input.hpp"
#include "engine/rendering/lve_camera.hpp"
#include "engine/scene/lve_scene.hpp"

#include <optional>

namespace lve {
    class LveEditor {
        public:
            void updateSelection(LveScene &scene, const LveInput &input, const LveCamera &camera, VkRect2D sceneArea);

            LveGameObject* getSelected(LveScene &scene) const;
        
        private:
            std::optional<LveGameObject::id_t> selectedId;
    }; 
}