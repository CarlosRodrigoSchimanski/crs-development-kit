#include "engine/terrain/crs_terrain.hpp"
#include "engine/gameplay/character_controller_system.hpp"
#include "engine/physics/crs_physics_system.hpp"
#include "engine/scene/scene_serializer.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

using namespace crsdk;

//metros -> índice do ponto de pintura (a grade da pintura tem 4 pontos por metro)
static int P(int meters) { return meters * TERRAIN_PAINT_PER_METER; }

TEST_CASE("terreno: começa plano na altura 0, com o tamanho dos pedaços") {
    TerrainData terrain{2};
    CHECK(terrain.size() == doctest::Approx(512.f));
    CHECK(terrain.samplesPerSide() == 513);
    CHECK(terrain.sample(0, 0) == doctest::Approx(0.f));
    CHECK(terrain.heightAt(300.5f, 12.25f) == doctest::Approx(0.f));
}

TEST_CASE("terreno: a amostra da divisa é a mesma nos pedaços vizinhos") {
    TerrainData terrain{2};
    terrain.setSample(256, 10, 3.f);   //divisa entre o pedaço (0,0) e o (1,0)
    terrain.setSample(256, 256, 7.f);  //o canto dos quatro pedaços

    const int edge = TERRAIN_TILE_CELLS;
    CHECK(terrain.tile(0, 0).heights[10 * TERRAIN_TILE_SAMPLES + edge] == doctest::Approx(3.f));
    CHECK(terrain.tile(1, 0).heights[10 * TERRAIN_TILE_SAMPLES + 0] == doctest::Approx(3.f));
    CHECK(terrain.tile(0, 0).heights[edge * TERRAIN_TILE_SAMPLES + edge] == doctest::Approx(7.f));
    CHECK(terrain.tile(1, 0).heights[edge * TERRAIN_TILE_SAMPLES + 0] == doctest::Approx(7.f));
    CHECK(terrain.tile(0, 1).heights[0 * TERRAIN_TILE_SAMPLES + edge] == doctest::Approx(7.f));
    CHECK(terrain.tile(1, 1).heights[0] == doctest::Approx(7.f));
}

TEST_CASE("terreno: editar muda a versão só dos pedaços que têm a amostra") {
    TerrainData terrain{2};
    terrain.setSample(10, 10, 1.f);
    CHECK(terrain.tile(0, 0).version > 0);
    CHECK(terrain.tile(1, 0).version == 0);
    CHECK(terrain.tile(1, 1).version == 0);
}

TEST_CASE("terreno: heightAt interpola pelo triângulo do quadrado (diagonal (0,0)-(1,1))") {
    TerrainData terrain{1};
    terrain.setSample(1, 1, 2.f);

    CHECK(terrain.heightAt(0.5f, 0.5f) == doctest::Approx(1.f));     //no meio da diagonal
    CHECK(terrain.heightAt(0.75f, 0.25f) == doctest::Approx(0.5f));  //triângulo de baixo
    CHECK(terrain.heightAt(0.25f, 0.75f) == doctest::Approx(0.5f));  //triângulo de cima
    CHECK(terrain.heightAt(1.f, 1.f) == doctest::Approx(2.f));
}

TEST_CASE("terreno: subir é mais forte no meio, some na borda e não passa do raio") {
    TerrainData terrain{1};
    terrain.raise({100.f, 100.f}, 10.f, 2.f);

    CHECK(terrain.sample(100, 100) == doctest::Approx(2.f));
    CHECK(terrain.sample(105, 100) > 0.5f);
    CHECK(terrain.sample(105, 100) < 2.f);
    CHECK(terrain.sample(105, 100) == doctest::Approx(terrain.sample(95, 100)));  //simétrico
    CHECK(terrain.sample(110, 100) == doctest::Approx(0.f));
    CHECK(terrain.sample(100, 115) == doctest::Approx(0.f));

    terrain.raise({100.f, 100.f}, 10.f, -2.f);  //abaixar desfaz
    CHECK(terrain.sample(100, 100) == doctest::Approx(0.f));
}

TEST_CASE("terreno: suavizar baixa um pico e nivelar leva pra altura pedida") {
    TerrainData terrain{1};
    terrain.setSample(50, 50, 9.f);
    terrain.smooth({50.f, 50.f}, 3.f, 1.f);
    CHECK(terrain.sample(50, 50) == doctest::Approx(1.f));  //média dos 9: 9 / 9
    CHECK(terrain.sample(51, 50) > 0.f);                     //o vizinho subiu um pouco

    TerrainData flat{1};
    flat.raise({50.f, 50.f}, 8.f, 4.f);
    flat.flatten({50.f, 50.f}, 8.f, 1.f, 1.f);
    CHECK(flat.sample(50, 50) == doctest::Approx(1.f));
}

TEST_CASE("terreno: pincel na divisa escreve nos dois pedaços (sem degrau)") {
    TerrainData terrain{2};
    terrain.raise({256.f, 100.f}, 6.f, 3.f);
    CHECK(terrain.tile(0, 0).heights[100 * TERRAIN_TILE_SAMPLES + TERRAIN_TILE_CELLS] == doctest::Approx(3.f));
    CHECK(terrain.tile(1, 0).heights[100 * TERRAIN_TILE_SAMPLES] == doctest::Approx(3.f));
    CHECK(terrain.heightAt(255.5f, 100.f) == doctest::Approx(terrain.heightAt(256.5f, 100.f)).epsilon(0.01));
}

