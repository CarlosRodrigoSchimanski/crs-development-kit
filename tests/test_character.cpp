#include "engine/gameplay/character_controller_system.hpp"
#include "engine/physics/crs_physics_system.hpp"

#include <doctest/doctest.h>

#include <glm/gtc/constants.hpp>

#include <cmath>

using namespace crsdk;

constexpr float DT = 1.f / 60.f;

//um personagem parado no ar (sem chão: só o movimento horizontal importa aqui)
static CrsGameObject &addCharacter(CrsScene &scene) {
    CrsGameObject &player = scene.createGameObject();
    player.rigidBody.emplace();
    player.character.emplace();
    return player;
}

//um frame do jogo: o controlador antes da física, como no FirstApp
static void simulate(CrsScene &scene, const CharacterInput &input, float seconds) {
    CharacterControllerSystem controller;
    PhysicsSystem physics;
    //arredonda: 1 / (1/60) dá 59.999... em float, e o static_cast sozinho cortaria pra 59
    int steps = static_cast<int>(std::lround(seconds / DT));
    for (int i = 0; i < steps; i++) {
        //no jogo quem preenche é o componente Python, antes do controlador
        for (auto &kv : scene.getGameObjects()) {
            if (kv.second.character) kv.second.character->input = input;
        }
        controller.update(scene);
        physics.update(scene, DT);
    }
}

TEST_CASE("personagem anda na direção pedida com a velocidade dele") {
    CrsScene scene;
    CrsGameObject &player = addCharacter(scene);
    CharacterInput input;
    input.moveDirection = {0.f, 0.f, 1.f};

    simulate(scene, input, 1.f);

    CHECK(player.transform.translation.z == doctest::Approx(CharacterComponent{}.moveSpeed).epsilon(0.01));
    CHECK(player.transform.translation.x == doctest::Approx(0.f));
}

TEST_CASE("correndo (Shift) usa a velocidade de corrida") {
    CrsScene scene;
    CrsGameObject &player = addCharacter(scene);
    CharacterInput input;
    input.moveDirection = {0.f, 0.f, 1.f};
    input.run = true;

    simulate(scene, input, 1.f);

    CHECK(player.transform.translation.z == doctest::Approx(CharacterComponent{}.runSpeed).epsilon(0.01));
}

TEST_CASE("andar na diagonal não fica mais rápido") {
    CrsScene scene;
    CrsGameObject &player = addCharacter(scene);
    CharacterInput input;
    input.moveDirection = {1.f, 0.f, 1.f};  //W + D: comprimento 1.41

    simulate(scene, input, 1.f);

    glm::vec2 walked{player.transform.translation.x, player.transform.translation.z};
    CHECK(glm::length(walked) == doctest::Approx(CharacterComponent{}.moveSpeed).epsilon(0.01));
}

TEST_CASE("sem intenção de movimento o personagem para") {
    CrsScene scene;
    CrsGameObject &player = addCharacter(scene);
    CharacterInput walk;
    walk.moveDirection = {1.f, 0.f, 0.f};
    simulate(scene, walk, 0.5f);
    float stoppedAt = player.transform.translation.x;

    simulate(scene, CharacterInput{}, 0.5f);

    CHECK(player.transform.translation.x == doctest::Approx(stoppedAt));
}

TEST_CASE("objeto sem componente de personagem não é movido pelo controlador") {
    CrsScene scene;
    CrsGameObject &box = scene.createGameObject();
    box.rigidBody.emplace();
    CharacterInput input;
    input.moveDirection = {1.f, 0.f, 0.f};

    simulate(scene, input, 1.f);

    CHECK(box.transform.translation.x == doctest::Approx(0.f));
}

//personagem em pé num chão estático grande (mesma montagem do loadGameObjects)
static CrsGameObject &addFloor(CrsScene &scene) {
    CrsGameObject &floor = scene.createGameObject();
    floor.transform.translation = {0.f, -1.5f, 0.f};
    floor.transform.scale = {10.f, 1.f, 10.f};
    floor.rigidBody.emplace();
    floor.rigidBody->type = BodyType::Static;
    floor.rigidBody->halfExtents = {1.f, 0.f, 1.f};
    return floor;
}

TEST_CASE("personagem apoiado no chão fica onGround") {
    CrsScene scene;
    addFloor(scene);
    CrsGameObject &player = addCharacter(scene);

    simulate(scene, CharacterInput{}, 1.f);

    CHECK(player.rigidBody->onGround);
}

