#pragma once

#include "engine/core/crs_device.hpp"
#include "engine/core/crs_handle.hpp"
#include "engine/rendering/crs_buffer.hpp"
#include "engine/rendering/crs_descriptors.hpp"
#include "engine/rendering/crs_pipeline.hpp"
#include "engine/rendering/crs_texture.hpp"

#include <imgui.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace crsdk {
    //a esfera de prévia de um material, como no Blender e no Poly Haven: desenhada UMA vez por material
    //numa imagem pequena, com luzes de estúdio, e mostrada no editor como uma imagem do ImGui.
    //Os mapas do material são carregados só pra desenhar e soltos logo depois; fica só a miniatura.
    //(O Asset Manager vai usar a mesma esfera na grade de materiais)
    class MaterialPreviews {
        public:
            explicit MaterialPreviews(CrsDevice &device);
            ~MaterialPreviews();

            MaterialPreviews(const MaterialPreviews &) = delete;
            MaterialPreviews &operator=(const MaterialPreviews &) = delete;

            //a miniatura do material (pasta em assets/materials). Na primeira vez pede o desenho e devolve
            //0 (ainda não tem); nos frames seguintes devolve a imagem pronta pro ImGui::Image
            ImTextureID get(const std::string &folder);

            //desenha as miniaturas pedidas: chamar no command buffer do frame, FORA de qualquer passe de
            //desenho (antes do passe da tela), e depois que a interface já pediu o que precisava
            void render(VkCommandBuffer commandBuffer);

            static constexpr uint32_t SIZE = 256;  //pixels de lado (mostrada menor: a borda fica lisa)

        private:
            void createSphere();
            void createPipeline();

            struct Preview {
                CrsAllocatedImage color;
                CrsImageView colorView;
                CrsAllocatedImage depth;
                CrsImageView depthView;
                VkDescriptorSet imguiSet = VK_NULL_HANDLE;  //a imagem como o ImGui enxerga
                bool ready = false;
                //os mapas e o set usados no desenho: soltos alguns frames depois (a GPU ainda pode estar desenhando)
                std::vector<std::unique_ptr<CrsTexture>> textures;
                VkDescriptorSet materialSet = VK_NULL_HANDLE;
                uint64_t drawnFrame = 0;
            };
            //desenha uma miniatura no command buffer (carrega os mapas na hora)
            void draw(VkCommandBuffer commandBuffer, const std::string &folder, Preview &preview);

            CrsDevice &crsDevice;
            VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
            std::unique_ptr<CrsBuffer> vertexBuffer;
            std::unique_ptr<CrsBuffer> indexBuffer;
            uint32_t indexCount = 0;
            std::unique_ptr<CrsDescriptorSetLayout> materialSetLayout;
            std::unique_ptr<CrsDescriptorPool> materialPool;
            CrsPipelineLayout pipelineLayout;
            std::unique_ptr<CrsPipeline> pipeline;
            CrsSampler sampler;  //como o ImGui lê a miniatura
            //neutros pro mapa que o material não tem
            std::unique_ptr<CrsTexture> whiteData;
            std::unique_ptr<CrsTexture> flatNormal;

            std::unordered_map<std::string, Preview> previews;
            std::vector<std::string> pending;
            uint64_t frameCounter = 0;
    };
}
