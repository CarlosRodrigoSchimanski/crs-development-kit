#include "engine/scene/crs_game_object.hpp"

#include <doctest/doctest.h>

using namespace crsdk;

//duas matrizes iguais, número a número (com folga de arredondamento)
static void checkSameMatrix(const glm::mat4 &a, const glm::mat4 &b) {
    for (int column = 0; column < 4; column++) {
        for (int row = 0; row < 4; row++) {
            CHECK(a[column][row] == doctest::Approx(b[column][row]).epsilon(1e-4));
        }
    }
}

TEST_CASE("setFromMatrix desfaz o mat4 (ida e volta com rotação nos três eixos)") {
    TransformComponent original;
    original.translation = {1.f, -2.f, 3.f};
    original.rotation = {0.4f, -1.1f, 0.7f};  //x, y e z ao mesmo tempo: pega troca de ordem
    original.scale = {2.f, 0.5f, 1.5f};

    TransformComponent rebuilt;
    rebuilt.setFromMatrix(original.mat4());

    checkSameMatrix(rebuilt.mat4(), original.mat4());
}

TEST_CASE("setFromMatrix devolve os mesmos valores quando não há ambiguidade") {
    TransformComponent original;
    original.translation = {5.f, 0.f, -1.f};
    original.rotation = {0.3f, 0.8f, -0.5f};
    original.scale = {1.f, 2.f, 3.f};

    TransformComponent rebuilt;
    rebuilt.setFromMatrix(original.mat4());

    CHECK(rebuilt.translation.x == doctest::Approx(5.f));
    CHECK(rebuilt.translation.z == doctest::Approx(-1.f));
    CHECK(rebuilt.rotation.x == doctest::Approx(0.3f));
    CHECK(rebuilt.rotation.y == doctest::Approx(0.8f));
    CHECK(rebuilt.rotation.z == doctest::Approx(-0.5f));
    CHECK(rebuilt.scale.y == doctest::Approx(2.f));
    CHECK(rebuilt.scale.z == doctest::Approx(3.f));
}

//para onde um eixo do objeto aponta no mundo (coluna da matriz de rotação)
static glm::vec3 axis(const TransformComponent &transform, int column) {
    return transform.rotationMatrix()[column];
}

static void checkSameVec(glm::vec3 a, glm::vec3 b) {
    CHECK(a.x == doctest::Approx(b.x).epsilon(1e-4));
    CHECK(a.y == doctest::Approx(b.y).epsilon(1e-4));
    CHECK(a.z == doctest::Approx(b.z).epsilon(1e-4));
}

TEST_CASE("applyRotation: girar em volta do próprio eixo (local) e do mundo dá resultados diferentes") {
    const float quarter = glm::radians(90.f);
    //os dois começam virados 90 graus em Y: o X do objeto aponta pro -Z do mundo
    TransformComponent local{};
    local.rotation.y = quarter;
    TransformComponent world = local;
    checkSameVec(axis(local, 0), {0.f, 0.f, -1.f});

    local.applyRotation({quarter, 0.f, 0.f}, true);
    world.applyRotation({quarter, 0.f, 0.f}, false);

    //local: gira em volta do X do objeto (o -Z do mundo): o X dele não sai do lugar
    checkSameVec(axis(local, 0), {0.f, 0.f, -1.f});
    //mundo: gira em volta do X do mundo: o X do objeto, que estava em -Z, vai pra +Y
    checkSameVec(axis(world, 0), {0.f, 1.f, 0.f});
}

TEST_CASE("applyRotation: mantém posição e escala, e soma com a rotação que já tinha") {
    TransformComponent transform{};
    transform.translation = {1.f, 2.f, 3.f};
    transform.scale = {2.f, 1.f, 0.5f};
    transform.rotation.y = glm::radians(30.f);

    transform.applyRotation({0.f, glm::radians(15.f), 0.f});

    CHECK(transform.rotation.y == doctest::Approx(glm::radians(45.f)));
    CHECK(transform.translation.z == doctest::Approx(3.f));
    CHECK(transform.scale.x == doctest::Approx(2.f));
    CHECK(transform.scale.z == doctest::Approx(0.5f));
}

TEST_CASE("applyRotation no mundo de um filho: usa os eixos do mundo, não os do pai") {
    const float quarter = glm::radians(90.f);
    //pai virado 90 graus em Y; o filho sem rotação própria
    TransformComponent parent{};
    parent.rotation.y = quarter;
    TransformComponent child{};

    child.applyRotation({quarter, 0.f, 0.f}, false, parent.rotationMatrix());

    //no mundo, o filho = pai x filho: o X dele (que era o -Z do mundo) foi girado em volta do X do mundo
    glm::vec3 childXInWorld = parent.rotationMatrix() * child.rotationMatrix()[0];
    checkSameVec(childXInWorld, {0.f, 1.f, 0.f});
}
