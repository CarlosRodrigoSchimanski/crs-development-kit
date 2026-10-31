#include "engine/physics/crs_physics_system.hpp"

#include <doctest/doctest.h>

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>

using namespace crsdk;

//passo fixo de 60 quadros por segundo, como um jogo rodando liso
constexpr float DT = 1.f / 60.f;
//corpo apoiado afunda até 2 cm no que está embaixo (a "penetration slop" da Jolt: evita tremer)
constexpr float SLOP = 0.025f;

//roda a física por uma quantidade de segundos simulados
static void simulate(PhysicsSystem &physics, CrsScene &scene, float seconds) {
    //arredonda: 1 / (1/60) dá 59.999... em float, e o static_cast sozinho cortaria pra 59
    int steps = static_cast<int>(std::lround(seconds / DT));
    for (int i = 0; i < steps; i++) {
        physics.update(scene, DT);
    }
}

TEST_CASE("corpo dinâmico cai com a gravidade (pro -Y)") {
    CrsScene scene;
    PhysicsSystem physics;
    CrsGameObject &cube = scene.createGameObject();
    cube.rigidBody.emplace();

    simulate(physics, scene, 1.f);

    //queda livre: 0.5 * g * t² = 4.9 em 1 segundo (2% de folga pela integração por passos)
    CHECK(cube.transform.translation.y == doctest::Approx(-4.9f).epsilon(0.02));
    CHECK(cube.rigidBody->velocity.y < 0.f);
}

TEST_CASE("cubo pousa no chão estático e para") {
    CrsScene scene;
    PhysicsSystem physics;

    //mesma cena do loadGameObjects: cubo de lado 2 em y = 1.5, chão chato em y = 0
    CrsGameObject &cube = scene.createGameObject();
    cube.transform.translation = {0.f, 1.5f, 0.f};
    cube.rigidBody.emplace();

    CrsGameObject &floor = scene.createGameObject();
    floor.transform.scale = {5.f, 1.f, 5.f};
    floor.rigidBody.emplace();
    floor.rigidBody->type = BodyType::Static;
    floor.rigidBody->halfExtents = {1.f, 0.f, 1.f};

    simulate(physics, scene, 2.f);

    //centro do cubo meia altura (1) acima da superfície do chão (0)
    CHECK(cube.transform.translation.y == doctest::Approx(1.f).epsilon(SLOP));
    CHECK(cube.rigidBody->velocity.y == doctest::Approx(0.f).epsilon(0.01));
    CHECK(cube.rigidBody->onGround);
    //o chão não se mexe
    CHECK(floor.transform.translation.y == doctest::Approx(0.f));
}

TEST_CASE("objeto sem corpo físico é ignorado") {
    CrsScene scene;
    PhysicsSystem physics;
    CrsGameObject &prop = scene.createGameObject();
    prop.transform.translation = {1.f, 2.f, 3.f};

    simulate(physics, scene, 1.f);

    CHECK(prop.transform.translation.x == doctest::Approx(1.f));
    CHECK(prop.transform.translation.y == doctest::Approx(2.f));
    CHECK(prop.transform.translation.z == doctest::Approx(3.f));
}

TEST_CASE("origem nos pés: o corpo pousa com os pés no chão") {
    CrsScene scene;
    PhysicsSystem physics;

    //personagem: caixa de altura 1.8 acima da origem
    CrsGameObject &player = scene.createGameObject();
    player.transform.translation = {0.f, 2.f, 0.f};
    player.transform.scale = {0.4f, 0.9f, 0.4f};
    player.rigidBody.emplace();
    player.rigidBody->center = {0.f, 1.f, 0.f};

    CrsGameObject &floor = scene.createGameObject();
    floor.transform.translation = {0.f, -0.5f, 0.f};
    floor.transform.scale = {10.f, 1.f, 10.f};
    floor.rigidBody.emplace();
    floor.rigidBody->type = BodyType::Static;
    floor.rigidBody->halfExtents = {1.f, 0.f, 1.f};

    simulate(physics, scene, 2.f);

    //a origem (os pés) fica exatamente na superfície do chão
    CHECK(std::abs(player.transform.translation.y - -0.5f) < SLOP);
    CHECK(player.rigidBody->onGround);
}

//chão grande e chato em y = 0, como o da cena
static CrsGameObject &addFloor(CrsScene &scene) {
    CrsGameObject &floor = scene.createGameObject();
    floor.transform.scale = {20.f, 1.f, 20.f};
    floor.rigidBody.emplace();
    floor.rigidBody->type = BodyType::Static;
    floor.rigidBody->halfExtents = {1.f, 0.f, 1.f};
    return floor;
}

TEST_CASE("esfera pousa com o centro a um raio do chão") {
    CrsScene scene;
    PhysicsSystem physics;
    addFloor(scene);
    CrsGameObject &ball = scene.createGameObject();
    ball.transform.translation = {0.f, 3.f, 0.f};
    ball.rigidBody.emplace();
    ball.rigidBody->shape = ColliderShape::Sphere;
    ball.rigidBody->radius = 0.5f;

    simulate(physics, scene, 3.f);

    CHECK(std::abs(ball.transform.translation.y - 0.5f) < SLOP);
    CHECK(ball.rigidBody->onGround);
}

TEST_CASE("cápsula com origem nos pés pousa de pé com os pés no chão") {
    CrsScene scene;
    PhysicsSystem physics;
    addFloor(scene);
    CrsGameObject &body = scene.createGameObject();
    body.transform.translation = {0.f, 2.f, 0.f};
    body.rigidBody.emplace();
    body.rigidBody->shape = ColliderShape::Capsule;
    body.rigidBody->radius = 0.4f;
    body.rigidBody->height = 1.8f;
    body.rigidBody->center = {0.f, 0.9f, 0.f};

    simulate(physics, scene, 2.f);

    CHECK(std::abs(body.transform.translation.y) < SLOP);
}

