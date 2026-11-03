#include "engine/scene/crs_scene.hpp"

#include <doctest/doctest.h>

#include <glm/gtc/constants.hpp>

using namespace crsdk;

//posição no mundo de um objeto (a coluna 3 da matriz é a translação)
static glm::vec3 worldPosition(CrsScene &scene, const CrsGameObject &obj) {
    return glm::vec3{scene.worldMatrix(obj)[3]};
}

TEST_CASE("objeto sem pai: a matriz do mundo é a do próprio transform") {
    CrsScene scene;
    CrsGameObject &obj = scene.createGameObject();
    obj.transform.translation = {1.f, 2.f, 3.f};

    glm::vec3 p = worldPosition(scene, obj);
    CHECK(p.x == doctest::Approx(1.f));
    CHECK(p.y == doctest::Approx(2.f));
    CHECK(p.z == doctest::Approx(3.f));
}

TEST_CASE("filho acompanha o pai") {
    CrsScene scene;
    CrsGameObject &parent = scene.createGameObject();
    parent.transform.translation = {5.f, 0.f, 0.f};
    CrsGameObject &child = scene.createGameObject();
    child.parentId = parent.getId();
    child.transform.translation = {1.f, 0.f, 0.f};  //1 à direita do pai

    CHECK(worldPosition(scene, child).x == doctest::Approx(6.f));

    parent.transform.translation = {10.f, 0.f, 0.f};
    CHECK(worldPosition(scene, child).x == doctest::Approx(11.f));
}

TEST_CASE("filho gira em volta do pai (ordem pai x filho)") {
    CrsScene scene;
    CrsGameObject &parent = scene.createGameObject();
    parent.transform.translation = {5.f, 0.f, 0.f};
    parent.transform.rotation.y = glm::half_pi<float>();  //90 graus em Y
    CrsGameObject &child = scene.createGameObject();
    child.parentId = parent.getId();
    child.transform.translation = {1.f, 0.f, 0.f};

    //o "1 à direita" do pai, girado 90 graus, vira 1 em Z (a ordem errada daria x = 6)
    glm::vec3 p = worldPosition(scene, child);
    CHECK(p.x == doctest::Approx(5.f));
    CHECK(std::abs(p.z) == doctest::Approx(1.f));
}

TEST_CASE("neto soma pai e avô") {
    CrsScene scene;
    CrsGameObject &grandparent = scene.createGameObject();
    grandparent.transform.translation = {1.f, 0.f, 0.f};
    CrsGameObject &parent = scene.createGameObject();
    parent.parentId = grandparent.getId();
    parent.transform.translation = {0.f, 2.f, 0.f};
    CrsGameObject &child = scene.createGameObject();
    child.parentId = parent.getId();
    child.transform.translation = {0.f, 0.f, 3.f};

    glm::vec3 p = worldPosition(scene, child);
    CHECK(p.x == doctest::Approx(1.f));
    CHECK(p.y == doctest::Approx(2.f));
    CHECK(p.z == doctest::Approx(3.f));
}

//uma parede: caixa de colisão estática de lado 2 (ou do tamanho pedido) numa posição
static CrsGameObject &addWall(CrsScene &scene, glm::vec3 position, glm::vec3 scale = glm::vec3{1.f}) {
    CrsGameObject &wall = scene.createGameObject();
    wall.transform.translation = position;
    wall.transform.scale = scale;
    wall.rigidBody.emplace();
    wall.rigidBody->type = BodyType::Static;
    return wall;
}

TEST_CASE("raycast: acerta o colisor mais perto e diz a distância") {
    CrsScene scene;
    CrsGameObject &near = addWall(scene, {0.f, 0.f, 5.f});
    addWall(scene, {0.f, 0.f, 10.f});
    Ray ray{{0.f, 0.f, 0.f}, {0.f, 0.f, 1.f}};

    auto hit = scene.raycast(ray, 100.f);

    REQUIRE(hit.has_value());
    CHECK(hit->id == near.getId());
    CHECK(hit->distance == doctest::Approx(4.f));  //face da frente em z = 5 - 1
}

