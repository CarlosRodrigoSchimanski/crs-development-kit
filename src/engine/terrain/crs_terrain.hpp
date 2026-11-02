#pragma once

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
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
    //o [0] é o chão base) e cada ponto guarda só os que tem ali, com quanto de cada um
    constexpr int TERRAIN_POINT_SLOTS = 8;    //por ponto (pintar um 9º tira o mais fraco: na prática não acontece)
    constexpr int TERRAIN_MAX_PALETTE = 255;  //materiais diferentes num terreno
    //a pintura tem a PRÓPRIA grade, mais fina que a das alturas: 4 pontos por metro (um a cada 25 cm).
    //Com 1 por metro o spray virava manchas borradas de 1 m; assim sai grão de verdade
    constexpr int TERRAIN_PAINT_PER_METER = 4;
    constexpr float TERRAIN_PAINT_SPACING = TERRAIN_CELL_SIZE / TERRAIN_PAINT_PER_METER;
    constexpr int TERRAIN_PAINT_CELLS = TERRAIN_TILE_CELLS * TERRAIN_PAINT_PER_METER;  //1024 por pedaço
    constexpr int TERRAIN_PAINT_SAMPLES = TERRAIN_PAINT_CELLS + 1;                     //borda compartilhada
    //o desenho é feito em trechos de 32 m (64 por pedaço): cada um só pede os materiais que tem
    constexpr int TERRAIN_CHUNK_CELLS = 32;
    constexpr int TERRAIN_CHUNKS_PER_TILE = TERRAIN_TILE_CELLS / TERRAIN_CHUNK_CELLS;
    constexpr int TERRAIN_CHUNK_SAMPLES = TERRAIN_CHUNK_CELLS + 1;
    constexpr int TERRAIN_CHUNK_PAINT_CELLS = TERRAIN_CHUNK_CELLS * TERRAIN_PAINT_PER_METER;  //128
    constexpr int TERRAIN_CHUNK_PAINT_SAMPLES = TERRAIN_CHUNK_PAINT_CELLS + 1;               //129
    //A REGRA: até 4 materiais por trecho. A engine não tira nada ao pintar: no editor o trecho com
    //mais ganha uma aura de alerta; no Play/jogo exportado ele só desenha os 4 que mais cobrem
    constexpr int TERRAIN_CHUNK_LIMIT = 4;
    //quantos o shader consegue misturar num trecho (o editor mostra até isso, pra dar pra ver o excesso)
    constexpr int TERRAIN_CHUNK_LAYERS = 8;
    //o que fica abaixo disso numa pintura é zerado e volta pros outros (resto de pincelada fraca)
    constexpr float TERRAIN_MIN_WEIGHT = 0.02f;

    //os materiais de um ponto: até 8 (índice na paleta, quanto), somando 1; peso 0 = posição vazia.
    //É a forma de TRABALHAR com um ponto (as contas do pincel); guardado, ele vira um PaintTexel
    struct LayerMix {
        std::array<uint8_t, TERRAIN_POINT_SLOTS> ids{};
        std::array<float, TERRAIN_POINT_SLOTS> weights{1.f};  //começa todo no chão base (o resto fica 0)
    };

    //um ponto da pintura como fica guardado: os mesmos 8 (índice, peso), com o peso em 1 byte (0..255,
    //somando 255). 16 bytes por ponto: com 4 pontos por metro, um pedaço tem 1025 x 1025 deles
    struct PaintTexel {
        std::array<uint8_t, TERRAIN_POINT_SLOTS> ids{};
        std::array<uint8_t, TERRAIN_POINT_SLOTS> weights{255};
        bool operator==(const PaintTexel &) const = default;
    };
    LayerMix unpackTexel(const PaintTexel &texel);
    PaintTexel packTexel(const LayerMix &mix);

    //um material e quanto ele cobre (0..1): o resultado do pincel Checar
    struct LayerShare {
        int layer;
        float share;
    };

    struct TerrainTile {
        //alturas em metros, linha por linha: a altura (x, z) do pedaço fica em [z * 257 + x]
        std::vector<float> heights = std::vector<float>(TERRAIN_TILE_SAMPLES * TERRAIN_TILE_SAMPLES, 0.f);
        //a pintura, na grade dela (4 por metro): o ponto (x, z) do pedaço fica em [z * 1025 + x]
        std::vector<PaintTexel> paint = std::vector<PaintTexel>(TERRAIN_PAINT_SAMPLES * TERRAIN_PAINT_SAMPLES);
        //mudam a cada edição: quem guarda uma cópia sabe que tem que refazer. Separadas: pintar não refaz
        //a malha nem a física, esculpir não reenvia a pintura
        uint64_t version = 0;       //alturas (malha, física)
        uint64_t paintVersion = 0;  //pintura (a imagem dela na GPU)
        //a pintura de cada trecho de 32 m ([z * 8 + x]): a GPU reenvia só o trecho que mudou
        std::array<uint64_t, TERRAIN_CHUNKS_PER_TILE * TERRAIN_CHUNKS_PER_TILE> chunkPaintVersions{};
        //o ponto (local no pedaço) mudou: sobe a versão de todo trecho que tem ele (a divisa é de 2 ou 4)
        void touchPaint(int localX, int localZ);
        void touchAllPaint();
    };

    //histórico (Ctrl+Z): o que uma pincelada mudou, como estava (ou como ficou). As alturas por pedaço
    //(264 KB cada) e a pintura por TRECHO de 32 m (266 KB cada; o pedaço inteiro teria 17 MB)
    struct TileHeightsSnapshot {
        int tile;
        std::vector<float> heights;
    };
    struct PaintChunkSnapshot {
        int tile;
        int chunkX, chunkZ;
        std::vector<PaintTexel> texels;  //129 x 129, com a borda
    };
    struct TerrainSnapshot {
        std::vector<TileHeightsSnapshot> heights;
        std::vector<PaintChunkSnapshot> paint;
        bool empty() const { return heights.empty() && paint.empty(); }
        size_t bytes() const;
    };

    //o relevo do terreno: as alturas, divididas em pedaços. Coordenadas "locais" = metros a partir do
    //canto do terreno (0 até size()); "amostra" = o índice de uma altura no terreno inteiro; "ponto de
    //pintura" = o índice na grade da pintura (4 por metro: o ponto (8, 4) fica em (2 m, 1 m))
    class TerrainData {
        public:
            //plano, na altura 0
            explicit TerrainData(int tilesPerSide);

            int tilesPerSide() const { return tiles; }
            //amostras por lado no terreno inteiro (as bordas compartilhadas contam uma vez só)
            int samplesPerSide() const { return tiles * TERRAIN_TILE_CELLS + 1; }
            int paintSamplesPerSide() const { return tiles * TERRAIN_PAINT_CELLS + 1; }
            float size() const { return static_cast<float>(tiles) * TERRAIN_TILE_SIZE; }

            const TerrainTile &tile(int tileX, int tileZ) const;

            //cópia com outro tamanho, mantendo o canto (0, 0): os pedaços que continuam ficam iguais,
            //os novos (pra +X e +Z) nascem planos, os que sobram saem
            TerrainData resized(int newTilesPerSide) const;

            //altura de uma amostra (fora do terreno: a da borda mais próxima)
            float sample(int x, int z) const;
            //muda uma amostra em todo pedaço que tem ela (1, ou 2 na divisa, ou 4 no canto)
            void setSample(int x, int z, float height);
            //a pintura de um ponto de pintura; mudar grava em todo pedaço que tem ele
            LayerMix mix(int px, int pz) const;
            void setMix(int px, int pz, const LayerMix &value);
            //quanto do material 'layer' (índice na paleta) tem no ponto de pintura (0..1)
            float weight(int px, int pz, int layer) const;
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
            //apaga ele, e o que sai volta pros outros daquele ponto (ou pro chão base, se só tinha ele).
            //spray (0..1): 0 = borda suave (o efeito some liso até a borda); 1 = borda salpicada (indo
            //pra borda, só alguns pontos são pintados, cada vez mais raros, mas com força cheia). Os
            //pontos sorteados dependem do lugar e do 'seed': o editor usa um seed por pincelada, então
            //segurar o pincel reforça os MESMOS pontos (o salpicado não vira liso).
            //Dentro do journal (uma pincelada do editor) as chamadas se somam sem perder nada, mesmo
            //em passos bem pequenos; fora dele cada chamada vale sozinha
            void paint(glm::vec2 center, float radius, int layer, float amount, float spray = 0.f, uint32_t seed = 0);
            //tira o material da paleta em todo o terreno: o que estava pintado com ele volta pro chão
            //base, e os de índice maior descem uma posição (a paleta do componente tem que acompanhar)
            void removeLayer(int layer);
            //o que estava pintado com o material vira chão base, sem mexer no índice de ninguém (o
            //histórico do Ctrl+Z continua valendo: nenhum outro ponto muda de número)
            void mergeIntoBase(int layer);

            //histórico (Ctrl+Z): entre beginJournal e endJournal, a PRIMEIRA mudança de cada pedaço (alturas)
            //e de cada trecho (pintura) guarda uma cópia de como estava antes. endJournal devolve essas cópias
            void beginJournal();
            TerrainSnapshot endJournal();
            //como estão agora as mesmas partes de um snapshot (o "depois" da pincelada, pro Ctrl+Y)
            TerrainSnapshot snapshotLike(const TerrainSnapshot &which) const;
            //põe as partes de volta; as versões sobem (a malha, a física e a pintura na GPU refazem)
            void restore(const TerrainSnapshot &snapshot);

            //quanto do terreno cada material da paleta cobre (0..1, somando 1); 'paletteSize' posições
            std::vector<float> layerUsage(int paletteSize) const;
            //os materiais que aparecem num trecho de 32 m (com a borda), do que mais cobre pro que menos,
            //no máximo 'maxCount': é a lista que o desenho daquele trecho pede (o resto é ignorado)
            std::vector<uint8_t> chunkLayers(int tileX, int tileZ, int chunkX, int chunkZ, int maxCount = TERRAIN_CHUNK_LAYERS) const;
            //quantos materiais diferentes aparecem no trecho (mais que TERRAIN_CHUNK_LIMIT = alerta)
            int chunkLayerCount(int tileX, int tileZ, int chunkX, int chunkZ) const;
            //pincel Checar: os materiais dentro do círculo e quanto cada um cobre ali, do maior pro menor
            std::vector<LayerShare> brushLayers(glm::vec2 center, float radius) const;

            //dois arquivos por pedaço na pasta: tile_x_z.bin (alturas) e paint_x_z.bin (a pintura comprimida:
            //áreas iguais viram um registro só); os de um tamanho antigo maior são apagados. Carregar: o que
            //não tem arquivo fica plano/sem pintura (lê também os formatos antigos, de 1 ponto por metro)
            void save(const std::string &folder) const;
            static TerrainData load(const std::string &folder, int tilesPerSide);

        private:
            //chama change(alturaAtual, peso) pra cada amostra dentro do círculo e grava o resultado
            template <typename Change>
            void applyBrush(glm::vec2 center, float radius, Change change);
            //o pedaço que guarda a amostra (o último pedaço fica com a borda final) e o índice nele
            size_t sampleIndex(int &x, int &z, int &tileIndex) const;
            size_t paintIndex(int &px, int &pz, int &tileIndex) const;
            //chama write(pedaço, índice) pra cada pedaço que tem a amostra (a divisa é de mais de um)
            template <typename Write>
            void forEachCopy(int x, int z, Write write);
            template <typename Write>
            void forEachPaintCopy(int px, int pz, Write write);
            //histórico: guarda o trecho do ponto de pintura (local no pedaço), se ainda não guardou
            void journalPaint(size_t tileIndex, int localX, int localZ);

            int tiles;
            std::vector<TerrainTile> tileData;  //linha por linha: o pedaço (x, z) fica em [z * tiles + x]
            //o journal do histórico (ver beginJournal)
            bool journaling = false;
            TerrainSnapshot journal;
            std::vector<bool> journaledTiles;   //alturas, por pedaço
            std::vector<bool> journaledChunks;  //pintura, por trecho ([pedaço * 64 + trecho])
            //a pincelada em andamento (dentro do journal): cada ponto tocado guarda como estava antes e
            //quanto já recebeu, em float. O ponto é sempre refeito a partir do original: a 144 fps cada
            //frame soma ~0,017, que em 1 byte (e com o corte de 2%) se perderia frame a frame
            struct StrokePoint {
                PaintTexel original;
                float amount = 0.f;
            };
            std::unordered_map<uint64_t, StrokePoint> strokePoints;
    };
}
