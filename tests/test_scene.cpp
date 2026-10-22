#include "engine/scene/lve_scene.hpp"

#include <doctest/doctest.h>

#include <glm/gtc/constants.hpp>

using namespace lve;

//posição no mundo de um objeto (a coluna 3 da matriz é a translação)
static glm::vec3 worldPosition(LveScene &scene, const LveGameObject &obj) {
    return glm::vec3{scene.worldMatrix(obj)[3]};
}

TEST_CASE("objeto sem pai: a matriz do mundo é a do próprio transform") {
    LveScene scene;
    LveGameObject &obj = scene.createGameObject();
    obj.transform.translation = {1.f, 2.f, 3.f};

    glm::vec3 p = worldPosition(scene, obj);
    CHECK(p.x == doctest::Approx(1.f));
    CHECK(p.y == doctest::Approx(2.f));
    CHECK(p.z == doctest::Approx(3.f));
}

TEST_CASE("filho acompanha o pai") {
    LveScene scene;
    LveGameObject &parent = scene.createGameObject();
    parent.transform.translation = {5.f, 0.f, 0.f};
    LveGameObject &child = scene.createGameObject();
    child.parentId = parent.getId();
    child.transform.translation = {1.f, 0.f, 0.f};  //1 à direita do pai

    CHECK(worldPosition(scene, child).x == doctest::Approx(6.f));

    parent.transform.translation = {10.f, 0.f, 0.f};
    CHECK(worldPosition(scene, child).x == doctest::Approx(11.f));
}

TEST_CASE("filho gira em volta do pai (ordem pai x filho)") {
    LveScene scene;
    LveGameObject &parent = scene.createGameObject();
    parent.transform.translation = {5.f, 0.f, 0.f};
    parent.transform.rotation.y = glm::half_pi<float>();  //90 graus em Y
    LveGameObject &child = scene.createGameObject();
    child.parentId = parent.getId();
    child.transform.translation = {1.f, 0.f, 0.f};

    //o "1 à direita" do pai, girado 90 graus, vira 1 em Z (a ordem errada daria x = 6)
    glm::vec3 p = worldPosition(scene, child);
    CHECK(p.x == doctest::Approx(5.f));
    CHECK(std::abs(p.z) == doctest::Approx(1.f));
}

TEST_CASE("neto soma pai e avô") {
    LveScene scene;
    LveGameObject &grandparent = scene.createGameObject();
    grandparent.transform.translation = {1.f, 0.f, 0.f};
    LveGameObject &parent = scene.createGameObject();
    parent.parentId = grandparent.getId();
    parent.transform.translation = {0.f, 2.f, 0.f};
    LveGameObject &child = scene.createGameObject();
    child.parentId = parent.getId();
    child.transform.translation = {0.f, 0.f, 3.f};

    glm::vec3 p = worldPosition(scene, child);
    CHECK(p.x == doctest::Approx(1.f));
    CHECK(p.y == doctest::Approx(2.f));
    CHECK(p.z == doctest::Approx(3.f));
}
