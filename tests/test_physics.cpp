#include "engine/physics/lve_physics_system.hpp"

#include <doctest/doctest.h>

#include <cmath>

using namespace lve;

//passo fixo de 60 quadros por segundo, como um jogo rodando liso
constexpr float DT = 1.f / 60.f;

//roda a física por uma quantidade de segundos simulados
static void simulate(PhysicsSystem &physics, LveScene &scene, float seconds) {
    //arredonda: 1 / (1/60) dá 59.999... em float, e o static_cast sozinho cortaria pra 59
    int steps = static_cast<int>(std::lround(seconds / DT));
    for (int i = 0; i < steps; i++) {
        physics.update(scene, DT);
    }
}

TEST_CASE("corpo dinâmico cai com a gravidade (pro -Y)") {
    LveScene scene;
    PhysicsSystem physics;
    LveGameObject &cube = scene.createGameObject();
    cube.rigidBody.emplace();

    simulate(physics, scene, 1.f);

    //queda livre: 0.5 * g * t² = 4.9 em 1 segundo (2% de folga pela integração por passos)
    CHECK(cube.transform.translation.y == doctest::Approx(-4.9f).epsilon(0.02));
    CHECK(cube.rigidBody->velocity.y < 0.f);
}

TEST_CASE("cubo pousa no chão estático e para") {
    LveScene scene;
    PhysicsSystem physics;

    //mesma cena do loadGameObjects: cubo de lado 2 em y = 1.5, chão chato em y = 0
    LveGameObject &cube = scene.createGameObject();
    cube.transform.translation = {0.f, 1.5f, 0.f};
    cube.rigidBody.emplace();

    LveGameObject &floor = scene.createGameObject();
    floor.transform.scale = {5.f, 1.f, 5.f};
    floor.rigidBody.emplace();
    floor.rigidBody->type = BodyType::Static;
    floor.rigidBody->halfExtents = {1.f, 0.f, 1.f};

    simulate(physics, scene, 2.f);

    //centro do cubo meia altura (1) acima da superfície do chão (0)
    CHECK(cube.transform.translation.y == doctest::Approx(1.f));
    CHECK(cube.rigidBody->velocity.y == doctest::Approx(0.f));
    //o chão não se mexe
    CHECK(floor.transform.translation.y == doctest::Approx(0.f));
}

TEST_CASE("objeto sem corpo físico é ignorado") {
    LveScene scene;
    PhysicsSystem physics;
    LveGameObject &prop = scene.createGameObject();
    prop.transform.translation = {1.f, 2.f, 3.f};

    simulate(physics, scene, 1.f);

    CHECK(prop.transform.translation.x == doctest::Approx(1.f));
    CHECK(prop.transform.translation.y == doctest::Approx(2.f));
    CHECK(prop.transform.translation.z == doctest::Approx(3.f));
}

TEST_CASE("origem nos pés: o corpo pousa com os pés no chão") {
    LveScene scene;
    PhysicsSystem physics;

    //personagem: caixa de altura 1.8 acima da origem
    LveGameObject &player = scene.createGameObject();
    player.transform.translation = {0.f, 2.f, 0.f};
    player.transform.scale = {0.4f, 0.9f, 0.4f};
    player.rigidBody.emplace();
    player.rigidBody->center = {0.f, 1.f, 0.f};

    LveGameObject &floor = scene.createGameObject();
    floor.transform.translation = {0.f, -0.5f, 0.f};
    floor.transform.scale = {10.f, 1.f, 10.f};
    floor.rigidBody.emplace();
    floor.rigidBody->type = BodyType::Static;
    floor.rigidBody->halfExtents = {1.f, 0.f, 1.f};

    simulate(physics, scene, 2.f);

    //a origem (os pés) fica exatamente na superfície do chão
    CHECK(player.transform.translation.y == doctest::Approx(-0.5f));
    CHECK(player.rigidBody->onGround);
}
