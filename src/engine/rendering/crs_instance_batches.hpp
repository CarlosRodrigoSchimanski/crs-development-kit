#pragma once

#include "engine/core/crs_device.hpp"
#include "engine/rendering/crs_buffer.hpp"
#include "engine/rendering/crs_swap_chain.hpp"
#include "engine/scene/crs_scene.hpp"

#include <array>
#include <memory>
#include <vector>

namespace crsdk {
    //o que cada cópia (instância) leva pro shader: a matriz do objeto no mundo e a das normais.
    //Tem que bater com as entradas de location 4..11 do simple_shader.vert (e 4..7 do shadow.vert)
    struct InstanceData {
        glm::mat4 modelMatrix{1.f};
        glm::mat4 normalMatrix{1.f};
    };

    //INSTANCING: junta os objetos parados (sem animação) que usam o MESMO modelo e a mesma textura, e
    //desenha cada grupo numa chamada só, com N cópias (a GPU lê a matriz de cada cópia numa lista).
    //100 paredes iguais = 1 chamada, não 100. Pra quem usa nada muda: cada cópia continua um objeto
    //normal da cena; o agrupamento é refeito a cada frame
    class CrsInstanceBatches {
        public:
            //um grupo: o modelo (e a textura) e onde as matrizes dele começam na lista
            struct Batch {
                CrsGameObject *first;    //um dos objetos (o modelo e a textura do grupo vêm dele)
                uint32_t firstInstance;  //posição da primeira matriz do grupo na lista
                uint32_t count;          //quantas cópias
            };

            explicit CrsInstanceBatches(CrsDevice &device);

            CrsInstanceBatches(const CrsInstanceBatches &) = delete;
            CrsInstanceBatches &operator=(const CrsInstanceBatches &) = delete;

            //agrupa os objetos da cena que têm modelo, não são só do editor e não estão animados, e
            //escreve as matrizes na lista deste frame. Chamar uma vez por frame, antes de desenhar
            const std::vector<Batch> &build(CrsScene &scene, int frameIndex);
            //liga a lista deste frame como o buffer de vértice 'binding' (o modelo fica no 0)
            void bind(VkCommandBuffer commandBuffer, int frameIndex, uint32_t binding = 1) const;

            //a descrição do buffer das cópias pra pipeline: um InstanceData por cópia
            static VkVertexInputBindingDescription bindingDescription(uint32_t binding = 1);
            //as entradas das matrizes: a do objeto em 4..7 e (withNormalMatrix) a das normais em 8..11
            static std::vector<VkVertexInputAttributeDescription> attributeDescriptions(bool withNormalMatrix, uint32_t binding = 1);

        private:
            CrsDevice &crsDevice;
            //uma lista por frame em voo: a do frame que a GPU ainda está desenhando nunca é tocada
            std::array<std::unique_ptr<CrsBuffer>, CrsSwapChain::MAX_FRAMES_IN_FLIGHT> buffers{};
            std::array<uint32_t, CrsSwapChain::MAX_FRAMES_IN_FLIGHT> capacity{};
            std::vector<Batch> batches;
            std::vector<InstanceData> instances;
    };
}