TEST_CASE("terreno: normal aponta pra cima no plano e inclina na rampa") {
    TerrainData terrain{1};
    CHECK(terrain.normalAt(20, 20).y == doctest::Approx(1.f));
    //rampa subindo em +x (1 m de altura por metro): a normal olha pra -x
    for (int z = 0; z < 50; z++) {
        for (int x = 0; x < 50; x++) terrain.setSample(x, z, static_cast<float>(x));
    }
    glm::vec3 n = terrain.normalAt(20, 20);
    CHECK(n.x < -0.6f);
    CHECK(n.y == doctest::Approx(std::sqrt(0.5f)).epsilon(0.01));
}

TEST_CASE("terreno: raio de cima acerta o chão; de baixo ou fora não acerta") {
    TerrainData terrain{1};
    terrain.raise({128.f, 128.f}, 20.f, 5.f);

    auto down = terrain.raycast({128.f, 50.f, 128.f}, {0.f, -1.f, 0.f}, 100.f);
    REQUIRE(down.has_value());
    CHECK(*down == doctest::Approx(45.f).epsilon(0.001));

    //em diagonal: o ponto acertado está na superfície
    auto slanted = terrain.raycast({60.f, 30.f, 60.f}, {1.f, -0.5f, 1.f}, 500.f);
    REQUIRE(slanted.has_value());
    glm::vec3 hit = glm::vec3{60.f, 30.f, 60.f} + glm::normalize(glm::vec3{1.f, -0.5f, 1.f}) * *slanted;
    CHECK(hit.y == doctest::Approx(terrain.heightAt(hit.x, hit.z)).epsilon(0.01));

    CHECK_FALSE(terrain.raycast({128.f, -10.f, 128.f}, {0.f, -1.f, 0.f}, 100.f).has_value());  //começou embaixo
    CHECK_FALSE(terrain.raycast({128.f, 50.f, 128.f}, {0.f, 1.f, 0.f}, 100.f).has_value());    //pra cima
    CHECK_FALSE(terrain.raycast({-50.f, 10.f, 128.f}, {-1.f, -0.1f, 0.f}, 100.f).has_value()); //fora do terreno
}

TEST_CASE("terreno: salvar e carregar devolve as mesmas alturas (um arquivo por pedaço)") {
    const auto folder = std::filesystem::temp_directory_path() / "crs_test_terreno";
    std::filesystem::remove_all(folder);
    TerrainData terrain{2};
    terrain.raise({256.f, 256.f}, 30.f, 12.f);
    terrain.setSample(3, 500, -4.f);
    terrain.save(folder.string());

    CHECK(std::filesystem::exists(folder / "tile_1_1.bin"));
    TerrainData loaded = TerrainData::load(folder.string(), 2);
    CHECK(loaded.sample(256, 256) == doctest::Approx(12.f));
    CHECK(loaded.sample(3, 500) == doctest::Approx(-4.f));
    CHECK(loaded.sample(270, 250) == doctest::Approx(terrain.sample(270, 250)));

    //pedaço sem arquivo fica plano; arquivo que não é de terreno é erro
    std::filesystem::remove(folder / "tile_0_0.bin");
    CHECK(TerrainData::load(folder.string(), 2).sample(10, 10) == doctest::Approx(0.f));
    { std::ofstream bad{folder / "tile_1_0.bin", std::ios::binary}; bad << "lixo"; }
    CHECK_THROWS(TerrainData::load(folder.string(), 2));
    std::filesystem::remove_all(folder);
}

TEST_CASE("cena: raycast e terrainHeightAt acham o terreno onde o objeto dele está") {
    CrsScene scene;
    CrsGameObject &obj = scene.createGameObject();
    obj.transform.translation = {100.f, 5.f, 0.f};  //o objeto fica no meio: o terreno vai de -28 a 228 em x
    obj.terrain.emplace();
    obj.terrain->data->raise({128.f, 128.f}, 10.f, 3.f);

    auto height = scene.terrainHeightAt(100.f, 0.f);
    REQUIRE(height.has_value());
    CHECK(*height == doctest::Approx(8.f));  //5 do objeto + 3 do morro
    CHECK(scene.terrainHeightAt(300.f, 0.f) == std::nullopt);

    auto hit = scene.raycast(Ray{{100.f, 50.f, 0.f}, {0.f, -1.f, 0.f}}, 100.f);
    REQUIRE(hit.has_value());
    CHECK(hit->id == obj.getId());
    CHECK(hit->distance == doctest::Approx(42.f).epsilon(0.001));
}

TEST_CASE("física: personagem fica em pé em cima do morro do terreno") {
    CrsScene scene;
    CrsGameObject &ground = scene.createGameObject();
    ground.terrain.emplace();
    ground.terrain->data->raise({128.f, 128.f}, 40.f, 6.f);

    CrsGameObject &player = scene.createGameObject();
    player.transform.translation = {0.f, 15.f, 0.f};  //em cima do meio do terreno (o objeto do terreno está em 0)
    player.rigidBody.emplace();
    player.rigidBody->shape = ColliderShape::Capsule;
    player.rigidBody->radius = 0.4f;
    player.rigidBody->height = 1.8f;
    player.rigidBody->center = {0.f, 0.9f, 0.f};
    player.character.emplace();

    PhysicsSystem physics;
    for (int i = 0; i < 180; i++) physics.update(scene, 1.f / 60.f);

    CHECK(player.rigidBody->onGround);
    CHECK(player.transform.translation.y == doctest::Approx(6.f).epsilon(0.02));
}

TEST_CASE("física: esfera cai e rola morro abaixo") {
    CrsScene scene;
    CrsGameObject &ground = scene.createGameObject();
    ground.terrain.emplace();
    ground.terrain->data->raise({128.f, 128.f}, 30.f, 8.f);

    CrsGameObject &ball = scene.createGameObject();
    ball.transform.translation = {12.f, 15.f, 0.f};  //na encosta (uns 22 graus; perto do topo é quase plano)
    ball.rigidBody.emplace();
    ball.rigidBody->shape = ColliderShape::Sphere;
    ball.rigidBody->radius = 0.5f;

    PhysicsSystem physics;
    for (int i = 0; i < 240; i++) physics.update(scene, 1.f / 60.f);

    CHECK(ball.transform.translation.x > 16.f);  //desceu pro lado de fora
    CHECK(ball.transform.translation.y < 4.f);
}

