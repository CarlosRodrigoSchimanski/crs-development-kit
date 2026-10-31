#include "engine/terrain/crs_terrain.hpp"
#include "engine/gameplay/character_controller_system.hpp"
#include "engine/physics/crs_physics_system.hpp"
#include "engine/scene/scene_serializer.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <filesystem>
#include <fstream>

using namespace crsdk;

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