TEST_CASE("raycast: o que está depois da distância máxima não conta") {
    CrsScene scene;
    addWall(scene, {0.f, 0.f, 5.f});
    Ray ray{{0.f, 0.f, 0.f}, {0.f, 0.f, 1.f}};

    CHECK_FALSE(scene.raycast(ray, 3.f).has_value());
}

TEST_CASE("raycast: o objeto ignorado fica de fora (o raio sai de dentro dele)") {
    CrsScene scene;
    //o raio começa DENTRO desta caixa, como o pivô da câmera dentro da cápsula do personagem
    CrsGameObject &capsule = addWall(scene, {0.f, 0.f, 0.f});
    CrsGameObject &wall = addWall(scene, {0.f, 0.f, 5.f});
    Ray ray{{0.f, 0.f, 0.f}, {0.f, 0.f, 1.f}};

    auto withoutIgnore = scene.raycast(ray, 100.f);
    REQUIRE(withoutIgnore.has_value());
    CHECK(withoutIgnore->id == capsule.getId());  //sem ignorar, bate na própria cápsula logo no começo

    auto withIgnore = scene.raycast(ray, 100.f, capsule.getId());
    REQUIRE(withIgnore.has_value());
    CHECK(withIgnore->id == wall.getId());
}

TEST_CASE("raycast: usa o centro e a escala do colisor") {
    CrsScene scene;
    //caixa com a origem nos pés (centro 1 acima) e o dobro de largura em X
    CrsGameObject &body = addWall(scene, {0.f, 0.f, 0.f}, {2.f, 1.f, 1.f});
    body.rigidBody->center = {0.f, 1.f, 0.f};

    //raio de lado, na altura do centro da caixa (y = 1): acerta a face em x = -2
    Ray ray{{-10.f, 1.f, 0.f}, {1.f, 0.f, 0.f}};
    auto hit = scene.raycast(ray, 100.f);
    REQUIRE(hit.has_value());
    CHECK(hit->distance == doctest::Approx(8.f));

    //um pouco abaixo dos pés (y = 0.5) o raio passa por baixo da caixa
    Ray below{{-10.f, -0.5f, 0.f}, {1.f, 0.f, 0.f}};
    CHECK_FALSE(scene.raycast(below, 100.f).has_value());
}

TEST_CASE("ids: cada cena começa do zero") {
    CrsScene first;
    first.createGameObject();
    first.createGameObject();

    CrsScene second;
    CHECK(second.createGameObject().getId() == 0);
}

TEST_CASE("ids: criar com id escolhido, os automáticos continuam depois dele") {
    CrsScene scene;
    scene.createGameObject(10);

    CHECK(scene.createGameObject().getId() == 11);
}

TEST_CASE("ids: id repetido é erro") {
    CrsScene scene;
    scene.createGameObject(3);

    CHECK_THROWS(scene.createGameObject(3));
}


TEST_CASE("apagar um objeto apaga os filhos dele e solta a câmera ativa") {
    CrsScene scene;
    CrsGameObject &player = scene.createGameObject();
    CrsGameObject &pivot = scene.createGameObject();
    pivot.parentId = player.getId();
    CrsGameObject &camera = scene.createGameObject();
    camera.parentId = pivot.getId();
    CrsGameObject &other = scene.createGameObject();
    const auto playerId = player.getId(), pivotId = pivot.getId(), cameraId = camera.getId(), otherId = other.getId();
    scene.setActiveCamera(cameraId);

    scene.destroyGameObject(playerId);

    CHECK(scene.findGameObjectById(playerId) == nullptr);
    CHECK(scene.findGameObjectById(pivotId) == nullptr);
    CHECK(scene.findGameObjectById(cameraId) == nullptr);
    CHECK(scene.findGameObjectById(otherId) != nullptr);
    CHECK_FALSE(scene.getActiveCamera().has_value());
}

