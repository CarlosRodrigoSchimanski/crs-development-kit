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

    //desenha os objetos com TerrainComponent, em quadradinhos de 32 m (64 por pedaço), como o WoW:
    //cada quadradinho sabe quais materiais da paleta aparecem nele e só pede esses. Um material só é
    //carregado quando algum quadradinho desenhado precisa dele, e sai da memória de vídeo quando nenhum
    //usa mais (pintou por cima até ele sumir, ou tirou da paleta).
    //A malha mora num buffer que a CPU escreve; esculpir/pintar muda a versão do pedaço, e cada frame em
    //voo reescreve o buffer dele quando está atrasado (o da GPU desenhando agora nunca é tocado).
    //As camadas são misturadas no shader pela altura de cada material (displacement)
    class TerrainRenderSystem {
        public:
            TerrainRenderSystem(CrsDevice &device, VkFormat colorFormat, VkFormat depthFormat, VkDescriptorSetLayout globalSetLayout);

            TerrainRenderSystem(const TerrainRenderSystem &) = delete;
            TerrainRenderSystem &operator=(const TerrainRenderSystem &) = delete;

            void render(FrameInfo &frameInfo, const TerrainOverlay &overlay);

            //quantos materiais estão na memória de vídeo agora (pro painel de estatísticas)
            size_t loadedMaterialCount() const;

        private:
            void createPipeline(VkFormat colorFormat, VkFormat depthFormat);
            void createIndexBuffer();

            //os materiais de cada quadradinho (índices na paleta, na ordem das posições do shader)
            using ChunkLayers = std::array<std::vector<uint8_t>, TERRAIN_CHUNKS_PER_TILE * TERRAIN_CHUNKS_PER_TILE>;
            struct TileMesh {
                std::array<std::unique_ptr<CrsBuffer>, CrsSwapChain::MAX_FRAMES_IN_FLIGHT> vertices{};
                std::array<uint64_t, CrsSwapChain::MAX_FRAMES_IN_FLIGHT> versions{};
                std::array<bool, CrsSwapChain::MAX_FRAMES_IN_FLIGHT> written{};
                //a lista de cada quadradinho junto com o buffer do mesmo frame (os pesos do vértice seguem ela)
                std::array<ChunkLayers, CrsSwapChain::MAX_FRAMES_IN_FLIGHT> layers{};
            };
            struct TerrainMesh {
                //segura as alturas vivas enquanto a malha existir (o endereço não pode ser reaproveitado)
                std::shared_ptr<TerrainData> data;
                std::vector<TileMesh> tiles;
                uint64_t lastUsedFrame = 0;
            };
            //preenche o buffer deste frame com as alturas e a pintura atuais do pedaço (e a lista de cada quadradinho)
            void writeTile(const TerrainData &data, int tileX, int tileZ, CrsBuffer &buffer, ChunkLayers &layers);

            //os mapas de um material na GPU (o que falta aponta pra um neutro); carregado uma vez por pasta
            struct Material {
                std::vector<std::unique_ptr<CrsTexture>> textures;
                const CrsTexture *albedo = nullptr;
                const CrsTexture *normal = nullptr;
                const CrsTexture *roughness = nullptr;
                const CrsTexture *ao = nullptr;
                const CrsTexture *displacement = nullptr;
                bool loaded = false;  //false = pasta sem a cor ou arquivo com erro
                uint64_t lastUsedFrame = 0;
            };
            Material &getMaterial(const std::string &folder);
            //o set 1 de uma lista de materiais (até 8 posições); guardado pela lista de pastas
            struct LayerSet {
                VkDescriptorSet set = VK_NULL_HANDLE;
                std::vector<std::string> folders;
                uint64_t lastUsedFrame = 0;
            };
            VkDescriptorSet getLayerSet(const std::vector<std::string> &folders);
            //solta da memória os sets e materiais que ninguém desenhou faz tempo
            void releaseUnused();

            CrsDevice &crsDevice;
            CrsPipelineLayout pipelineLayout;
            std::unique_ptr<CrsPipeline> pipeline;
            std::unique_ptr<CrsDescriptorSetLayout> materialSetLayout;
            std::unique_ptr<CrsDescriptorPool> materialPool;
            //mapas neutros de 1 pixel: posição sem material, ou mapa que o material não tem
            std::unique_ptr<CrsTexture> whiteColor;   //cor branca
            std::unique_ptr<CrsTexture> whiteData;    //roughness 1 (áspero), AO 1 (sem fresta)
            std::unique_ptr<CrsTexture> flatNormal;   //normal map reto (128, 128, 255): sem relevo
            std::unique_ptr<CrsTexture> midHeight;    //displacement no meio (0.5)
            std::unordered_map<std::string, Material> materials;
            std::unordered_map<std::string, LayerSet> layerSets;

            //os triângulos são iguais em todo quadradinho: um índice só pra todos
            std::unique_ptr<CrsBuffer> indexBuffer;
            uint32_t indexCount = 0;

            std::unordered_map<const TerrainData *, TerrainMesh> meshes;
            uint64_t frameCounter = 0;
    };
}
