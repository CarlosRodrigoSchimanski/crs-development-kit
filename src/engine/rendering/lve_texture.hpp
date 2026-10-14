#pragma once

#include "engine/core/lve_device.hpp"
#include "engine/rendering/lve_upload_batch.hpp"

#include <string>
#include <vector>

namespace lve {
    class LveTexture {
        public:
            //os 3 construtores só GRAVAM o envio dos pixels no lote: a textura só pode ser usada
            //depois do batch.submit() (quem cria o lote decide quando enviar)
            LveTexture(LveDevice &device, const std::string &filepath, LveUploadBatch &batch);
            //imagem codificada (png/jpg) já na memória, ex: embutida num .glb
            LveTexture(LveDevice &device, const std::vector<unsigned char> &encodedImage, LveUploadBatch &batch);
            //pixels RGBA crus, ex: textura branca 1x1 de reserva
            LveTexture(LveDevice &device, uint32_t width, uint32_t height, const unsigned char *rgbaPixels, LveUploadBatch &batch);
            //sem destrutor escrito: os membros abaixo (handles RAII) se destroem sozinhos, na ordem
            //inversa da declaração (sampler, view, imagem) — e também se o construtor falhar no meio

            LveTexture(const LveTexture&) = delete;
            LveTexture& operator=(const LveTexture&) = delete;

            VkImageView getImageView() const;
            VkSampler getSampler() const;

        private:
            //RGBA 8 bits por canal, em sRGB (o shader recebe a cor já convertida pra linear)
            static constexpr VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;

            void upload(uint32_t width, uint32_t height, const unsigned char *pixels, LveUploadBatch &batch);
            void createTextureImage(uint32_t width, uint32_t height, const unsigned char *pixels, LveUploadBatch &batch);
            void copyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer buffer, uint32_t width, uint32_t height);
            void createTextureImageView();
            void createTextureSampler();

            LveDevice &lveDevice;
            LveAllocatedImage textureImage;
            LveImageView textureImageView;
            LveSampler textureSampler;
    };
}