TEST_CASE("duplicar copia o objeto e os filhos, com ids novos e o pai certo") {
    CrsScene scene;
    CrsGameObject &player = scene.createGameObject();
    player.name = "Player";
    player.transform.translation = {3.f, 0.f, 1.f};
    player.rigidBody.emplace();
    player.scripts.push_back({"controle", "Controle"});
    CrsGameObject &pivot = scene.createGameObject();
    pivot.name = "Pivo";
    pivot.parentId = player.getId();
    CrsGameObject &camera = scene.createGameObject();
    camera.parentId = pivot.getId();
    const auto playerId = player.getId(), pivotId = pivot.getId();

    auto copyId = scene.duplicateGameObject(playerId);

    REQUIRE(copyId.has_value());
    CHECK(scene.getGameObjects().size() == 6);
    CrsGameObject &copy = *scene.findGameObjectById(*copyId);
    CHECK(copy.name == "Player.001");
    CHECK(copy.transform.translation.x == doctest::Approx(3.f));  //mesmo lugar
    CHECK(copy.rigidBody.has_value());
    REQUIRE(copy.scripts.size() == 1);
    CHECK_FALSE(copy.parentId.has_value());  //o original não tinha pai

    //o filho da cópia é uma cópia do pivô, e o neto é filho dele (não do pivô original)
    const CrsGameObject *pivotCopy = nullptr;
    for (auto &kv : scene.getGameObjects()) {
        if (kv.second.parentId == *copyId) pivotCopy = &kv.second;
    }
    REQUIRE(pivotCopy != nullptr);
    CHECK(pivotCopy->name == "Pivo.001");
    CHECK(pivotCopy->getId() != pivotId);
    int grandchildren = 0;
    for (auto &kv : scene.getGameObjects()) {
        if (kv.second.parentId == pivotCopy->getId()) grandchildren++;
    }
    CHECK(grandchildren == 1);
}

TEST_CASE("duplicar de novo continua a numeração, e a cópia de uma cópia não vira cube.001.001") {
    CrsScene scene;
    CrsGameObject &cube = scene.createGameObject();
    cube.name = "cube";
    const auto id = cube.getId();
    auto first = scene.duplicateGameObject(id);
    auto second = scene.duplicateGameObject(id);
    auto ofCopy = scene.duplicateGameObject(*first);
    CHECK(scene.findGameObjectById(*first)->name == "cube.001");
    CHECK(scene.findGameObjectById(*second)->name == "cube.002");
    CHECK(scene.findGameObjectById(*ofCopy)->name == "cube.003");
    CHECK_FALSE(scene.duplicateGameObject(9999).has_value());
}

TEST_CASE("duplicar um terreno dá alturas próprias pra cópia") {
    CrsScene scene;
    CrsGameObject &ground = scene.createGameObject();
    ground.terrain.emplace();
    auto copyId = scene.duplicateGameObject(ground.getId());
    CrsGameObject &copy = *scene.findGameObjectById(*copyId);
    copy.terrain->data->raise({50.f, 50.f}, 6.f, 3.f);
    CHECK(scene.findGameObjectById(ground.getId())->terrain->data->sample(50, 50) == doctest::Approx(0.f));
    CHECK(copy.terrain->data->sample(50, 50) == doctest::Approx(3.f));
}

TEST_CASE("cena: worldBounds envolve a forma do objeto e a dos filhos, no mundo") {
    CrsScene scene;
    CrsGameObject &parent = scene.createGameObject();
    parent.transform.translation = {10.f, 0.f, 0.f};
    parent.rigidBody.emplace();
    parent.rigidBody->halfExtents = {1.f, 1.f, 1.f};
    CrsGameObject &child = scene.createGameObject();
    child.parentId = parent.getId();
    child.transform.translation = {0.f, 5.f, 0.f};  //5 m acima do pai
    child.rigidBody.emplace();
    child.rigidBody->halfExtents = {0.5f, 0.5f, 0.5f};

    auto bounds = scene.worldBounds(parent.getId());
    REQUIRE(bounds.has_value());
    CHECK(bounds->min.x == doctest::Approx(9.f));
    CHECK(bounds->max.x == doctest::Approx(11.f));
    CHECK(bounds->min.y == doctest::Approx(-1.f));
    CHECK(bounds->max.y == doctest::Approx(5.5f));  //o filho conta

    //só o filho: a caixa dele, já no lugar do mundo
    auto childBounds = scene.worldBounds(child.getId());
    REQUIRE(childBounds.has_value());
    CHECK(childBounds->center().x == doctest::Approx(10.f));
    CHECK(childBounds->center().y == doctest::Approx(5.f));
    CHECK(scene.worldBounds(9999) == std::nullopt);
}

