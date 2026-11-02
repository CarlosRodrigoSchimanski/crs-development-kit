#include "engine/scene/crs_undo_history.hpp"

#include <doctest/doctest.h>

using namespace crsdk;

//metros -> índice do ponto de pintura (a grade da pintura tem 4 pontos por metro)
static int P(int meters) { return meters * TERRAIN_PAINT_PER_METER; }

TEST_CASE("terreno: o journal guarda só os pedaços tocados, como estavam antes") {
    TerrainData terrain{2};
    terrain.beginJournal();
    terrain.raise({50.f, 50.f}, 6.f, 3.f);  //só no pedaço (0, 0)
    terrain.raise({60.f, 50.f}, 6.f, 3.f);  //de novo no mesmo: não guarda outra cópia
    TerrainSnapshot before = terrain.endJournal();

    REQUIRE(before.heights.size() == 1);
    CHECK(before.paint.empty());  //esculpir não guarda a pintura
    CHECK(before.heights[0].tile == 0);
    CHECK(before.heights[0].heights[static_cast<size_t>(50 * TERRAIN_TILE_SAMPLES + 50)] == doctest::Approx(0.f));
    CHECK(terrain.sample(50, 50) > 2.f);

    const uint64_t version = terrain.tile(0, 0).version;
    terrain.restore(before);
    CHECK(terrain.sample(50, 50) == doctest::Approx(0.f));
    CHECK(terrain.tile(0, 0).version > version);  //a versão sobe mesmo voltando: a malha refaz
}

TEST_CASE("terreno: fora do journal nada é guardado") {
    TerrainData terrain{1};
    terrain.raise({50.f, 50.f}, 6.f, 3.f);
    terrain.beginJournal();
    CHECK(terrain.endJournal().empty());
}

TEST_CASE("terreno: pincelada na divisa guarda os dois pedaços") {
    TerrainData terrain{2};
    terrain.beginJournal();
    terrain.paint({256.f, 50.f}, 4.f, 1, 1.f);
    const TerrainSnapshot snapshot = terrain.endJournal();
    CHECK(snapshot.heights.empty());  //pintar não guarda as alturas
    //o trecho da borda direita do pedaço (0, 0) e o da borda esquerda do (1, 0)
    REQUIRE(snapshot.paint.size() == 2);
    CHECK(snapshot.paint[0].tile != snapshot.paint[1].tile);
}

TEST_CASE("terreno: pintura guarda só os trechos de 32 m tocados (e a divisa entre trechos, os dois)") {
    TerrainData terrain{1};
    terrain.beginJournal();
    terrain.paint({16.f, 16.f}, 4.f, 1, 1.f);  //no meio do trecho (0, 0)
    terrain.paint({20.f, 16.f}, 4.f, 1, 1.f);  //de novo no mesmo: não guarda outra cópia
    CHECK(terrain.endJournal().paint.size() == 1);

    terrain.beginJournal();
    terrain.paint({32.f, 16.f}, 2.f, 2, 1.f);  //em cima da divisa dos trechos (0, 0) e (1, 0)
    TerrainSnapshot snapshot = terrain.endJournal();
    REQUIRE(snapshot.paint.size() == 2);
    CHECK(snapshot.paint[0].texels.size() == static_cast<size_t>(TERRAIN_CHUNK_PAINT_SAMPLES * TERRAIN_CHUNK_PAINT_SAMPLES));

    //voltar: os dois trechos, inclusive o ponto da divisa, ficam como antes
    terrain.restore(snapshot);
    CHECK(terrain.weight(P(32), P(16), 2) == doctest::Approx(0.f));
    CHECK(terrain.weight(P(31), P(16), 2) == doctest::Approx(0.f));
    CHECK(terrain.weight(P(16), P(16), 1) == doctest::Approx(1.f));  //a pincelada anterior continua
}

TEST_CASE("histórico: desfazer e refazer uma ação na cena") {
    CrsScene scene;
    CrsGameObject &cube = scene.createGameObject();
    const auto id = cube.getId();
    UndoHistory history;

    history.beginSceneEdit(scene);
    cube.transform.translation = {5.f, 0.f, 0.f};
    CHECK(history.endSceneEdit(scene, "Mover"));

    CHECK(history.undo(scene) == std::optional<std::string>{"Mover"});
    CHECK(scene.findGameObjectById(id)->transform.translation.x == doctest::Approx(0.f));
    CHECK(history.redo(scene) == std::optional<std::string>{"Mover"});
    CHECK(scene.findGameObjectById(id)->transform.translation.x == doctest::Approx(5.f));
}

TEST_CASE("histórico: ação que não mudou nada não vira passo") {
    CrsScene scene;
    scene.createGameObject();
    UndoHistory history;
    history.beginSceneEdit(scene);
    CHECK_FALSE(history.endSceneEdit(scene, "Clicar no outliner"));
    CHECK(history.undoCount() == 0);
    CHECK_FALSE(history.undo(scene).has_value());
}

TEST_CASE("histórico: desfazer apagar traz o objeto (e os filhos) de volta") {
    CrsScene scene;
    CrsGameObject &parent = scene.createGameObject();
    parent.name = "Pai";
    CrsGameObject &child = scene.createGameObject();
    child.parentId = parent.getId();
    const auto parentId = parent.getId(), childId = child.getId();
    UndoHistory history;

    history.beginSceneEdit(scene);
    scene.destroyGameObject(parentId);
    history.endSceneEdit(scene, "Apagar Pai");
    REQUIRE(scene.findGameObjectById(parentId) == nullptr);

    history.undo(scene);
    REQUIRE(scene.findGameObjectById(parentId) != nullptr);
    CHECK(scene.findGameObjectById(parentId)->name == "Pai");
    CHECK(scene.findGameObjectById(childId)->parentId == parentId);
}