TEST_CASE("pulo apoiado no chão sobe (y aumenta)") {
    CrsScene scene;
    addFloor(scene);
    CrsGameObject &player = addCharacter(scene);
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
    CrsScene scene;
    addFloor(scene);
    CrsGameObject &player = addCharacter(scene);
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
static CrsGameObject &addVisual(CrsScene &scene, CrsGameObject &body) {
    CrsGameObject &visual = scene.createGameObject();
    visual.parentId = body.getId();
    visual.faceMovement.emplace();
    visual.faceMovement->turnSpeed = 1000.f;  //vira na hora, pro teste olhar só a direção final
    return visual;
}

TEST_CASE("faceMovement: o visual fica de frente pro movimento (com o desvio do modelo)") {
    CrsScene scene;
    CrsGameObject &body = addCharacter(scene);
    CrsGameObject &visual = addVisual(scene, body);
    body.rigidBody->velocity = {1.f, 0.f, 0.f};  //andando pro +X

    CharacterControllerSystem controller;
    controller.faceMovement(scene, DT);

    //a frente do modelo é o +Z dele: depois de virar, esse +Z tem que apontar pro +X
    glm::vec3 modelFront = visual.transform.rotationMatrix()[2];
    CHECK(modelFront.x == doctest::Approx(1.f).epsilon(1e-4));
    CHECK(std::abs(modelFront.z) < 1e-4f);
}

TEST_CASE("faceMovement: parado, mantém a última direção") {
    CrsScene scene;
    CrsGameObject &body = addCharacter(scene);
    CrsGameObject &visual = addVisual(scene, body);
    visual.transform.rotation.y = 1.234f;
    body.rigidBody->velocity = {0.f, -3.f, 0.f};  //só caindo: nada no chão

    CharacterControllerSystem controller;
    controller.faceMovement(scene, DT);

    CHECK(visual.transform.rotation.y == doctest::Approx(1.234f));
}

TEST_CASE("cada personagem anda pela própria intenção, e o pedido de pulo vale um frame só") {
    CrsScene scene;
    addFloor(scene);
    CrsGameObject &a = addCharacter(scene);
    CrsGameObject &b = addCharacter(scene);
    a.transform.translation.x = -3.f;
    b.transform.translation.x = 3.f;
    CharacterControllerSystem controller;
    PhysicsSystem physics;
    for (int i = 0; i < 60; i++) physics.update(scene, DT);  //os dois apoiam no chão

    a.character->input.moveDirection = {1.f, 0.f, 0.f};
    a.character->input.jump = true;
    controller.update(scene);

    CHECK(a.rigidBody->velocity.x == doctest::Approx(a.character->moveSpeed));
    CHECK(b.rigidBody->velocity.x == doctest::Approx(0.f));  //ninguém pediu nada pro b
    CHECK(a.rigidBody->velocity.y == doctest::Approx(a.character->jumpSpeed));
    //usou o pulo: no frame seguinte só pula de novo se quem controla pedir de novo
    CHECK_FALSE(a.character->input.jump);
    //a direção continua até quem controla mudar
    CHECK(a.character->input.moveDirection.x == doctest::Approx(1.f));
}

//uma caixa (parede, degrau, caixote) no meio do caminho
static CrsGameObject &addBox(CrsScene &scene, glm::vec3 center, glm::vec3 half, BodyType type) {
    CrsGameObject &box = scene.createGameObject();
    box.transform.translation = center;
    box.rigidBody.emplace();
    box.rigidBody->type = type;
    box.rigidBody->halfExtents = half;
    return box;
}

//nesses testes o personagem é a cápsula do jogador (origem nos pés), em pé no chão (y = -1.5)
static CrsGameObject &addCapsuleCharacter(CrsScene &scene) {
    CrsGameObject &player = addCharacter(scene);
    player.transform.translation = {0.f, -1.5f, 0.f};
    player.rigidBody->shape = ColliderShape::Capsule;
    player.rigidBody->radius = 0.4f;
    player.rigidBody->height = 1.74f;
    player.rigidBody->center = {0.f, 0.87f, 0.f};
    player.rigidBody->mass = 70.f;
    return player;
}

TEST_CASE("personagem não atravessa parede") {
    CrsScene scene;
    addFloor(scene);
    CrsGameObject &player = addCapsuleCharacter(scene);
    //parede com a face da frente em z = 2.75
    addBox(scene, {0.f, -0.5f, 3.f}, {3.f, 1.f, 0.25f}, BodyType::Static);
    CharacterInput input;
    input.moveDirection = {0.f, 0.f, 1.f};
    input.run = true;

    simulate(scene, input, 3.f);

    //para encostado: a frente da cápsula (raio 0.4) na face da parede
    CHECK(player.transform.translation.z < 2.75f - 0.4f + 0.05f);
    CHECK(player.transform.translation.z > 2.f);
    //andando contra a parede a velocidade de verdade é zero (a animação fica parada)
    CHECK(std::abs(player.rigidBody->velocity.z) < 0.1f);
}

TEST_CASE("andando na diagonal contra a parede, o personagem desliza ao longo dela") {
    CrsScene scene;
    addFloor(scene);
    CrsGameObject &player = addCapsuleCharacter(scene);
    addBox(scene, {0.f, -0.5f, 1.f}, {10.f, 1.f, 0.25f}, BodyType::Static);
    CharacterInput input;
    input.moveDirection = {1.f, 0.f, 1.f};

    simulate(scene, input, 2.f);

    CHECK(player.transform.translation.z < 0.75f - 0.4f + 0.05f);
    CHECK(player.transform.translation.x > 1.f);  //seguiu de lado
}

TEST_CASE("personagem empurra a caixa dinâmica") {
    CrsScene scene;
    addFloor(scene);
    CrsGameObject &player = addCapsuleCharacter(scene);
    CrsGameObject &crate = addBox(scene, {0.f, -1.f, 1.5f}, {0.5f, 0.5f, 0.5f}, BodyType::Dynamic);
    CharacterInput input;
    input.moveDirection = {0.f, 0.f, 1.f};

    simulate(scene, input, 3.f);

    CHECK(crate.transform.translation.z > 2.5f);
    //a caixa foi na frente: o personagem não passou por dentro dela
    CHECK(player.transform.translation.z < crate.transform.translation.z);
}

TEST_CASE("personagem sobe degrau baixo (20 cm) andando") {
    CrsScene scene;
    addFloor(scene);
    CrsGameObject &player = addCapsuleCharacter(scene);
    //degrau de 20 cm, começando em z = 1
    addBox(scene, {0.f, -1.4f, 3.f}, {3.f, 0.1f, 2.f}, BodyType::Static);
    CharacterInput input;
    input.moveDirection = {0.f, 0.f, 1.f};

    simulate(scene, input, 2.f);

    CHECK(player.transform.translation.z > 1.5f);
    CHECK(player.transform.translation.y == doctest::Approx(-1.3f).epsilon(0.03));
}
