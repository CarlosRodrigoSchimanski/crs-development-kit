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

//uma parede: caixa de colisão estática de lado 2 (ou do tamanho pedido) numa posição
static LveGameObject &addWall(LveScene &scene, glm::vec3 position, glm::vec3 scale = glm::vec3{1.f}) {
    LveGameObject &wall = scene.createGameObject();
    wall.transform.translation = position;
    wall.transform.scale = scale;
    wall.rigidBody.emplace();
    wall.rigidBody->type = BodyType::Static;
    return wall;
}

TEST_CASE("raycast: acerta o colisor mais perto e diz a distância") {
    LveScene scene;
    LveGameObject &near = addWall(scene, {0.f, 0.f, 5.f});
    addWall(scene, {0.f, 0.f, 10.f});
    Ray ray{{0.f, 0.f, 0.f}, {0.f, 0.f, 1.f}};

    auto hit = scene.raycast(ray, 100.f);

    REQUIRE(hit.has_value());
    CHECK(hit->id == near.getId());
    CHECK(hit->distance == doctest::Approx(4.f));  //face da frente em z = 5 - 1
}

TEST_CASE("raycast: o que está depois da distância máxima não conta") {
    LveScene scene;
    addWall(scene, {0.f, 0.f, 5.f});
    Ray ray{{0.f, 0.f, 0.f}, {0.f, 0.f, 1.f}};

    CHECK_FALSE(scene.raycast(ray, 3.f).has_value());
}

TEST_CASE("raycast: o objeto ignorado fica de fora (o raio sai de dentro dele)") {
    LveScene scene;
    //o raio começa DENTRO desta caixa, como o pivô da câmera dentro da cápsula do personagem
    LveGameObject &capsule = addWall(scene, {0.f, 0.f, 0.f});
    LveGameObject &wall = addWall(scene, {0.f, 0.f, 5.f});
    Ray ray{{0.f, 0.f, 0.f}, {0.f, 0.f, 1.f}};

    auto withoutIgnore = scene.raycast(ray, 100.f);
    REQUIRE(withoutIgnore.has_value());
    CHECK(withoutIgnore->id == capsule.getId());  //sem ignorar, bate na própria cápsula logo no começo

    auto withIgnore = scene.raycast(ray, 100.f, capsule.getId());
    REQUIRE(withIgnore.has_value());
    CHECK(withIgnore->id == wall.getId());
}

TEST_CASE("raycast: usa o centro e a escala do colisor") {
    LveScene scene;
    //caixa com a origem nos pés (centro 1 acima, o Y aponta pra baixo) e o dobro de largura em X
    LveGameObject &body = addWall(scene, {0.f, 0.f, 0.f}, {2.f, 1.f, 1.f});
    body.rigidBody->center = {0.f, -1.f, 0.f};

    //raio de lado, na altura do centro da caixa (y = -1): acerta a face em x = -2
    Ray ray{{-10.f, -1.f, 0.f}, {1.f, 0.f, 0.f}};
    auto hit = scene.raycast(ray, 100.f);
    REQUIRE(hit.has_value());
    CHECK(hit->distance == doctest::Approx(8.f));

    //um pouco abaixo dos pés (y = 0.5) o raio passa por baixo da caixa
    Ray below{{-10.f, 0.5f, 0.f}, {1.f, 0.f, 0.f}};
    CHECK_FALSE(scene.raycast(below, 100.f).has_value());
}
