#pragma once

#include "engine/core/crs_device.hpp"
#include "engine/rendering/crs_buffer.hpp"
#include "engine/rendering/crs_descriptors.hpp"
#include "engine/rendering/crs_texture.hpp"
#include "engine/rendering/crs_frameInfo.hpp"
#include "engine/rendering/crs_pipeline.hpp"
#include "engine/rendering/crs_swap_chain.hpp"

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace crsdk {
    //o que o editor desenha por cima do terreno (no Play fica tudo desligado)
    struct TerrainOverlay {
        float contourInterval = 0.f;  //metros entre as curvas de nível; 0 = sem curvas
        float brushRadius = 0.f;      //0 = sem pincel
        glm::vec3 brushCenter{0.f};   //no mundo
    };

    //desenha os objetos com TerrainComponent: uma malha por pedaço (257 x 257 vértices). A malha mora
    //num buffer que a CPU escreve; esculpir muda a versão do pedaço, e cada frame em voo reescreve o
    //buffer dele quando está atrasado (o da GPU desenhando agora nunca é tocado)
    class TerrainRenderSystem {
        public:
            TerrainRenderSystem(CrsDevice &device, VkFormat colorFormat, VkFormat depthFormat, VkDescriptorSetLayout globalSetLayout);

            TerrainRenderSystem(const TerrainRenderSystem &) = delete;
            TerrainRenderSystem &operator=(const TerrainRenderSystem &) = delete;

            void render(FrameInfo &frameInfo, const TerrainOverlay &overlay);

        private:
            void createPipeline(VkFormat colorFormat, VkFormat depthFormat);
            void createIndexBuffer();

            struct TileMesh {
                std::array<std::unique_ptr<CrsBuffer>, CrsSwapChain::MAX_FRAMES_IN_FLIGHT> vertices{};
                std::array<uint64_t, CrsSwapChain::MAX_FRAMES_IN_FLIGHT> versions{};
                std::array<bool, CrsSwapChain::MAX_FRAMES_IN_FLIGHT> written{};
            };
            struct TerrainMesh {
                //segura as alturas vivas enquanto a malha existir (o endereço não pode ser reaproveitado)
                std::shared_ptr<TerrainData> data;
                std::vector<TileMesh> tiles;
                uint64_t lastUsedFrame = 0;
            };
            //preenche o buffer deste frame com as alturas atuais do pedaço
            void writeTile(const TerrainData &data, int tileX, int tileZ, CrsBuffer &buffer);

            //um material carregado: os mapas na GPU e o descriptor set (set 1) que aponta pra eles
            struct Material {
                std::vector<std::unique_ptr<CrsTexture>> textures;
                VkDescriptorSet set = VK_NULL_HANDLE;
                bool loaded = false;  //false = pasta sem a cor ou arquivo com erro: usa a cor provisória
            };
            //o material da pasta, carregado na primeira vez que aparece (depois fica guardado)
            const Material &getMaterial(const std::string &folder);
            VkDescriptorSet makeMaterialSet(const CrsTexture &albedo, const CrsTexture &normal,
                                            const CrsTexture &roughness, const CrsTexture &ao);

            CrsDevice &crsDevice;
            CrsPipelineLayout pipelineLayout;
            std::unique_ptr<CrsPipeline> pipeline;
            std::unique_ptr<CrsDescriptorSetLayout> materialSetLayout;
            std::unique_ptr<CrsDescriptorPool> materialPool;
            //mapas neutros de 1 pixel pro que o material não tem (e o set do "sem material")
            std::unique_ptr<CrsTexture> whiteTexture;   //cor branca; AO 1 (sem fresta); roughness 1 (áspero)
            std::unique_ptr<CrsTexture> flatNormal;     //normal map reto (128, 128, 255): sem relevo
            VkDescriptorSet neutralSet = VK_NULL_HANDLE;
            std::unordered_map<std::string, Material> materials;

            //os triângulos são iguais em todo pedaço: um índice só pra todos
            std::unique_ptr<CrsBuffer> indexBuffer;
            uint32_t indexCount = 0;

            std::unordered_map<const TerrainData *, TerrainMesh> meshes;
            uint64_t frameCounter = 0;
    };
}
