#pragma once

#include "engine/rendering/crs_camera.hpp"
#include "engine/core/crs_device.hpp"
#include "engine/rendering/crs_pipeline.hpp"
#include "engine/scene/crs_game_object.hpp"
#include "engine/rendering/crs_frameInfo.hpp"
#include "engine/rendering/crs_buffer.hpp"
#include "engine/rendering/crs_swap_chain.hpp"

#include <memory>
#include <vector>

namespace crsdk {
    class PointLightSystem {
        public:

            PointLightSystem(CrsDevice& device, VkFormat colorFormat, VkFormat depthFormat, VkDescriptorSetLayout globalSetLayout);

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
            std::unique_ptr<CrsBuffer> createLightBuffer(size_t capacity);

            CrsDevice& crsDevice;
            std::unique_ptr<CrsPipeline> crsPipeline;
            CrsPipelineLayout pipelineLayout;
            //um por frame em voo: a CPU reescreve todo frame, e o frame anterior pode estar sendo lido
            std::vector<std::unique_ptr<CrsBuffer>> lightBuffers;
    };
}
