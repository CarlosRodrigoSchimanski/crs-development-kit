#include "engine/gameplay/character_controller_system.hpp"
#include "engine/physics/lve_physics_system.hpp"

#include <doctest/doctest.h>

#include <glm/gtc/constants.hpp>

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

    CHECK(player.transform.translation.z == doctest::Approx(CharacterComponent{}.moveSpeed).epsilon(0.01));
    CHECK(player.transform.translation.x == doctest::Approx(0.f));
}

TEST_CASE("correndo (Shift) usa a velocidade de corrida") {
    LveScene scene;
    LveGameObject &player = addCharacter(scene);
    CharacterInput input;
    input.moveDirection = {0.f, 0.f, 1.f};
    input.run = true;

    simulate(scene, input, 1.f);

    CHECK(player.transform.translation.z == doctest::Approx(CharacterComponent{}.runSpeed).epsilon(0.01));
}

TEST_CASE("andar na diagonal não fica mais rápido") {
    LveScene scene;
    LveGameObject &player = addCharacter(scene);
    CharacterInput input;
    input.moveDirection = {1.f, 0.f, 1.f};  //W + D: comprimento 1.41

    simulate(scene, input, 1.f);

    glm::vec2 walked{player.transform.translation.x, player.transform.translation.z};
    CHECK(glm::length(walked) == doctest::Approx(CharacterComponent{}.moveSpeed).epsilon(0.01));
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
    floor.transform.translation = {0.f, -1.5f, 0.f};
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

TEST_CASE("pulo apoiado no chão sobe (y aumenta)") {
    LveScene scene;
    addFloor(scene);
    LveGameObject &player = addCharacter(scene);
    simulate(scene, CharacterInput{}, 1.f);
    float standingY = player.transform.translation.y;

    CharacterInput jump;
    jump.jump = true;
    simulate(scene, jump, DT);  //um frame com o pulo
    simulate(scene, CharacterInput{}, 0.2f);

    CHECK(player.transform.translation.y > standingY + 0.5f);
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
    CHECK(player.rigidBody->velocity.y < velocityBefore);
}

TEST_CASE("turnTowards: vira pelo caminho mais curto (de 170 pra -170 graus são 20, não 340)") {
    float current = glm::radians(170.f);
    float target = glm::radians(-170.f);

    float turned = turnTowards(current, target, glm::radians(5.f));

    //deu 5 graus pra frente (passando dos 180), e não 5 graus pra trás
    CHECK(turned == doctest::Approx(glm::radians(175.f)));
}

TEST_CASE("turnTowards: não passa do alvo") {
    float turned = turnTowards(0.f, glm::radians(10.f), glm::radians(90.f));
    CHECK(turned == doctest::Approx(glm::radians(10.f)));
}

//cápsula se movendo + visual filho que vira pro movimento
static LveGameObject &addVisual(LveScene &scene, LveGameObject &body) {
    LveGameObject &visual = scene.createGameObject();
    visual.parentId = body.getId();
    visual.faceMovement.emplace();
    visual.faceMovement->turnSpeed = 1000.f;  //vira na hora, pro teste olhar só a direção final
    return visual;
}

TEST_CASE("faceMovement: o visual fica de frente pro movimento (com o desvio do modelo)") {
    LveScene scene;
    LveGameObject &body = addCharacter(scene);
    LveGameObject &visual = addVisual(scene, body);
    body.rigidBody->velocity = {1.f, 0.f, 0.f};  //andando pro +X

    CharacterControllerSystem controller;
    controller.faceMovement(scene, DT);

    //a frente do modelo é o +Z dele: depois de virar, esse +Z tem que apontar pro +X
    glm::vec3 modelFront = visual.transform.rotationMatrix()[2];
    CHECK(modelFront.x == doctest::Approx(1.f).epsilon(1e-4));
    CHECK(std::abs(modelFront.z) < 1e-4f);
}

TEST_CASE("faceMovement: parado, mantém a última direção") {
    LveScene scene;
    LveGameObject &body = addCharacter(scene);
    LveGameObject &visual = addVisual(scene, body);
    visual.transform.rotation.y = 1.234f;
    body.rigidBody->velocity = {0.f, -3.f, 0.f};  //só caindo: nada no chão

    CharacterControllerSystem controller;
    controller.faceMovement(scene, DT);

    CHECK(visual.transform.rotation.y == doctest::Approx(1.234f));
}
