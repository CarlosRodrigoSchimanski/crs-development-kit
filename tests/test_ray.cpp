#include "engine/scene/lve_ray.hpp"

#include <doctest/doctest.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace lve;

//caixa de lado 2 centrada na origem (como o cubo de teste)
const glm::vec3 BOX_MIN{-1.f};
const glm::vec3 BOX_MAX{1.f};

TEST_CASE("raio de frente acerta a caixa e diz a distância até a face") {
    Ray ray{{0.f, 0.f, -5.f}, {0.f, 0.f, 1.f}};

    auto t = intersectRayAabb(ray, BOX_MIN, BOX_MAX);

    REQUIRE(t.has_value());
    CHECK(*t == doctest::Approx(4.f));  //a face da frente está em z = -1
}

TEST_CASE("raio que passa ao lado não acerta") {
    Ray ray{{3.f, 0.f, -5.f}, {0.f, 0.f, 1.f}};
    CHECK_FALSE(intersectRayAabb(ray, BOX_MIN, BOX_MAX).has_value());
}

TEST_CASE("caixa atrás da origem do raio não conta") {
    Ray ray{{0.f, 0.f, 5.f}, {0.f, 0.f, 1.f}};  //olhando pra longe da caixa
    CHECK_FALSE(intersectRayAabb(ray, BOX_MIN, BOX_MAX).has_value());
}

TEST_CASE("caixa chata (plano) também é acertada") {
    Ray ray{{0.f, -5.f, 0.f}, {0.f, 1.f, 0.f}};
    auto t = intersectRayAabb(ray, {-1.f, 0.f, -1.f}, {1.f, 0.f, 1.f});
    REQUIRE(t.has_value());
    CHECK(*t == doctest::Approx(5.f));
}

TEST_CASE("entre dois objetos no caminho, o da frente tem o menor t") {
    Ray ray{{0.f, 0.f, -10.f}, {0.f, 0.f, 1.f}};
    glm::mat4 front = glm::translate(glm::mat4{1.f}, {0.f, 0.f, 0.f});
    glm::mat4 back = glm::translate(glm::mat4{1.f}, {0.f, 0.f, 5.f});

    auto tFront = intersectRayObject(ray, front, BOX_MIN, BOX_MAX);
    auto tBack = intersectRayObject(ray, back, BOX_MIN, BOX_MAX);

    REQUIRE(tFront.has_value());
    REQUIRE(tBack.has_value());
    CHECK(*tFront < *tBack);
}

TEST_CASE("objeto girado: o raio é levado pro espaço dele") {
    //caixa comprida em X (de -3 a 3), girada 90 graus em Y: no mundo ela fica comprida em Z
    glm::mat4 world = glm::rotate(glm::mat4{1.f}, glm::half_pi<float>(), glm::vec3{0.f, 1.f, 0.f});
    glm::vec3 longMin{-3.f, -0.5f, -0.5f};
    glm::vec3 longMax{3.f, 0.5f, 0.5f};

    //passa em x = 2: acertaria a caixa SEM girar, mas não acerta a caixa girada
    Ray sideRay{{2.f, 0.f, -10.f}, {0.f, 0.f, 1.f}};
    CHECK_FALSE(intersectRayObject(sideRay, world, longMin, longMax).has_value());

    //passa em z = 2 vindo de lado: só acerta porque a caixa girada ficou comprida em Z
    Ray alongRay{{-10.f, 0.f, 2.f}, {1.f, 0.f, 0.f}};
    CHECK(intersectRayObject(alongRay, world, longMin, longMax).has_value());
}