TEST_CASE("cena: o terreno é salvo ao lado da cena (assets/terrains/<cena>_<id>) e volta igual") {
    const auto root = std::filesystem::temp_directory_path() / "crs_test_projeto";
    std::filesystem::remove_all(root);
    const auto scenePath = root / "scenes" / "mundo.json";

    CrsScene scene;
    CrsGameObject &obj = scene.createGameObject();
    obj.name = "Terreno";
    obj.terrain.emplace();
    obj.terrain->data = std::make_shared<TerrainData>(2);
    obj.terrain->data->raise({300.f, 200.f}, 15.f, 4.f);
    saveScene(scene, scenePath.string());

    CHECK(std::filesystem::exists(root / "terrains" / ("mundo_" + std::to_string(obj.getId())) / "tile_1_0.bin"));
    CrsScene loaded = loadScene(scenePath.string(), [](const std::string &) { return nullptr; });
    const CrsGameObject &back = *loaded.findGameObjectById(obj.getId());
    REQUIRE(back.terrain.has_value());
    CHECK(back.terrain->data->tilesPerSide() == 2);
    CHECK(back.terrain->data->sample(300, 200) == doctest::Approx(4.f));
    std::filesystem::remove_all(root);
}

TEST_CASE("terreno: aumentar mantém o que foi esculpido no lugar; os pedaços novos nascem planos") {
    TerrainData terrain{1};
    terrain.raise({100.f, 100.f}, 10.f, 5.f);
    terrain.setSample(256, 40, 2.f);  //na borda que vai virar divisa

    TerrainData bigger = terrain.resized(3);
    CHECK(bigger.tilesPerSide() == 3);
    CHECK(bigger.size() == doctest::Approx(768.f));
    CHECK(bigger.sample(100, 100) == doctest::Approx(5.f));
    CHECK(bigger.sample(500, 500) == doctest::Approx(0.f));
    //a divisa com o pedaço novo tem a altura do antigo nos dois lados
    CHECK(bigger.tile(0, 0).heights[40 * TERRAIN_TILE_SAMPLES + TERRAIN_TILE_CELLS] == doctest::Approx(2.f));
    CHECK(bigger.tile(1, 0).heights[40 * TERRAIN_TILE_SAMPLES] == doctest::Approx(2.f));
}

TEST_CASE("terreno: diminuir tira os pedaços de +X e +Z e para no mínimo e no máximo") {
    TerrainData terrain{3};
    terrain.raise({100.f, 100.f}, 10.f, 5.f);
    terrain.raise({600.f, 600.f}, 10.f, 5.f);

    TerrainData smaller = terrain.resized(1);
    CHECK(smaller.size() == doctest::Approx(256.f));
    CHECK(smaller.sample(100, 100) == doctest::Approx(5.f));

    CHECK(terrain.resized(0).tilesPerSide() == 1);
    CHECK(terrain.resized(100).tilesPerSide() == TERRAIN_MAX_TILES);
}

TEST_CASE("terreno: salvar depois de diminuir apaga os arquivos dos pedaços que saíram") {
    const auto folder = std::filesystem::temp_directory_path() / "crs_test_terreno_menor";
    std::filesystem::remove_all(folder);
    TerrainData{2}.save(folder.string());
    CHECK(std::filesystem::exists(folder / "tile_1_1.bin"));

    TerrainData{1}.save(folder.string());
    CHECK(std::filesystem::exists(folder / "tile_0_0.bin"));
    CHECK_FALSE(std::filesystem::exists(folder / "tile_1_1.bin"));
    std::filesystem::remove_all(folder);
}

//as 8 camadas de uma amostra somam 1
static float weightSum(const TerrainData &terrain, int x, int z) {
    float sum = 0.f;
    for (float w : terrain.mix(P(x), P(z)).weights) sum += w;
    return sum;
}

TEST_CASE("pintura: começa tudo no chão base e pintar põe a camada no meio do pincel") {
    TerrainData terrain{1};
    CHECK(terrain.weight(P(50), P(50), 0) == doctest::Approx(1.f));

    terrain.paint({50.f, 50.f}, 6.f, 2, 1.f);

    CHECK(terrain.weight(P(50), P(50), 2) == doctest::Approx(1.f));
    CHECK(terrain.weight(P(50), P(50), 0) == doctest::Approx(0.f));
    CHECK(terrain.weight(P(53), P(50), 2) > 0.f);
    CHECK(terrain.weight(P(53), P(50), 2) < 1.f);
    CHECK(weightSum(terrain, 53, 50) == doctest::Approx(1.f));
    CHECK(terrain.weight(P(60), P(50), 2) == doctest::Approx(0.f));  //fora do pincel
}

TEST_CASE("pintura: pintar outra camada por cima divide o espaço, e apagar devolve pras outras") {
    TerrainData terrain{1};
    terrain.paint({50.f, 50.f}, 6.f, 1, 0.5f);  //metade camada 1, metade chão
    terrain.paint({50.f, 50.f}, 6.f, 2, 0.5f);  //a 2 ganha metade; as outras encolhem juntas

    CHECK(terrain.weight(P(50), P(50), 2) == doctest::Approx(0.5f).epsilon(0.01));
    CHECK(terrain.weight(P(50), P(50), 1) == doctest::Approx(0.25f).epsilon(0.01));
    CHECK(terrain.weight(P(50), P(50), 0) == doctest::Approx(0.25f).epsilon(0.01));

    terrain.paint({50.f, 50.f}, 6.f, 2, -1.f);  //Shift: apaga a 2
    CHECK(terrain.weight(P(50), P(50), 2) == doctest::Approx(0.f));
    CHECK(terrain.weight(P(50), P(50), 1) == doctest::Approx(0.5f).epsilon(0.01));
    CHECK(terrain.weight(P(50), P(50), 0) == doctest::Approx(0.5f).epsilon(0.01));
}

