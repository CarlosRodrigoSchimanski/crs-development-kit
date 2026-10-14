#pragma once

#include "engine/rendering/lve_camera.hpp"
#include "engine/core/lve_device.hpp"
#include "engine/rendering/lve_pipeline.hpp"
#include "engine/scene/lve_game_object.hpp"
#include "engine/rendering/lve_frameInfo.hpp"
#include "engine/rendering/lve_buffer.hpp"
#include "engine/rendering/lve_swap_chain.hpp"

#include <memory>
#include <vector>

namespace lve {
    class PointLightSystem {
        public:

            PointLightSystem(LveDevice& device, VkFormat colorFormat, VkFormat depthFormat, VkDescriptorSetLayout globalSetLayout);

            PointLightSystem(const PointLightSystem&) = delete;
            PointLightSystem& operator=(const PointLightSystem&) = delete;

            //copia as point lights da cena pro storage buffer do frame e põe a quantidade no
            //ubo.numLights. Se não couberem, recria o buffer do frame com o dobro do tamanho e
            //devolve true: aí quem chamou TEM que apontar o descriptor (binding 2) pro buffer novo
            //(nodiscard: o compilador avisa se o retorno for ignorado)
            [[nodiscard]] bool update(FrameInfo& frameInfo, GlobalUbo& ubo);
            void render(FrameInfo& frameInfo);

            //o storage buffer de luzes de um frame, pro descriptor do set global (binding 2)
            VkDescriptorBufferInfo lightBufferInfo(int frameIndex) const;

        private:
            void createPipeline(VkFormat colorFormat, VkFormat depthFormat);
            std::unique_ptr<LveBuffer> createLightBuffer(size_t capacity);

            LveDevice& lveDevice;
            std::unique_ptr<LvePipeline> lvePipeline;
            LvePipelineLayout pipelineLayout;
            //um por frame em voo: a CPU reescreve todo frame, e o frame anterior pode estar sendo lido
            std::vector<std::unique_ptr<LveBuffer>> lightBuffers;
    };
}
