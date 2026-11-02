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
        //editor: o trecho desenha até 8 materiais (dá pra ver o excesso) e o que passa de 4 ganha a
        //aura de alerta. Fora dele (Play, jogo exportado): só os 4 que mais cobrem, sem aura
        bool editor = false;
        float contourInterval = 0.f;  //metros entre as curvas de nível; 0 = sem curvas
        float brushRadius = 0.f;      //0 = sem pincel
        glm::vec3 brushCenter{0.f};   //no mundo
    };

    //desenha os objetos com TerrainComponent, em quadradinhos de 32 m (64 por pedaço), como o WoW:
    //cada quadradinho sabe quais materiais da paleta aparecem nele e só pede esses. Um material só é
    //carregado quando algum quadradinho desenhado precisa dele, e sai da memória de vídeo quando nenhum
    //usa mais (pintou por cima até ele sumir, ou tirou da paleta).
    //A malha mora num buffer que a CPU escreve; esculpir muda a versão do pedaço, e cada frame em voo
    //reescreve o buffer dele quando está atrasado (o da GPU desenhando agora nunca é tocado).
    //A PINTURA (4 pontos por metro) fica numa imagem por pedaço na GPU, a "splat": 8 x 8 regiões de
    //129 x 129, uma por quadradinho, com os pesos na ordem da lista de materiais dele (2 camadas RGBA =
    //8 pesos). Pintar reenvia só as regiões dos quadradinhos que mudaram (prepare, antes do passe).
    //As camadas são misturadas no shader pela altura de cada material (displacement)
    class TerrainRenderSystem {
        public:
            TerrainRenderSystem(CrsDevice &device, VkFormat colorFormat, VkFormat depthFormat, VkDescriptorSetLayout globalSetLayout);

            TerrainRenderSystem(const TerrainRenderSystem &) = delete;
            TerrainRenderSystem &operator=(const TerrainRenderSystem &) = delete;

            //envia pra GPU a pintura que mudou: chamar FORA do passe de desenho (cópias não podem
            //acontecer dentro dele), antes do render do mesmo frame
            void prepare(FrameInfo &frameInfo, const TerrainOverlay &overlay);
            void render(FrameInfo &frameInfo, const TerrainOverlay &overlay);

            //quantos materiais estão na memória de vídeo agora (pro painel de estatísticas)
            size_t loadedMaterialCount() const;

        private:
            void createPipeline(VkFormat colorFormat, VkFormat depthFormat);
            void createIndexBuffer();

            //os materiais de cada quadradinho (índices na paleta, na ordem das posições do shader)
            using ChunkLayers = std::array<std::vector<uint8_t>, TERRAIN_CHUNKS_PER_TILE * TERRAIN_CHUNKS_PER_TILE>;
            static constexpr size_t CHUNKS = TERRAIN_CHUNKS_PER_TILE * TERRAIN_CHUNKS_PER_TILE;
            struct TileMesh {
                //a malha (só as alturas), um buffer por frame em voo
                std::array<std::unique_ptr<CrsBuffer>, CrsSwapChain::MAX_FRAMES_IN_FLIGHT> vertices{};
                std::array<uint64_t, CrsSwapChain::MAX_FRAMES_IN_FLIGHT> versions{};
                std::array<bool, CrsSwapChain::MAX_FRAMES_IN_FLIGHT> written{};
                //a pintura: uma imagem só (a cópia nova espera os frames anteriores pararem de ler)
                CrsAllocatedImage splat;
                CrsImageView splatView;
                VkDescriptorSet splatSet = VK_NULL_HANDLE;
                bool splatReady = false;  //já saiu do layout inicial
                //por quadradinho: a versão da pintura que está na imagem, se já foi enviado, a lista de
                //materiais (a ordem dos pesos na imagem) e se passou do limite (a aura do editor)
                std::array<uint64_t, CHUNKS> chunkVersions{};
                std::array<bool, CHUNKS> uploaded{};
                ChunkLayers layers{};
                std::array<bool, CHUNKS> overLimit{};
                //com quantos materiais por trecho a imagem foi escrita (trocar editor/Play reenvia tudo)
                int maxLayers = 0;
            };
            struct TerrainMesh {
                //segura as alturas vivas enquanto a malha existir (o endereço não pode ser reaproveitado)
                std::shared_ptr<TerrainData> data;
                std::vector<TileMesh> tiles;
                uint64_t lastUsedFrame = 0;
            };
            //preenche o buffer deste frame com as alturas atuais do pedaço
            void writeTile(const TerrainData &data, int tileX, int tileZ, CrsBuffer &buffer);
            //a malha de um terreno (criada na primeira vez que ele aparece)
            TerrainMesh &meshFor(const std::shared_ptr<TerrainData> &data);
            void createSplat(TileMesh &tile);
            //solta os sets de pintura de uma malha que vai sair
            void releaseMesh(TerrainMesh &mesh);

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
            //set 2: a imagem de pintura do pedaço
            std::unique_ptr<CrsDescriptorSetLayout> splatSetLayout;
            std::unique_ptr<CrsDescriptorPool> splatPool;
            CrsSampler splatSampler;
            //onde a CPU escreve a pintura antes da cópia pra imagem, um por frame em voo
            std::array<std::unique_ptr<CrsBuffer>, CrsSwapChain::MAX_FRAMES_IN_FLIGHT> staging{};
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