TEST_CASE("a escala do objeto aumenta a forma") {
    CrsScene scene;
    PhysicsSystem physics;
    addFloor(scene);
    //cubo de lado 2 com escala 2: lado 4, o centro para a 2 do chão
    CrsGameObject &cube = scene.createGameObject();
    cube.transform.translation = {0.f, 5.f, 0.f};
    cube.transform.scale = glm::vec3{2.f};
    cube.rigidBody.emplace();

    simulate(physics, scene, 3.f);

    CHECK(cube.transform.translation.y == doctest::Approx(2.f).epsilon(SLOP));
}

TEST_CASE("caixa que cai inclinada tomba e termina deitada numa face") {
    CrsScene scene;
    PhysicsSystem physics;
    addFloor(scene);
    CrsGameObject &cube = scene.createGameObject();
    cube.transform.translation = {0.f, 3.f, 0.f};
    cube.transform.rotation = {0.f, 0.f, glm::radians(30.f)};  //cai na quina
    cube.rigidBody.emplace();
    cube.rigidBody->halfExtents = {0.5f, 0.5f, 0.5f};

    simulate(physics, scene, 4.f);

    //deitada numa face: o centro a meio lado do chão, e algum eixo da caixa de pé
    CHECK(cube.transform.translation.y == doctest::Approx(0.5f).epsilon(SLOP * 2.f));
    glm::mat3 r = cube.transform.rotationMatrix();
    float upright = std::max({std::abs(r[0].y), std::abs(r[1].y), std::abs(r[2].y)});
    CHECK(upright > 0.99f);
}

TEST_CASE("mudar a forma refaz o corpo (esfera maior para mais alto)") {
    CrsScene scene;
    PhysicsSystem physics;
    addFloor(scene);
    CrsGameObject &ball = scene.createGameObject();
    ball.transform.translation = {0.f, 2.f, 0.f};
    ball.rigidBody.emplace();
    ball.rigidBody->shape = ColliderShape::Sphere;
    ball.rigidBody->radius = 0.5f;
    simulate(physics, scene, 2.f);

    ball.rigidBody->radius = 1.f;
    ball.transform.translation.y = 3.f;
    simulate(physics, scene, 3.f);

    CHECK(std::abs(ball.transform.translation.y - 1.f) < SLOP);
}

TEST_CASE("objeto movido por fora (editor, script) é levado pro lugar novo") {
    CrsScene scene;
    PhysicsSystem physics;
    addFloor(scene);
    CrsGameObject &cube = scene.createGameObject();
    cube.transform.translation = {0.f, 1.f, 0.f};
    cube.rigidBody.emplace();
    simulate(physics, scene, 1.f);

    cube.transform.translation = {5.f, 1.f, -3.f};
    simulate(physics, scene, 1.f);

    CHECK(cube.transform.translation.x == doctest::Approx(5.f).epsilon(0.01));
    CHECK(cube.transform.translation.z == doctest::Approx(-3.f).epsilon(0.01));
}

TEST_CASE("velocidade mudada pelo script vira impulso (pra cima: sobe)") {
    CrsScene scene;
    PhysicsSystem physics;
    addFloor(scene);
    CrsGameObject &cube = scene.createGameObject();
    cube.transform.translation = {0.f, 1.f, 0.f};
    cube.rigidBody.emplace();
    simulate(physics, scene, 1.f);

    cube.rigidBody->velocity.y = 5.f;
    simulate(physics, scene, 0.25f);

    CHECK(cube.transform.translation.y > 1.5f);
}

TEST_CASE("tirar o RigidBody tira o objeto da física (para de cair)") {
    CrsScene scene;
    PhysicsSystem physics;
    CrsGameObject &cube = scene.createGameObject();
    cube.rigidBody.emplace();
    simulate(physics, scene, 0.5f);

    cube.rigidBody.reset();
    float y = cube.transform.translation.y;
    simulate(physics, scene, 0.5f);

    CHECK(cube.transform.translation.y == doctest::Approx(y));
}

TEST_CASE("clear esquece os corpos: a mesma cena recomeça do zero") {
    CrsScene scene;
    PhysicsSystem physics;
    CrsGameObject &cube = scene.createGameObject();
    cube.rigidBody.emplace();
    simulate(physics, scene, 0.5f);

    physics.clear();
    cube.transform.translation = {0.f, 0.f, 0.f};
    cube.rigidBody->velocity = {0.f, 0.f, 0.f};
    simulate(physics, scene, 1.f);

    CHECK(cube.transform.translation.y == doctest::Approx(-4.9f).epsilon(0.02));
}

TEST_CASE("caixa que envolve a forma: esfera e cápsula") {
    RigidBodyComponent body{};
    body.shape = ColliderShape::Sphere;
    body.radius = 0.5f;
    body.center = {0.f, 1.f, 0.f};
    CHECK(body.boundsMin().y == doctest::Approx(0.5f));
    CHECK(body.boundsMax().x == doctest::Approx(0.5f));

    body.shape = ColliderShape::Capsule;
    body.radius = 0.4f;
    body.height = 1.8f;
    body.center = {0.f, 0.9f, 0.f};
    CHECK(body.boundsMin().y == doctest::Approx(0.f));
    CHECK(body.boundsMax().y == doctest::Approx(1.8f));
    CHECK(body.boundsMax().z == doctest::Approx(0.4f));
}
