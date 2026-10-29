#pragma once

#include "engine/animation/lve_animation_library.hpp"
#include "engine/core/lve_device.hpp"
#include "engine/rendering/lve_buffer.hpp"
#include "engine/scene/lve_scene.hpp"

#include <memory>
#include <vector>

namespace lve {
    //matrizes dos ossos de cada objeto animado, num storage buffer por frame (set 0, binding 3)
    class SkinningSystem {
        public:
            SkinningSystem(LveDevice &device, LveAnimationLibrary &animationLibrary);

            SkinningSystem(const SkinningSystem&) = delete;
            SkinningSystem& operator=(const SkinningSystem&) = delete;

            //calcula a pose de cada objeto com AnimatorComponent e esqueleto e escreve as matrizes no
            //buffer do frame, uma fatia por objeto (o boneOffset dele diz onde começa). Se não couber,
            //recria o buffer maior e devolve true: aí quem chamou TEM que apontar o descriptor (binding 3) pro novo
            [[nodiscard]] bool update(LveScene &scene, int frameIndex);

            //o buffer de ossos de um frame, pro descriptor do set global (binding 3)
            VkDescriptorBufferInfo boneBufferInfo(int frameIndex) const;

        private:
            std::unique_ptr<LveBuffer> createBoneBuffer(size_t capacity);
            //a animação que o animator pede (nullptr = nenhuma)
            const AnimationClip *findClip(const LveModel &model, const AnimatorComponent &animator);

            LveDevice &lveDevice;
            LveAnimationLibrary &animationLibrary;
            //um por frame em voo: a CPU reescreve todo frame, e o frame anterior pode estar sendo lido
            std::vector<std::unique_ptr<LveBuffer>> boneBuffers;
    };
}