TEST_CASE("pintura: apagar onde só tinha a camada volta pro chão base") {
    TerrainData terrain{1};
    terrain.paint({50.f, 50.f}, 6.f, 3, 1.f);
    terrain.paint({50.f, 50.f}, 6.f, 3, -1.f);
    CHECK(terrain.weight(P(50), P(50), 0) == doctest::Approx(1.f));
    CHECK(weightSum(terrain, 50, 50) == doctest::Approx(1.f));
}

TEST_CASE("pintura: na divisa grava nos dois pedaços e muda a versão deles") {
    TerrainData terrain{2};
    const uint64_t before = terrain.tile(1, 0).paintVersion;
    const uint64_t heightsBefore = terrain.tile(1, 0).version;
    terrain.paint({256.f, 40.f}, 4.f, 1, 1.f);
    const size_t row = static_cast<size_t>(P(40) * TERRAIN_PAINT_SAMPLES);
    const LayerMix left = unpackTexel(terrain.tile(0, 0).paint[row + TERRAIN_PAINT_CELLS]);  //última coluna do pedaço da esquerda
    const LayerMix right = unpackTexel(terrain.tile(1, 0).paint[row]);                     //primeira do da direita
    //quanto do material 1 a amostra tem (ele entra na primeira posição livre, não necessariamente a 0)
    auto weightOf = [](const LayerMix &m, int layer) {
        for (size_t i = 0; i < m.ids.size(); i++) {
            if (m.ids[i] == layer && m.weights[i] > 0.f) return m.weights[i];
        }
        return 0.f;
    };
    CHECK(weightOf(left, 1) == doctest::Approx(1.f));
    CHECK(weightOf(right, 1) == doctest::Approx(1.f));
    CHECK(terrain.tile(1, 0).paintVersion > before);
    CHECK(terrain.tile(1, 0).version == heightsBefore);  //pintar não refaz a malha nem a física
}

TEST_CASE("pintura: tirar uma camada devolve o que ela tinha pro chão base e desce as de cima") {
    TerrainData terrain{1};
    terrain.paint({50.f, 50.f}, 6.f, 1, 1.f);
    terrain.paint({100.f, 100.f}, 6.f, 2, 1.f);

    terrain.removeLayer(1);

    CHECK(terrain.weight(P(50), P(50), 0) == doctest::Approx(1.f));    //era da 1: voltou pro chão
    CHECK(terrain.weight(P(100), P(100), 1) == doctest::Approx(1.f));  //a 2 virou a 1
    CHECK(terrain.weight(P(100), P(100), 2) == doctest::Approx(0.f));
    terrain.removeLayer(0);  //o chão base não sai
    CHECK(terrain.weight(P(50), P(50), 0) == doctest::Approx(1.f));
}

TEST_CASE("pintura: salvar e carregar devolve as camadas (com a precisão de 1 byte)") {
    const auto folder = std::filesystem::temp_directory_path() / "crs_test_pintura";
    std::filesystem::remove_all(folder);
    TerrainData terrain{2};
    terrain.paint({300.f, 200.f}, 8.f, 4, 1.f);
    terrain.paint({300.f, 200.f}, 8.f, 1, 0.3f);
    terrain.save(folder.string());

    CHECK(std::filesystem::exists(folder / "paint_1_0.bin"));
    TerrainData loaded = TerrainData::load(folder.string(), 2);
    for (int layer = 0; layer < 8; layer++) {
        CHECK(loaded.weight(P(300), P(200), layer) == doctest::Approx(terrain.weight(P(300), P(200), layer)).epsilon(0.01));
        CHECK(loaded.weight(P(304), P(203), layer) == doctest::Approx(terrain.weight(P(304), P(203), layer)).epsilon(0.01));
    }
    //terreno salvo antes das camadas (sem paint_*.bin): tudo no chão base
    std::filesystem::remove(folder / "paint_1_0.bin");
    CHECK(TerrainData::load(folder.string(), 2).weight(P(300), P(200), 0) == doctest::Approx(1.f));
    std::filesystem::remove_all(folder);
}

TEST_CASE("pintura: aumentar o terreno mantém o que foi pintado") {
    TerrainData terrain{1};
    terrain.paint({100.f, 100.f}, 6.f, 2, 1.f);
    TerrainData bigger = terrain.resized(2);
    CHECK(bigger.weight(P(100), P(100), 2) == doctest::Approx(1.f));
    CHECK(bigger.weight(P(400), P(400), 0) == doctest::Approx(1.f));
}

