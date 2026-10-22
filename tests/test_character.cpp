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

//personagem em pé num chão estático grande (mesma montagem do loadGameObjects)
static LveGameObject &addFloor(LveScene &scene) {
    LveGameObject &floor = scene.createGameObject();
    floor.transform.translation = {0.f, 1.5f, 0.f};
    floor.transform.scale = {10.f, 1.f, 10.f};
    floor.rigidBody.emplace();
    floor.rigidBody->type = BodyType::Static;
    floor.rigidBody->halfExtents = {1.f, 0.f, 1.f};
    return floor;
}

TEST_CASE("personagem apoiado no chão fica onGround") {
    LveScene scene;
    addFloor(scene);
    LveGameObject &player = addCharacter(scene);

    simulate(scene, CharacterInput{}, 1.f);

    CHECK(player.rigidBody->onGround);
}

TEST_CASE("pulo apoiado no chão sobe (y diminui: o Y aponta pra baixo)") {
    LveScene scene;
    addFloor(scene);
    LveGameObject &player = addCharacter(scene);
    simulate(scene, CharacterInput{}, 1.f);
    float standingY = player.transform.translation.y;

    CharacterInput jump;
    jump.jump = true;
    simulate(scene, jump, DT);  //um frame com o pulo
    simulate(scene, CharacterInput{}, 0.2f);

    CHECK(player.transform.translation.y < standingY - 0.5f);
    CHECK_FALSE(player.rigidBody->onGround);
}

TEST_CASE("pulo no ar não faz nada") {
    LveScene scene;
    addFloor(scene);
    LveGameObject &player = addCharacter(scene);
    simulate(scene, CharacterInput{}, 1.f);

    CharacterInput jump;
    jump.jump = true;
    simulate(scene, jump, DT);
    simulate(scene, CharacterInput{}, 0.2f);  //no ar, subindo
    float velocityBefore = player.rigidBody->velocity.y;

    simulate(scene, jump, DT);  //segundo pulo, ainda no ar

    //a velocidade segue só a gravidade: nada de impulso novo pra cima
    CHECK(player.rigidBody->velocity.y > velocityBefore);
}