TEST_CASE("cena: worldBounds de objeto vazio é um ponto; do terreno, ele inteiro com o relevo") {
    CrsScene scene;
    CrsGameObject &empty = scene.createGameObject();
    empty.transform.translation = {1.f, 2.f, 3.f};
    auto point = scene.worldBounds(empty.getId());
    REQUIRE(point.has_value());
    CHECK(point->size().x == doctest::Approx(0.f));
    CHECK(point->center().z == doctest::Approx(3.f));

    CrsGameObject &ground = scene.createGameObject();
    ground.terrain.emplace();
    ground.terrain->data->raise({128.f, 128.f}, 10.f, 6.f);  //morro de 6 m no meio
    auto terrain = scene.worldBounds(ground.getId());
    REQUIRE(terrain.has_value());
    CHECK(terrain->min.x == doctest::Approx(-128.f));  //o objeto fica no meio do terreno
    CHECK(terrain->max.z == doctest::Approx(128.f));
    CHECK(terrain->max.y == doctest::Approx(6.f));
    CHECK(terrain->min.y == doctest::Approx(0.f));
}

TEST_CASE("cena: setParent troca o pai sem o objeto sair do lugar (posição, rotação e escala)") {
    CrsScene scene;
    CrsGameObject &collider = scene.createGameObject();  //o pai: um collider girado e escalado
    collider.transform.translation = {10.f, 0.f, 5.f};
    collider.transform.rotation = {0.f, glm::half_pi<float>(), 0.f};
    collider.transform.scale = {2.f, 2.f, 2.f};
    CrsGameObject &wall = scene.createGameObject();
    wall.transform.translation = {12.f, 1.f, 5.f};
    wall.transform.rotation = {0.f, 0.3f, 0.f};
    const glm::mat4 before = scene.worldMatrix(wall);

    REQUIRE(scene.setParent(wall.getId(), collider.getId()));
    CHECK(wall.parentId == collider.getId());
    const glm::mat4 after = scene.worldMatrix(wall);
    for (int c = 0; c < 4; c++) {
        for (int r = 0; r < 4; r++) CHECK(after[c][r] == doctest::Approx(before[c][r]).epsilon(1e-4));
    }

    //o pai anda e gira: o filho vai junto
    collider.transform.translation.x += 3.f;
    CHECK(worldPosition(scene, wall).x != doctest::Approx(before[3].x));

    //tirar do pai: continua onde estava (agora com o pai mexido)
    const glm::vec3 moved = worldPosition(scene, wall);
    REQUIRE(scene.setParent(wall.getId(), std::nullopt));
    CHECK_FALSE(wall.parentId.has_value());
    CHECK(wall.transform.translation.x == doctest::Approx(moved.x).epsilon(1e-4));
    CHECK(wall.transform.translation.z == doctest::Approx(moved.z).epsilon(1e-4));
}

TEST_CASE("cena: setParent recusa círculo, o próprio objeto, terreno e o que não existe") {
    CrsScene scene;
    CrsGameObject &a = scene.createGameObject();
    CrsGameObject &b = scene.createGameObject();
    CrsGameObject &c = scene.createGameObject();
    REQUIRE(scene.setParent(b.getId(), a.getId()));
    REQUIRE(scene.setParent(c.getId(), b.getId()));

    CHECK_FALSE(scene.setParent(a.getId(), c.getId()));  //a seria neto de si mesmo
    CHECK_FALSE(scene.setParent(a.getId(), a.getId()));
    CHECK_FALSE(scene.setParent(b.getId(), a.getId()));  //já é o pai
    CHECK_FALSE(scene.setParent(9999, a.getId()));
    CHECK_FALSE(scene.setParent(a.getId(), 9999));
    CrsGameObject &ground = scene.createGameObject();
    ground.terrain.emplace();
    CHECK_FALSE(scene.setParent(ground.getId(), a.getId()));
    CHECK_FALSE(a.parentId.has_value());
}