TEST_CASE("cena: as camadas voltam na ordem, e a cena antiga (um material só) vira o chão base") {
    const auto root = std::filesystem::temp_directory_path() / "crs_test_camadas";
    std::filesystem::remove_all(root);
    const auto scenePath = root / "scenes" / "mundo.json";
    CrsScene scene;
    CrsGameObject &obj = scene.createGameObject();
    obj.terrain.emplace();
    obj.terrain->layers = {{"assets/materials/a", 4.f}, {"assets/materials/b", 2.5f}};
    obj.terrain->blend = 0.8f;
    saveScene(scene, scenePath.string());

    CrsScene loaded = loadScene(scenePath.string(), [](const std::string &) { return nullptr; });
    const auto &layers = loaded.findGameObjectById(obj.getId())->terrain->layers;
    REQUIRE(layers.size() == 2);
    CHECK(layers[1].material == "assets/materials/b");
    CHECK(layers[1].textureSize == doctest::Approx(2.5f));
    CHECK(loaded.findGameObjectById(obj.getId())->terrain->blend == doctest::Approx(0.8f));  //a Mistura

    {
        std::ofstream out{scenePath};
        out << R"({"version": 2, "objects": [{"id": 0, "transform": {"translation": [0,0,0], "rotation": [0,0,0], "scale": [1,1,1]},
                   "terrain": {"tiles": 1, "folder": "../terrains/x", "material": "assets/materials/velho", "textureSize": 3.0}}]})";
    }
    CrsScene old = loadScene(scenePath.string(), [](const std::string &) { return nullptr; });
    const auto &oldLayers = old.findGameObjectById(0)->terrain->layers;
    REQUIRE(oldLayers.size() == 1);
    CHECK(oldLayers[0].material == "assets/materials/velho");
    CHECK(oldLayers[0].textureSize == doctest::Approx(3.f));
    CHECK(old.findGameObjectById(0)->terrain->blend == doctest::Approx(0.5f));  //cena sem Mistura: o padrão
    std::filesystem::remove_all(root);
}

TEST_CASE("pintura: a engine não tira material ao pintar; o trecho com mais de 4 é só avisado") {
    TerrainData terrain{1};
    //5 materiais (o chão e mais 4) no mesmo ponto, no trecho (1, 1)
    for (int layer = 1; layer <= 4; layer++) terrain.paint({48.f, 48.f}, 6.f, layer, 0.3f);

    for (int layer = 1; layer <= 4; layer++) CHECK(terrain.weight(P(48), P(48), layer) > 0.f);  //ninguém saiu
    CHECK(weightSum(terrain, 48, 48) == doctest::Approx(1.f));
    CHECK(terrain.chunkLayerCount(0, 0, 1, 1) == 5);  //5 > 4: aura no editor
    CHECK(terrain.chunkLayerCount(0, 0, 7, 7) == 1);  //trecho intocado: só o chão
    //no Play só os 4 que mais cobrem; no editor até 8 (dá pra ver o que passou)
    CHECK(terrain.chunkLayers(0, 0, 1, 1, TERRAIN_CHUNK_LIMIT).size() == 4);
    CHECK(terrain.chunkLayers(0, 0, 1, 1).size() == 5);
}

TEST_CASE("pintura: o limite é por trecho, não por ponto (materiais em pontos diferentes somam)") {
    TerrainData terrain{1};
    //4 manchas separadas dentro do mesmo trecho de 32 m (o (0, 0)): cada ponto tem só 2 materiais
    terrain.paint({6.f, 6.f}, 3.f, 1, 1.f);
    terrain.paint({24.f, 6.f}, 3.f, 2, 1.f);
    terrain.paint({6.f, 24.f}, 3.f, 3, 1.f);
    terrain.paint({24.f, 24.f}, 3.f, 4, 1.f);
    CHECK(terrain.chunkLayerCount(0, 0, 0, 0) == 5);
}

TEST_CASE("pintura: um ponto guarda até 8; pintar o 9º tira o mais fraco") {
    TerrainData terrain{1};
    for (int layer = 1; layer <= 7; layer++) terrain.paint({50.f, 50.f}, 6.f, layer, 0.15f);
    LayerMix before = terrain.mix(P(50), P(50));
    size_t weakest = 0;
    for (size_t i = 1; i < before.weights.size(); i++) {
        if (before.weights[i] < before.weights[weakest]) weakest = i;
    }
    const int weakestLayer = before.ids[weakest];

    terrain.paint({50.f, 50.f}, 6.f, 8, 0.15f);

    CHECK(terrain.weight(P(50), P(50), 8) == doctest::Approx(0.15f).epsilon(0.01));
    CHECK(terrain.weight(P(50), P(50), weakestLayer) == doctest::Approx(0.f));
    CHECK(weightSum(terrain, 50, 50) == doctest::Approx(1.f));
}

TEST_CASE("pintura: o pincel Checar diz quanto cada material cobre dentro do círculo") {
    TerrainData terrain{1};
    //duas pinceladas: uma só cobre 100% bem no centro (a 2 m ainda sobra ~9% de chão)
    terrain.paint({100.f, 100.f}, 10.f, 3, 1.f);
    terrain.paint({100.f, 100.f}, 10.f, 3, 1.f);

    //círculo pequeno no meio da mancha: só o 3
    auto center = terrain.brushLayers({100.f, 100.f}, 2.f);
    REQUIRE(center.size() == 1);
    CHECK(center[0].layer == 3);
    CHECK(center[0].share == doctest::Approx(1.f));

    //círculo grande: o chão aparece em volta, do que mais cobre pro que menos, somando 100%
    auto wide = terrain.brushLayers({100.f, 100.f}, 30.f);
    REQUIRE(wide.size() == 2);
    CHECK(wide[0].layer == 0);
    CHECK(wide[1].layer == 3);
    CHECK(wide[0].share + wide[1].share == doctest::Approx(1.f));
}

