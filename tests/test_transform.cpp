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
