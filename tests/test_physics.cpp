#include "engine/physics/lve_physics_system.hpp"

#include <doctest/doctest.h>

using namespace lve;

//passo fixo de 60 quadros por segundo, como um jogo rodando liso
constexpr float DT = 1.f / 60.f;

//roda a física por uma quantidade de segundos simulados
static void simulate(PhysicsSystem &physics, LveScene &scene, float seconds) {
    int steps = static_cast<int>(seconds / DT);
    for (int i = 0; i < steps; i++) {
        physics.update(scene, DT);
    }
}

TEST_CASE("corpo dinâmico cai com a gravidade (+Y é pra baixo)") {
    LveScene scene;
    PhysicsSystem physics;
    LveGameObject &cube = scene.createGameObject();
    cube.rigidBody.emplace();

    simulate(physics, scene, 1.f);

    //queda livre: 0.5 * g * t² = 4.9 em 1 segundo (2% de folga pela integração por passos)
    CHECK(cube.transform.translation.y == doctest::Approx(4.9f).epsilon(0.02));
    CHECK(cube.rigidBody->velocity.y > 0.f);
}

TEST_CASE("cubo pousa no chão estático e para") {
    LveScene scene;
    PhysicsSystem physics;

    //mesma cena do loadGameObjects: cubo de lado 2 na origem, chão chato em y = 1.5
    LveGameObject &cube = scene.createGameObject();
    cube.rigidBody.emplace();

    LveGameObject &floor = scene.createGameObject();
    floor.transform.translation = {0.f, 1.5f, 0.f};
    floor.transform.scale = {5.f, 1.f, 5.f};
    floor.rigidBody.emplace();
    floor.rigidBody->type = BodyType::Static;
    floor.rigidBody->halfExtents = {1.f, 0.f, 1.f};

    simulate(physics, scene, 2.f);

    //centro do cubo meia altura (1) acima da superfície do chão (1.5)
    CHECK(cube.transform.translation.y == doctest::Approx(0.5f));
    CHECK(cube.rigidBody->velocity.y == doctest::Approx(0.f));
    //o chão não se mexe
    CHECK(floor.transform.translation.y == doctest::Approx(1.5f));
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