TEST_CASE("terreno: limpar a paleta esvazia só os materiais que não aparecem em lugar nenhum") {
    CrsScene scene;
    CrsGameObject &obj = scene.createGameObject();
    obj.terrain.emplace();
    obj.terrain->layers = {{"chao"}, {"lama"}, {"pedra"}, {"rocha"}};
    obj.terrain->data->paint({50.f, 50.f}, 6.f, 1, 1.f);    //lama
    obj.terrain->data->paint({150.f, 150.f}, 6.f, 3, 1.f);  //rocha
    obj.terrain->data->paint({50.f, 50.f}, 6.f, 2, 1.f);    //pedra por cima da lama
    obj.terrain->data->paint({50.f, 50.f}, 12.f, 0, 1.f);   //e o chão por cima de tudo ali
    obj.terrain->data->paint({50.f, 50.f}, 12.f, 0, 1.f);

    obj.terrain->compactPalette();

    //ninguém muda de número (o Ctrl+Z continua valendo): os sem uso ficam com o nome vazio
    REQUIRE(obj.terrain->layers.size() == 4);
    CHECK(obj.terrain->layers[0].material == "chao");
    CHECK(obj.terrain->layers[1].material.empty());
    CHECK(obj.terrain->layers[2].material.empty());
    CHECK(obj.terrain->layers[3].material == "rocha");
    CHECK(obj.terrain->data->weight(P(150), P(150), 3) == doctest::Approx(1.f));  //a rocha continua sendo a 3

    //sem uso no fim da lista: sai de verdade
    obj.terrain->data->paint({150.f, 150.f}, 12.f, 0, 1.f);
    obj.terrain->data->paint({150.f, 150.f}, 12.f, 0, 1.f);
    obj.terrain->compactPalette();
    CHECK(obj.terrain->layers.size() == 1);
}

TEST_CASE("pintura: arquivo do formato de 4 pares (salvo pelo commit anterior) é lido igual") {
    const auto folder = std::filesystem::temp_directory_path() / "crs_test_pintura_4pares";
    std::filesystem::remove_all(folder);
    TerrainData{1}.save(folder.string());
    {
        //cabeçalho + 8 bytes por amostra: 4 índices e 4 pesos; todas: 70% no 2, 30% no 5
        std::ofstream out{folder / "paint_0_0.bin", std::ios::binary};
        const uint32_t samples = TERRAIN_TILE_SAMPLES, format = 4;
        out.write("CRST", 4);
        out.write(reinterpret_cast<const char *>(&samples), 4);
        out.write(reinterpret_cast<const char *>(&format), 4);
        const unsigned char sample[8] = {2, 5, 0, 0, 179, 77, 0, 0};
        for (int i = 0; i < TERRAIN_TILE_SAMPLES * TERRAIN_TILE_SAMPLES; i++) out.write(reinterpret_cast<const char *>(sample), 8);
    }
    TerrainData loaded = TerrainData::load(folder.string(), 1);
    CHECK(loaded.weight(P(10), P(10), 2) == doctest::Approx(0.7f).epsilon(0.02));
    CHECK(loaded.weight(P(10), P(10), 5) == doctest::Approx(0.3f).epsilon(0.02));
    CHECK(loaded.weight(P(10), P(10), 0) == doctest::Approx(0.f));
    std::filesystem::remove_all(folder);
}

TEST_CASE("pintura: resto de pincelada fraca (abaixo de 2%) é zerado e volta pros outros") {
    TerrainData terrain{1};
    terrain.paint({50.f, 50.f}, 6.f, 1, 0.5f);
    terrain.paint({50.f, 50.f}, 6.f, 2, 0.99f);  //a 1 e o chão ficam com 0.5% cada: viram 0
    CHECK(terrain.weight(P(50), P(50), 2) == doctest::Approx(1.f));
    CHECK(terrain.weight(P(50), P(50), 1) == doctest::Approx(0.f));
    CHECK(terrain.weight(P(50), P(50), 0) == doctest::Approx(0.f));
}

TEST_CASE("pintura: o quadradinho de 32 m só pede os materiais que aparecem nele") {
    TerrainData terrain{1};
    terrain.paint({16.f, 16.f}, 6.f, 5, 1.f);    //no quadradinho (0, 0)
    terrain.paint({100.f, 100.f}, 6.f, 7, 1.f);  //no quadradinho (3, 3)

    auto first = terrain.chunkLayers(0, 0, 0, 0);
    REQUIRE(first.size() == 2);
    CHECK(first[0] == 0);  //o chão cobre mais que o 5 (do que mais cobre pro que menos)
    CHECK(first[1] == 5);
    auto other = terrain.chunkLayers(0, 0, 3, 3);
    CHECK(std::find(other.begin(), other.end(), 7) != other.end());
    CHECK(std::find(other.begin(), other.end(), 5) == other.end());  //o 5 não aparece aqui: não é pedido
    auto empty = terrain.chunkLayers(0, 0, 7, 7);
    REQUIRE(empty.size() == 1);
    CHECK(empty[0] == 0);
}

TEST_CASE("pintura: pintar por cima até o material sumir tira ele do quadradinho e do uso") {
    TerrainData terrain{1};
    terrain.paint({16.f, 16.f}, 8.f, 1, 1.f);
    terrain.paint({16.f, 16.f}, 30.f, 2, 1.f);  //cobre tudo com o 2, num raio bem maior
    terrain.paint({16.f, 16.f}, 30.f, 2, 1.f);

    auto layers = terrain.chunkLayers(0, 0, 0, 0);
    CHECK(std::find(layers.begin(), layers.end(), 1) == layers.end());
    std::vector<float> usage = terrain.layerUsage(3);
    CHECK(usage[1] == doctest::Approx(0.f));
    CHECK(usage[2] > 0.f);
    CHECK(usage[0] + usage[1] + usage[2] == doctest::Approx(1.f));
}

