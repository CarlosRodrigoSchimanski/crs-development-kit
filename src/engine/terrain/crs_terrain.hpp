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

    struct TerrainTile {
        //alturas em metros, linha por linha: a altura (x, z) do pedaço fica em [z * 257 + x]
        std::vector<float> heights = std::vector<float>(TERRAIN_TILE_SAMPLES * TERRAIN_TILE_SAMPLES, 0.f);
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

            //um arquivo por pedaço (tile_x_z.bin) na pasta (os de um tamanho antigo maior são apagados);
            //carregar: pedaço sem arquivo fica plano
            void save(const std::string &folder) const;
            static TerrainData load(const std::string &folder, int tilesPerSide);

        private:
            //chama change(alturaAtual, peso) pra cada amostra dentro do círculo e grava o resultado
            template <typename Change>
            void applyBrush(glm::vec2 center, float radius, Change change);

            int tiles;
            std::vector<TerrainTile> tileData;  //linha por linha: o pedaço (x, z) fica em [z * tiles + x]
    };
}
