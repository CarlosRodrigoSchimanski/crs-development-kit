#pragma once

#include "engine/core/crs_device.hpp"
#include "engine/rendering/crs_upload_batch.hpp"

#include <string>
#include <vector>

namespace crsdk {
    class CrsTexture {
        public:
            //os 3 construtores só GRAVAM o envio dos pixels no lote: a textura só pode ser usada
            //depois do batch.submit() (quem cria o lote decide quando enviar)
            CrsTexture(CrsDevice &device, const std::string &filepath, CrsUploadBatch &batch);
            //imagem codificada (png/jpg) já na memória, ex: embutida num .glb
            CrsTexture(CrsDevice &device, const std::vector<unsigned char> &encodedImage, CrsUploadBatch &batch);
            //pixels RGBA crus, ex: textura branca 1x1 de reserva
            CrsTexture(CrsDevice &device, uint32_t width, uint32_t height, const unsigned char *rgbaPixels, CrsUploadBatch &batch);
            //sem destrutor escrito: os membros abaixo (handles RAII) se destroem sozinhos, na ordem
            //inversa da declaração (sampler, view, imagem) — e também se o construtor falhar no meio

            CrsTexture(const CrsTexture&) = delete;
            CrsTexture& operator=(const CrsTexture&) = delete;

            VkImageView getImageView() const;
            VkSampler getSampler() const;

        private:
            //RGBA 8 bits por canal, em sRGB (o shader recebe a cor já convertida pra linear)
            static constexpr VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;

            void upload(uint32_t width, uint32_t height, const unsigned char *pixels, CrsUploadBatch &batch);
            void createTextureImage(uint32_t width, uint32_t height, const unsigned char *pixels, CrsUploadBatch &batch);
            void copyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer buffer, uint32_t width, uint32_t height);
            void createTextureImageView();
            void createTextureSampler();

            CrsDevice &crsDevice;
            CrsAllocatedImage textureImage;
            CrsImageView textureImageView;
            CrsSampler textureSampler;
    };
}