TEST_CASE("pintura: arquivo do formato antigo (8 camadas fixas) é lido") {
    const auto folder = std::filesystem::temp_directory_path() / "crs_test_pintura_antiga";
    std::filesystem::remove_all(folder);
    TerrainData{1}.save(folder.string());
    {
        //cabeçalho + 8 bytes por amostra; todas as amostras: 10% no 0, 50% no 3, 40% no 6
        std::ofstream out{folder / "paint_0_0.bin", std::ios::binary};
        const uint32_t samples = TERRAIN_TILE_SAMPLES, format = 8;
        out.write("CRST", 4);
        out.write(reinterpret_cast<const char *>(&samples), 4);
        out.write(reinterpret_cast<const char *>(&format), 4);
        const unsigned char sample[8] = {26, 0, 0, 128, 0, 0, 102, 0};
        for (int i = 0; i < TERRAIN_TILE_SAMPLES * TERRAIN_TILE_SAMPLES; i++) out.write(reinterpret_cast<const char *>(sample), 8);
    }
    TerrainData loaded = TerrainData::load(folder.string(), 1);
    CHECK(loaded.weight(P(10), P(10), 3) == doctest::Approx(0.5f).epsilon(0.02));
    CHECK(loaded.weight(P(10), P(10), 6) == doctest::Approx(0.4f).epsilon(0.02));
    CHECK(loaded.weight(P(10), P(10), 0) == doctest::Approx(0.1f).epsilon(0.05));
    std::filesystem::remove_all(folder);
}

TEST_CASE("pintura: espalhar 0 é o pincel suave de sempre") {
    TerrainData smooth{1}, sprayZero{1};
    smooth.paint({100.f, 100.f}, 10.f, 1, 0.5f);
    sprayZero.paint({100.f, 100.f}, 10.f, 1, 0.5f, 0.f, 7);
    for (int x = 90; x <= 110; x++) CHECK(sprayZero.weight(P(x), P(100), 1) == doctest::Approx(smooth.weight(P(x), P(100), 1)));
}

TEST_CASE("pintura: espalhar 1 pinta cheio no meio e salpica a borda") {
    TerrainData terrain{1};
    terrain.paint({100.f, 100.f}, 20.f, 1, 1.f, 1.f, 3);

    CHECK(terrain.weight(P(100), P(100), 1) == doctest::Approx(1.f));  //no meio: cheio
    //no anel perto da borda (14 a 18 m do centro): uns pontos pintados com força cheia, outros intactos
    int painted = 0, untouched = 0;
    for (int z = 80; z <= 120; z++) {
        for (int x = 80; x <= 120; x++) {
            const float d = glm::length(glm::vec2{static_cast<float>(x) - 100.f, static_cast<float>(z) - 100.f});
            if (d < 14.f || d > 18.f) continue;
            const float w = terrain.weight(P(x), P(z), 1);
            if (w > 0.99f) painted++;
            else if (w < 0.01f) untouched++;
        }
    }
    CHECK(painted > 0);
    CHECK(untouched > painted);  //perto da borda a maioria fica de fora
}

TEST_CASE("pintura: o salpicado depende do sorteio da pincelada (mesmo sorteio, mesmos pontos)") {
    TerrainData a{1}, b{1}, c{1};
    a.paint({100.f, 100.f}, 20.f, 1, 1.f, 1.f, 5);
    b.paint({100.f, 100.f}, 20.f, 1, 1.f, 1.f, 5);
    c.paint({100.f, 100.f}, 20.f, 1, 1.f, 1.f, 6);
    int sameAB = 0, diffAC = 0;
    for (int x = 84; x <= 116; x++) {
        if (a.weight(P(x), P(100), 1) == b.weight(P(x), P(100), 1)) sameAB++;
        if (a.weight(P(x), P(100), 1) != c.weight(P(x), P(100), 1)) diffAC++;
    }
    CHECK(sameAB == 33);
    CHECK(diffAC > 0);
}

TEST_CASE("pintura: 4 pontos por metro, e só os trechos tocados mudam de versão (a GPU reenvia só eles)") {
    TerrainData terrain{1};
    terrain.paint({16.25f, 16.f}, 0.2f, 1, 1.f);  //pincel menor que a distância entre os pontos: só o do meio
    CHECK(terrain.weight(65, P(16), 1) == doctest::Approx(1.f));  //16,25 m = ponto 65
    CHECK(terrain.weight(P(16), P(16), 1) == doctest::Approx(0.f));  //25 cm do lado: intocado
    CHECK(terrain.weight(66, P(16), 1) == doctest::Approx(0.f));

    const TerrainTile &tile = terrain.tile(0, 0);
    CHECK(tile.chunkPaintVersions[0] > 0);  //o trecho (0, 0)
    CHECK(tile.chunkPaintVersions[1] == 0);
    CHECK(tile.chunkPaintVersions[TERRAIN_CHUNKS_PER_TILE] == 0);
    terrain.paint({32.f, 32.f}, 0.1f, 2, 1.f);  //o canto entre 4 trechos: os 4
    CHECK(tile.chunkPaintVersions[1] > 0);
    CHECK(tile.chunkPaintVersions[TERRAIN_CHUNKS_PER_TILE] > 0);
    CHECK(tile.chunkPaintVersions[TERRAIN_CHUNKS_PER_TILE + 1] > 0);
    CHECK(tile.chunkPaintVersions[2] == 0);
}

TEST_CASE("pintura: o arquivo antigo (1 ponto por metro) vira 4 por metro misturando os vizinhos, sem degrau") {
    const auto folder = std::filesystem::temp_directory_path() / "crs_test_pintura_convertida";
    std::filesystem::remove_all(folder);
    TerrainData{1}.save(folder.string());
    {
        //tudo no chão base, menos a amostra (10, 10): toda no material 3
        std::ofstream out{folder / "paint_0_0.bin", std::ios::binary};
        const uint32_t samples = TERRAIN_TILE_SAMPLES, format = 4;
        out.write("CRST", 4);
        out.write(reinterpret_cast<const char *>(&samples), 4);
        out.write(reinterpret_cast<const char *>(&format), 4);
        for (int z = 0; z < TERRAIN_TILE_SAMPLES; z++) {
            for (int x = 0; x < TERRAIN_TILE_SAMPLES; x++) {
                const unsigned char base[8] = {0, 0, 0, 0, 255, 0, 0, 0};
                const unsigned char rock[8] = {3, 0, 0, 0, 255, 0, 0, 0};
                out.write(reinterpret_cast<const char *>(x == 10 && z == 10 ? rock : base), 8);
            }
        }
    }
    TerrainData loaded = TerrainData::load(folder.string(), 1);
    CHECK(loaded.weight(P(10), P(10), 3) == doctest::Approx(1.f));                       //em cima da amostra antiga
    CHECK(loaded.weight(P(10) + 2, P(10), 3) == doctest::Approx(0.5f).epsilon(0.01));    //meio metro: metade
    CHECK(loaded.weight(P(10) + 2, P(10), 0) == doctest::Approx(0.5f).epsilon(0.01));
    CHECK(loaded.weight(P(10) - 1, P(10), 3) == doctest::Approx(0.75f).epsilon(0.01));   //25 cm antes: 3/4
    CHECK(loaded.weight(P(11), P(10), 3) == doctest::Approx(0.f));                       //na amostra vizinha: nada
    std::filesystem::remove_all(folder);
}

