#pragma once

#include "engine/core/crs_device.hpp"
#include "engine/rendering/crs_buffer.hpp"

#include <memory>
#include <vector>

namespace crsdk {

    //lote de envio de dados pra GPU: junta várias cópias (vértices, índices, texturas...) num
    //command buffer só e envia tudo de uma vez no submit(), com UMA espera em vez de uma por cópia.
    //Os staging buffers ficam guardados aqui até o submit: a GPU lê deles durante a cópia,
    //então não podem ser destruídos antes dela terminar
    class CrsUploadBatch {
        public:
            //já abre o command buffer: dá pra começar a gravar cópias logo em seguida
            explicit CrsUploadBatch(CrsDevice &device);
            //se o submit não foi chamado (ex: exceção no meio do carregamento), descarta o lote sem enviar
            ~CrsUploadBatch();

            CrsUploadBatch(const CrsUploadBatch &) = delete;
            CrsUploadBatch &operator=(const CrsUploadBatch &) = delete;

            //copia os dados pra um staging buffer (memória que a CPU enxerga) e devolve ele; quem
            //chama grava a cópia staging -> destino no commandBuffer() (ex: textura, com barreiras)
            VkBuffer stage(const void *data, VkDeviceSize size);
            //atalho pro caso comum: stage + cópia pra um buffer inteiro (vértices, índices)
            void copyToBuffer(VkBuffer dst, const void *data, VkDeviceSize size);

            VkCommandBuffer commandBuffer() const;

            //envia tudo, espera a GPU terminar e libera os staging buffers; o lote não pode mais ser usado
            void submit();

        private:
            CrsDevice &crsDevice;
            VkCommandBuffer commandBuffer_;
            std::vector<std::unique_ptr<CrsBuffer>> stagingBuffers;
            bool submitted{false};
    };
}
