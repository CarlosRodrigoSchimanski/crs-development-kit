#pragma once

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace crsdk {
    //cada pedaço (tile) do terreno: 256 x 256 quadrados de 1 m, ou seja 257 x 257 alturas.
    //A borda é a MESMA altura nos dois pedaços vizinhos (por isso 256 + 1): sem rachadura na divisa
    constexpr int TERRAIN_TILE_CELLS = 256;
    constexpr int TERRAIN_TILE_SAMPLES = TERRAIN_TILE_CELLS + 1;
    constexpr float TERRAIN_CELL_SIZE = 1.f;
    constexpr float TERRAIN_TILE_SIZE = TERRAIN_TILE_CELLS * TERRAIN_CELL_SIZE;
    //o tamanho é sempre um número inteiro de pedaços por lado (256 m, 512 m, 768 m...), até este máximo
    //(2048 m: sem culling por pedaço, mais que isso gasta memória de vídeo demais)
    constexpr int TERRAIN_MAX_TILES = 8;

    //pintura, como no WoW: o terreno tem uma PALETA de materiais (a lista dos que já foram usados nele,
    //o [0] é o chão base) e cada ponto guarda só os poucos que tem ali, com quanto de cada um
    constexpr int TERRAIN_LAYERS_PER_POINT = 4;  //pintar um 5º num ponto tira o mais fraco dele
    constexpr int TERRAIN_MAX_PALETTE = 255;     //materiais diferentes num terreno
    //o desenho é feito em quadradinhos de 32 m (64 por pedaço): cada um só pede os materiais que tem
    constexpr int TERRAIN_CHUNK_CELLS = 32;
    constexpr int TERRAIN_CHUNKS_PER_TILE = TERRAIN_TILE_CELLS / TERRAIN_CHUNK_CELLS;
    constexpr int TERRAIN_CHUNK_SAMPLES = TERRAIN_CHUNK_CELLS + 1;
    //quantos materiais um quadradinho mistura no shader (4 por ponto, mas pontos vizinhos podem ter outros)
    constexpr int TERRAIN_CHUNK_LAYERS = 8;
    //o que fica abaixo disso numa pintura é zerado e volta pros outros (resto de pincelada fraca)
    constexpr float TERRAIN_MIN_WEIGHT = 0.02f;

    //os materiais de um ponto: até 4 (índice na paleta, quanto), somando 1; peso 0 = posição vazia
    struct LayerMix {
        std::array<uint8_t, TERRAIN_LAYERS_PER_POINT> ids{};
        std::array<float, TERRAIN_LAYERS_PER_POINT> weights{1.f, 0.f, 0.f, 0.f};  //começa todo no chão base
    };

    struct TerrainTile {
        //alturas em metros, linha por linha: a altura (x, z) do pedaço fica em [z * 257 + x]
        std::vector<float> heights = std::vector<float>(TERRAIN_TILE_SAMPLES * TERRAIN_TILE_SAMPLES, 0.f);
        //a pintura de cada amostra, na mesma ordem das alturas
        std::vector<LayerMix> paint = std::vector<LayerMix>(TERRAIN_TILE_SAMPLES * TERRAIN_TILE_SAMPLES);
        //muda a cada edição: quem guarda uma cópia (malha na GPU, corpo na física) sabe que tem que refazer
        uint64_t version = 0;
    };

    //o relevo do terreno: as alturas, divididas em pedaços. Coordenadas "locais" = metros a partir do
    //canto do terreno (0 até size()); "amostra" = o índice de uma altura no terreno inteiro
    class TerrainData {
        public:
            //plano, na altura 0
            explicit TerrainData(int tilesPerSide);

            int tilesPerSide() const { return tiles; }
            //amostras por lado no terreno inteiro (as bordas compartilhadas contam uma vez só)
            int samplesPerSide() const { return tiles * TERRAIN_TILE_CELLS + 1; }
            float size() const { return static_cast<float>(tiles) * TERRAIN_TILE_SIZE; }

            const TerrainTile &tile(int tileX, int tileZ) const;

            //cópia com outro tamanho, mantendo o canto (0, 0): os pedaços que continuam ficam iguais,
            //os novos (pra +X e +Z) nascem planos, os que sobram saem
            TerrainData resized(int newTilesPerSide) const;

            //altura de uma amostra (fora do terreno: a da borda mais próxima)
            float sample(int x, int z) const;
            //muda uma amostra em todo pedaço que tem ela (1, ou 2 na divisa, ou 4 no canto)
            void setSample(int x, int z, float height);
            //a pintura de uma amostra; mudar grava em todo pedaço que tem ela
            LayerMix mix(int x, int z) const;
            void setMix(int x, int z, const LayerMix &value);
            //quanto do material 'layer' (índice na paleta) tem na amostra (0..1)
            float weight(int x, int z, int layer) const;
            //normal da superfície numa amostra (pelos vizinhos: sem costura na divisa dos pedaços)
            glm::vec3 normalAt(int x, int z) const;

            //altura num ponto qualquer, interpolada pelo mesmo triângulo que a malha e a física usam
            float heightAt(float x, float z) const;

            //raio (origem e direção em coordenadas locais; a altura é a do terreno, não a do mundo):
            //distância até onde ele encosta no chão, vindo de cima
            std::optional<float> raycast(const glm::vec3 &origin, const glm::vec3 &direction, float maxDistance) const;

            //pincéis: centro em coordenadas locais (x, z), raio em metros; o efeito é máximo no meio e
            //some suave até a borda do círculo
            void raise(glm::vec2 center, float radius, float amount);  //amount < 0 abaixa
            void smooth(glm::vec2 center, float radius, float strength);  //strength 0..1 por chamada
            void flatten(glm::vec2 center, float radius, float height, float strength);
            //pinta o material 'layer' da paleta (amount 0..1 por chamada no meio do pincel); amount < 0
            //apaga ele, e o que sai volta pros outros daquele ponto (ou pro chão base, se só tinha ele)
            void paint(glm::vec2 center, float radius, int layer, float amount);
            //tira o material da paleta em todo o terreno: o que estava pintado com ele volta pro chão
            //base, e os de índice maior descem uma posição (a paleta do componente tem que acompanhar)
            void removeLayer(int layer);

            //quanto do terreno cada material da paleta cobre (0..1, somando 1); 'paletteSize' posições
            std::vector<float> layerUsage(int paletteSize) const;
            //os materiais que aparecem num quadradinho de 32 m (com a borda), do que mais cobre pro que
            //menos, no máximo TERRAIN_CHUNK_LAYERS: é a lista que o desenho daquele quadradinho pede
            std::vector<uint8_t> chunkLayers(int tileX, int tileZ, int chunkX, int chunkZ) const;

            //dois arquivos por pedaço na pasta: tile_x_z.bin (alturas) e paint_x_z.bin (pintura: 4 índices
            //e 4 pesos de 1 byte por amostra); os de um tamanho antigo maior são apagados. Carregar: o que
            //não tem arquivo fica plano/sem pintura (lê também a pintura antiga, de 8 camadas fixas)
            void save(const std::string &folder) const;
            static TerrainData load(const std::string &folder, int tilesPerSide);

        private:
            //chama change(alturaAtual, peso) pra cada amostra dentro do círculo e grava o resultado
            template <typename Change>
            void applyBrush(glm::vec2 center, float radius, Change change);
            //o pedaço que guarda a amostra (o último pedaço fica com a borda final) e o índice nele
            size_t sampleIndex(int &x, int &z, int &tileIndex) const;
            //chama write(pedaço, índice) pra cada pedaço que tem a amostra (a divisa é de mais de um)
            template <typename Write>
            void forEachCopy(int x, int z, Write write);

            int tiles;
            std::vector<TerrainTile> tileData;  //linha por linha: o pedaço (x, z) fica em [z * tiles + x]
    };
}