TEST_CASE("pintura: segurar o pincel a 144 fps acumula até na borda (passos pequenos não se perdem)") {
    TerrainData terrain{1};
    const float perFrame = 0.5f * 5.f / 144.f;  //força 0,5 a 144 fps
    terrain.beginJournal();
    for (int frame = 0; frame < 144 * 3; frame++) terrain.paint({100.f, 100.f}, 10.f, 1, perFrame, 0.6f, 1);
    terrain.endJournal();
    CHECK(terrain.weight(P(100), P(100), 1) == doctest::Approx(1.f));
    int holes = 0;
    for (int pz = P(94); pz <= P(106); pz++) {
        for (int px = P(94); px <= P(106); px++) {
            const float dx = static_cast<float>(px - P(100)) / TERRAIN_PAINT_PER_METER;
            const float dz = static_cast<float>(pz - P(100)) / TERRAIN_PAINT_PER_METER;
            if (dx * dx + dz * dz > 36.f) continue;  //só o círculo de 6 m
            if (terrain.weight(px, pz, 1) < 0.5f) holes++;
        }
    }
    CHECK(holes == 0);  //até 6 m do meio (raio 10), 3 s segurando: tudo pintado

    //fora do journal cada chamada vale sozinha: o mesmo passo pequeno some (é o que o editor evita)
    TerrainData single{1};
    for (int frame = 0; frame < 10; frame++) single.paint({100.f, 100.f}, 10.f, 1, perFrame);
    CHECK(single.weight(P(100), P(100), 1) == doctest::Approx(0.f));
}

TEST_CASE("física: personagem parado na ladeira não escorrega (e o visual não fica girando)") {
    CrsScene scene;
    CrsGameObject &ground = scene.createGameObject();
    ground.terrain.emplace();
    ground.terrain->data->raise({128.f, 128.f}, 30.f, 8.f);

    CrsGameObject &player = scene.createGameObject();
    player.transform.translation = {12.f, 15.f, 0.f};  //na encosta (uns 22 graus)
    player.rigidBody.emplace();
    player.rigidBody->shape = ColliderShape::Capsule;
    player.rigidBody->radius = 0.4f;
    player.rigidBody->height = 1.8f;
    player.rigidBody->center = {0.f, 0.9f, 0.f};
    player.character.emplace();
    CrsGameObject &visual = scene.createGameObject();
    visual.parentId = player.getId();
    visual.faceMovement.emplace();
    visual.transform.rotation.y = 1.f;

    PhysicsSystem physics;
    CharacterControllerSystem controller;
    for (int i = 0; i < 120; i++) {  //cai e assenta
        controller.update(scene);
        physics.update(scene, 1.f / 60.f);
    }
    REQUIRE(player.rigidBody->onGround);
    const glm::vec3 rested = player.transform.translation;
    for (int i = 0; i < 180; i++) {  //3 s parado
        controller.update(scene);
        physics.update(scene, 1.f / 60.f);
        controller.faceMovement(scene, 1.f / 60.f);
    }
    CHECK(glm::length(player.transform.translation - rested) < 0.05f);
    CHECK(visual.transform.rotation.y == doctest::Approx(1.f));  //parado: continua olhando pra onde olhava
}

TEST_CASE("física: personagem descendo a ladeira andando continua grudado no chão") {
    CrsScene scene;
    CrsGameObject &ground = scene.createGameObject();
    ground.terrain.emplace();
    ground.terrain->data->raise({128.f, 128.f}, 30.f, 8.f);

    CrsGameObject &player = scene.createGameObject();
    player.transform.translation = {3.f, 15.f, 0.f};  //perto do topo
    player.rigidBody.emplace();
    player.rigidBody->shape = ColliderShape::Capsule;
    player.rigidBody->radius = 0.4f;
    player.rigidBody->height = 1.8f;
    player.rigidBody->center = {0.f, 0.9f, 0.f};
    player.character.emplace();

    PhysicsSystem physics;
    CharacterControllerSystem controller;
    for (int i = 0; i < 120; i++) {
        controller.update(scene);
        physics.update(scene, 1.f / 60.f);
    }
    player.character->input.moveDirection = {1.f, 0.f, 0.f};  //morro abaixo, correndo
    player.character->input.run = true;
    int airborne = 0;
    for (int i = 0; i < 120; i++) {
        controller.update(scene);
        physics.update(scene, 1.f / 60.f);
        if (!player.rigidBody->onGround) airborne++;
    }
    CHECK(player.transform.translation.x > 11.f);  //correu uns 10 m
    CHECK(airborne < 5);                          //sem ficar pulando rampa abaixo
    const float groundY = *scene.terrainHeightAt(player.transform.translation.x, player.transform.translation.z);
    CHECK(player.transform.translation.y == doctest::Approx(groundY).epsilon(0.03));
}