TEST_CASE("histórico: pincelada no terreno desfaz e refaz as alturas e a paleta") {
    CrsScene scene;
    CrsGameObject &obj = scene.createGameObject();
    obj.terrain.emplace();
    obj.terrain->layers = {{"chao"}};
    UndoHistory history;

    //a pincelada como o editor faz: paleta antes, journal, pinta, journal, paleta depois
    std::vector<TerrainLayer> layersBefore = obj.terrain->layers;
    obj.terrain->data->beginJournal();
    obj.terrain->layers.push_back({"lama"});
    obj.terrain->data->paint({50.f, 50.f}, 6.f, 1, 1.f);
    obj.terrain->data->raise({50.f, 50.f}, 6.f, 2.f);
    TerrainSnapshot before = obj.terrain->data->endJournal();
    TerrainSnapshot after = obj.terrain->data->snapshotLike(before);
    history.pushTerrainStroke(obj.getId(), before, after, layersBefore, obj.terrain->layers, "Pintar");

    history.undo(scene);
    CrsGameObject &back = *scene.findGameObjectById(obj.getId());
    CHECK(back.terrain->data->weight(P(50), P(50), 1) == doctest::Approx(0.f));
    CHECK(back.terrain->data->sample(50, 50) == doctest::Approx(0.f));
    CHECK(back.terrain->layers.size() == 1);

    history.redo(scene);
    CHECK(back.terrain->data->weight(P(50), P(50), 1) == doctest::Approx(1.f));
    CHECK(back.terrain->data->sample(50, 50) == doctest::Approx(2.f));
    CHECK(back.terrain->layers.size() == 2);
}

TEST_CASE("histórico: ação nova depois de desfazer apaga o que dava pra refazer") {
    CrsScene scene;
    CrsGameObject &cube = scene.createGameObject();
    UndoHistory history;
    history.beginSceneEdit(scene);
    cube.transform.translation.x = 1.f;
    history.endSceneEdit(scene, "A");
    history.undo(scene);
    REQUIRE(history.redoCount() == 1);

    history.beginSceneEdit(scene);
    scene.findGameObjectById(cube.getId())->transform.translation.y = 3.f;
    history.endSceneEdit(scene, "B");
    CHECK(history.redoCount() == 0);
    CHECK_FALSE(history.redo(scene).has_value());
}

TEST_CASE("histórico: mudar o tamanho do terreno e depois pintar desfazem na ordem certa") {
    CrsScene scene;
    CrsGameObject &obj = scene.createGameObject();
    obj.terrain.emplace();
    obj.terrain->layers = {{"chao"}};
    const auto id = obj.getId();
    UndoHistory history;

    //1) aumentar: as alturas viram outras (outro ponteiro), como o editor faz
    history.beginSceneEdit(scene);
    obj.terrain->data = std::make_shared<TerrainData>(obj.terrain->data->resized(2));
    history.endSceneEdit(scene, "Tamanho");

    //2) pintar no pedaço novo
    TerrainComponent &terrain = *scene.findGameObjectById(id)->terrain;
    std::vector<TerrainLayer> layersBefore = terrain.layers;
    terrain.data->beginJournal();
    terrain.layers.push_back({"lama"});
    terrain.data->paint({400.f, 400.f}, 6.f, 1, 1.f);
    TerrainSnapshot before = terrain.data->endJournal();
    history.pushTerrainStroke(id, before, terrain.data->snapshotLike(before), layersBefore, terrain.layers, "Pintar");

    history.undo(scene);  //desfaz a pintura
    CHECK(scene.findGameObjectById(id)->terrain->data->tilesPerSide() == 2);
    CHECK(scene.findGameObjectById(id)->terrain->data->weight(P(400), P(400), 1) == doctest::Approx(0.f));
    history.undo(scene);  //desfaz o tamanho
    CHECK(scene.findGameObjectById(id)->terrain->data->tilesPerSide() == 1);
    history.redo(scene);
    history.redo(scene);
    CHECK(scene.findGameObjectById(id)->terrain->data->tilesPerSide() == 2);
    CHECK(scene.findGameObjectById(id)->terrain->data->weight(P(400), P(400), 1) == doctest::Approx(1.f));
}

TEST_CASE("histórico: guarda no máximo MAX_STEPS passos (os mais antigos saem)") {
    CrsScene scene;
    CrsGameObject &cube = scene.createGameObject();
    const auto id = cube.getId();
    UndoHistory history;
    for (size_t i = 0; i < UndoHistory::MAX_STEPS + 10; i++) {
        history.beginSceneEdit(scene);
        scene.findGameObjectById(id)->transform.translation.x = static_cast<float>(i + 1);
        history.endSceneEdit(scene, "Mover");
    }
    CHECK(history.undoCount() == UndoHistory::MAX_STEPS);
}

TEST_CASE("terreno: mergeIntoBase passa a pintura do material pro chão base sem mudar o número de ninguém") {
    TerrainData terrain{1};
    terrain.paint({50.f, 50.f}, 6.f, 1, 1.f);
    terrain.paint({150.f, 150.f}, 6.f, 2, 1.f);
    terrain.mergeIntoBase(1);
    CHECK(terrain.weight(P(50), P(50), 0) == doctest::Approx(1.f));
    CHECK(terrain.weight(P(50), P(50), 1) == doctest::Approx(0.f));
    CHECK(terrain.weight(P(150), P(150), 2) == doctest::Approx(1.f));  //o 2 continua sendo o 2
}
