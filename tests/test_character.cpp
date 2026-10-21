#include "engine/gameplay/character_controller_system.hpp"
#include "engine/physics/lve_physics_system.hpp"

#include <doctest/doctest.h>

#include <cmath>

using namespace lve;

constexpr float DT = 1.f / 60.f;

//um personagem parado no ar (sem chão: só o movimento horizontal importa aqui)
static LveGameObject &addCharacter(LveScene &scene) {
    LveGameObject &player = scene.createGameObject();
    player.rigidBody.emplace();
    player.character.emplace();
    return player;
}

//um frame do jogo: o controlador antes da física, como no FirstApp
static void simulate(LveScene &scene, const CharacterInput &input, float seconds) {
    CharacterControllerSystem controller;
    PhysicsSystem physics;
    //arredonda: 1 / (1/60) dá 59.999... em float, e o static_cast sozinho cortaria pra 59
    int steps = static_cast<int>(std::lround(seconds / DT));
    for (int i = 0; i < steps; i++) {
        controller.update(scene, input);
        physics.update(scene, DT);
    }
}

TEST_CASE("personagem anda na direção pedida com a velocidade dele") {
    LveScene scene;
    LveGameObject &player = addCharacter(scene);
    CharacterInput input;
    input.moveDirection = {0.f, 0.f, 1.f};

    simulate(scene, input, 1.f);

    //moveSpeed padrão = 4 por segundo
    CHECK(player.transform.translation.z == doctest::Approx(4.f).epsilon(0.01));
    CHECK(player.transform.translation.x == doctest::Approx(0.f));
}

TEST_CASE("andar na diagonal não fica mais rápido") {
    LveScene scene;
    LveGameObject &player = addCharacter(scene);
    CharacterInput input;
    input.moveDirection = {1.f, 0.f, 1.f};  //W + D: comprimento 1.41

    simulate(scene, input, 1.f);

    glm::vec2 walked{player.transform.translation.x, player.transform.translation.z};
    CHECK(glm::length(walked) == doctest::Approx(4.f).epsilon(0.01));
}

TEST_CASE("sem intenção de movimento o personagem para") {
    LveScene scene;
    LveGameObject &player = addCharacter(scene);
    CharacterInput walk;
    walk.moveDirection = {1.f, 0.f, 0.f};
    simulate(scene, walk, 0.5f);
    float stoppedAt = player.transform.translation.x;

    simulate(scene, CharacterInput{}, 0.5f);

    CHECK(player.transform.translation.x == doctest::Approx(stoppedAt));
}

TEST_CASE("objeto sem componente de personagem não é movido pelo controlador") {
    LveScene scene;
    LveGameObject &box = scene.createGameObject();
    box.rigidBody.emplace();
    CharacterInput input;
    input.moveDirection = {1.f, 0.f, 0.f};

    simulate(scene, input, 1.f);

    CHECK(box.transform.translation.x == doctest::Approx(0.f));
}
