#pragma once

#include "engine/scene/crs_scene.hpp"

#include <optional>
#include <string>
#include <vector>

namespace crsdk {
    //o histórico do Ctrl+Z / Ctrl+Y do editor. Dois tipos de passo:
    //  - CENA: uma cópia da cena antes e depois da ação (mover, girar, mudar um campo, adicionar,
    //    apagar, mudar o tamanho do terreno...). Barato: a cena é pequena e o terreno não é copiado
    //    (ela guarda só o ponteiro das alturas dele);
    //  - PINCELADA no terreno: só os pedaços que ela tocou, como estavam antes e como ficaram, e a
    //    paleta de materiais (copiar o terreno inteiro a cada pincelada pesaria dezenas de MB).
    //Desfazer na ordem certa (do último pro primeiro) é o que deixa os dois tipos sempre de acordo
    class UndoHistory {
        public:
            //ação na cena que pode durar vários frames (arrastar uma seta ou um campo): o começo guarda
            //a cena como estava; o fim vira um passo, se algo mudou de verdade
            void beginSceneEdit(const CrsScene &before);
            bool sceneEditOpen() const { return pendingBefore.has_value(); }
            //true = virou um passo (false = nada mudou, ex: clicou num objeto do outliner)
            bool endSceneEdit(const CrsScene &after, const std::string &label);

            //uma pincelada terminada no terreno do objeto 'terrainId'
            void pushTerrainStroke(CrsGameObject::id_t terrainId, std::vector<TileSnapshot> before, std::vector<TileSnapshot> after,
                                   std::vector<TerrainLayer> layersBefore, std::vector<TerrainLayer> layersAfter, const std::string &label);

            //desfaz/refaz o último passo; devolve o nome dele (vazio = não tinha o que desfazer/refazer)
            std::optional<std::string> undo(CrsScene &scene);
            std::optional<std::string> redo(CrsScene &scene);

            //outra cena aberta: o histórico da anterior não vale mais
            void clear();
            size_t undoCount() const { return done.size(); }
            size_t redoCount() const { return undone.size(); }

            //limites: os passos mais antigos saem primeiro
            static constexpr size_t MAX_STEPS = 200;
            static constexpr size_t MAX_BYTES = 768ull * 1024 * 1024;  //as pinceladas guardam pedaços do terreno

        private:
            struct Step {
                std::string label;
                //passo de cena
                std::optional<CrsScene> sceneBefore;
                std::optional<CrsScene> sceneAfter;
                //passo de pincelada
                CrsGameObject::id_t terrainId = 0;
                std::vector<TileSnapshot> tilesBefore;
                std::vector<TileSnapshot> tilesAfter;
                std::vector<TerrainLayer> layersBefore;
                std::vector<TerrainLayer> layersAfter;
                size_t bytes = 0;
            };
            void push(Step step);
            //aplica um lado do passo (before = desfazer)
            static void apply(const Step &step, CrsScene &scene, bool before);

            std::vector<Step> done;    //o que dá pra desfazer (o último é o mais recente)
            std::vector<Step> undone;  //o que dá pra refazer
            std::optional<CrsScene> pendingBefore;
            std::string pendingFingerprint;
    };
}
