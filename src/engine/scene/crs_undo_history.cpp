#include "engine/scene/crs_undo_history.hpp"

#include "engine/scene/scene_serializer.hpp"

namespace crsdk {

    static size_t tilesBytes(const std::vector<TileSnapshot> &tiles) {
        size_t bytes = 0;
        for (const TileSnapshot &s : tiles) {
            bytes += s.tile.heights.size() * sizeof(float) + s.tile.paint.size() * sizeof(LayerMix);
        }
        return bytes;
    }

    void UndoHistory::beginSceneEdit(const CrsScene &before) {
        pendingBefore = before;
        pendingFingerprint = sceneFingerprint(before);
    }

    bool UndoHistory::endSceneEdit(const CrsScene &after, const std::string &label) {
        if (!pendingBefore) return false;
        std::optional<CrsScene> before = std::move(pendingBefore);
        pendingBefore.reset();
        if (sceneFingerprint(after) == pendingFingerprint) return false;  //nada mudou de verdade

        Step step{};
        step.label = label;
        step.sceneBefore = std::move(before);
        step.sceneAfter = after;
        step.bytes = 64 * 1024;  //uma cena pequena (estimativa: as cenas não guardam o terreno)
        push(std::move(step));
        return true;
    }

    void UndoHistory::pushTerrainStroke(CrsGameObject::id_t terrainId, std::vector<TileSnapshot> before, std::vector<TileSnapshot> after,
                                        std::vector<TerrainLayer> layersBefore, std::vector<TerrainLayer> layersAfter, const std::string &label) {
        if (before.empty()) return;  //a pincelada não mudou nada (ex: apagou um material que não tinha)
        Step step{};
        step.label = label;
        step.terrainId = terrainId;
        step.bytes = tilesBytes(before) + tilesBytes(after);
        step.tilesBefore = std::move(before);
        step.tilesAfter = std::move(after);
        step.layersBefore = std::move(layersBefore);
        step.layersAfter = std::move(layersAfter);
        push(std::move(step));
    }

    void UndoHistory::push(Step step) {
        //ação nova: o que tinha sido desfeito não dá mais pra refazer
        undone.clear();
        done.push_back(std::move(step));
        size_t bytes = 0;
        for (const Step &s : done) bytes += s.bytes;
        while (!done.empty() && (done.size() > MAX_STEPS || bytes > MAX_BYTES)) {
            bytes -= done.front().bytes;
            done.erase(done.begin());
        }
    }

    void UndoHistory::apply(const Step &step, CrsScene &scene, bool before) {
        if (step.sceneBefore) {
            scene = before ? *step.sceneBefore : *step.sceneAfter;
            return;
        }
        //pincelada: os pedaços e a paleta do terreno (se o terreno ainda existe)
        CrsGameObject *obj = scene.findGameObjectById(step.terrainId);
        if (!obj || !obj->terrain) return;
        obj->terrain->data->restoreTiles(before ? step.tilesBefore : step.tilesAfter);
        obj->terrain->layers = before ? step.layersBefore : step.layersAfter;
    }

    std::optional<std::string> UndoHistory::undo(CrsScene &scene) {
        if (done.empty()) return std::nullopt;
        Step step = std::move(done.back());
        done.pop_back();
        apply(step, scene, true);
        std::string label = step.label;
        undone.push_back(std::move(step));
        return label;
    }

    std::optional<std::string> UndoHistory::redo(CrsScene &scene) {
        if (undone.empty()) return std::nullopt;
        Step step = std::move(undone.back());
        undone.pop_back();
        apply(step, scene, false);
        std::string label = step.label;
        done.push_back(std::move(step));
        return label;
    }

    void UndoHistory::clear() {
        done.clear();
        undone.clear();
        pendingBefore.reset();
    }
}
