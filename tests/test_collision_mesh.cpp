#include "engine/physics/crs_physics_system.hpp"
#include "engine/rendering/crs_gltf_export.hpp"

#include <doctest/doctest.h>

#include <glm/gtc/constants.hpp>

#include <cmath>
#include <filesystem>
#include <memory>

using namespace crsdk;

namespace {
    constexpr float STEP = 1.f / 60.f;

    void simulate(PhysicsSystem &physics, CrsScene &scene, float seconds) {
        for (int i = 0; i < static_cast<int>(std::lround(seconds / STEP)); i++) physics.update(scene, STEP);
    }

    //um quadrado deitado (chão), de -size a +size em x e z, na altura y
    ExportMesh floorQuad(const std::string &name, float size, float y) {
        ExportMesh mesh{name, {{-size, y, -size}, {size, y, -size}, {size, y, size}, {-size, y, size}}, {}, {0, 2, 1, 0, 3, 2}};
        mesh.normals.assign(4, glm::vec3{0.f, 1.f, 0.f});
        return mesh;
    }

    //um "L" visto de cima, feito de dois degraus em alturas diferentes (forma côncava como malha só):
    //x de 0 a 4 e z de 0 a 2 na altura 1; x de 0 a 2 e z de 2 a 4 na altura 3
    std::shared_ptr<const CollisionMesh> stepsMesh() {
        CollisionMesh mesh;
        mesh.vertices = {{0, 1, 0}, {4, 1, 0}, {4, 1, 2}, {0, 1, 2},
                         {0, 3, 2}, {2, 3, 2}, {2, 3, 4}, {0, 3, 4}};
        mesh.indices = {0, 2, 1, 0, 3, 2, 4, 6, 5, 4, 7, 6};
        mesh.buildEdges();
        return std::make_shared<const CollisionMesh>(mesh);
    }

    CrsGameObject &addBall(CrsScene &scene, glm::vec3 position) {
        CrsGameObject &ball = scene.createGameObject();
        ball.transform.translation = position;
        ball.rigidBody.emplace();
        ball.rigidBody->shape = ColliderShape::Sphere;
        ball.rigidBody->radius = 0.25f;
        return ball;
    }
}

TEST_CASE("colisão do modelo: o nome UCX_ ou COL_ marca a malha (sem diferença de maiúscula)") {
    CHECK(isCollisionMeshName("UCX_parede"));
    CHECK(isCollisionMeshName("ucx_escada_01"));
    CHECK(isCollisionMeshName("COL_casa"));
    CHECK_FALSE(isCollisionMeshName("parede_UCX_"));
    CHECK_FALSE(isCollisionMeshName("UCX"));
    CHECK_FALSE(isCollisionMeshName("Cube"));
}

TEST_CASE("colisão do modelo: o contorno tem as bordas e as dobras, sem a diagonal da face plana") {
    CollisionMesh flat;
    flat.vertices = {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}};
    flat.indices = {0, 1, 2, 0, 2, 3};  //um quadrado plano: as 4 bordas, a diagonal não
    flat.buildEdges();
    CHECK(flat.edges.size() == 8);

    //dobrado no meio (um telhado): a dobra entra; e vértice repetido na mesma posição é soldado
    CollisionMesh roof;
    roof.vertices = {{0, 0, 0}, {1, 1, 0}, {1, 1, 1}, {1, 1, 0}, {2, 0, 0}, {1, 1, 1}};
    roof.indices = {0, 1, 2, 3, 4, 5};
    roof.buildEdges();
    CHECK(roof.edges.size() == 10);  //2 triângulos soltos de 3 arestas, com uma em comum (a cumeeira)
}

TEST_CASE("glb: gravar e ler de volta; a malha UCX_ vira colisão e não é desenhada") {
    const auto path = std::filesystem::temp_directory_path() / "crs_test_ucx.glb";
    writeGlb(path.string(), {floorQuad("chao", 2.f, 0.f), floorQuad("UCX_chao", 3.f, 0.5f)});

    CrsModel::Builder builder;
    builder.loadModel(path.string());
    //a malha visível: só o chão de 2 m
    REQUIRE(builder.vertices.size() == 4);
    CHECK(builder.indices.size() == 6);
    for (const auto &v : builder.vertices) {
        CHECK(std::abs(v.position.x) == doctest::Approx(2.f));
        CHECK(v.normal.y == doctest::Approx(1.f));
    }
    //a colisão: o de 3 m, meio metro acima
    REQUIRE(builder.collision.vertices.size() == 4);
    CHECK(builder.collision.indices.size() == 6);
    CHECK(builder.collision.vertices[0].y == doctest::Approx(0.5f));
    CHECK(builder.collision.edges.size() == 8);  //o quadrado: as 4 bordas
    std::filesystem::remove(path);
}

TEST_CASE("glb: arquivo só com UCX_ não tem nada pra desenhar, só colisão") {
    const auto path = std::filesystem::temp_directory_path() / "crs_test_so_colisao.glb";
    writeGlb(path.string(), {floorQuad("UCX_a", 1.f, 0.f), floorQuad("COL_b", 1.f, 2.f)});
    CrsModel::Builder builder;
    builder.loadModel(path.string());
    CHECK(builder.vertices.empty());
    CHECK(builder.collision.vertices.size() == 8);  //as duas juntas
    CHECK(builder.collision.indices.size() == 12);
    std::filesystem::remove(path);
}

TEST_CASE("exportar grupo: as peças saem com o pai na origem; gizmo de fora; colisão vira UCX_") {
    CrsScene scene;
    CrsGameObject &group = scene.createGameObject();  //o Empty do grupo, longe e girado
    group.name = "Casa";
    group.editorOnly = true;
    group.transform.translation = {100.f, 5.f, 50.f};
    group.transform.rotation = {0.f, glm::half_pi<float>(), 0.f};
    CrsGameObject &wall = scene.createGameObject();
    wall.name = "parede";
    wall.transform.translation = {101.f, 5.f, 50.f};  //1 m pro +X do mundo
    REQUIRE(scene.setParent(wall.getId(), group.getId()));

    CrsModel::Builder piece;  //a geometria da peça: um ponto na origem dela, mais dois pra formar um triângulo
    piece.vertices = {{}, {}, {}};
    piece.vertices[1].position = {0.f, 1.f, 0.f};
    piece.vertices[2].position = {0.f, 0.f, 1.f};
    piece.indices = {0, 1, 2};
    piece.collision.vertices = {{0, 0, 0}, {0, 2, 0}, {0, 0, 2}};
    piece.collision.indices = {0, 1, 2};
    CrsModel::Builder gizmo = piece;
    auto geometryOf = [&](const CrsGameObject &obj) -> const CrsModel::Builder * {
        return obj.editorOnly ? &gizmo : &piece;
    };

    const std::vector<ExportMesh> meshes = collectGroupMeshes(scene, group.getId(), geometryOf);
    REQUIRE(meshes.size() == 2);  //a peça e a colisão dela; o gizmo do Empty não
    CHECK(meshes[0].name == "parede");
    CHECK(meshes[1].name == "UCX_parede");
    //a parede está 1 m pro +X do mundo; o grupo girou 90 graus: no espaço do grupo isso é o -Z
    const glm::vec3 origin = meshes[0].positions[0];
    CHECK(origin.x == doctest::Approx(0.f).epsilon(1e-4));
    CHECK(origin.y == doctest::Approx(0.f).epsilon(1e-4));
    CHECK(std::abs(origin.z) == doctest::Approx(1.f).epsilon(1e-4));
    CHECK(meshes[1].positions[1].y == doctest::Approx(2.f).epsilon(1e-4));
}

TEST_CASE("física: colisão do modelo é estática com a forma exata, mesmo côncava") {
    CrsScene scene;
    PhysicsSystem physics;
    CrsGameObject &steps = scene.createGameObject();
    steps.collisionMesh = stepsMesh();

    CrsGameObject &low = addBall(scene, {3.f, 6.f, 1.f});     //em cima do degrau baixo (altura 1)
    CrsGameObject &high = addBall(scene, {1.f, 6.f, 3.f});    //em cima do degrau alto (altura 3)
    CrsGameObject &gap = addBall(scene, {3.f, 6.f, 3.f});     //no "dente" do L: não tem chão ali
    simulate(physics, scene, 2.f);

    CHECK(low.transform.translation.y == doctest::Approx(1.25f).epsilon(0.03));
    CHECK(high.transform.translation.y == doctest::Approx(3.25f).epsilon(0.03));
    CHECK(gap.transform.translation.y < 0.f);  //côncavo de verdade: o vão continua vão
}

TEST_CASE("física: colisão do modelo segue o grupo (posição, giro e escala)") {
    CrsScene scene;
    PhysicsSystem physics;
    CrsGameObject &group = scene.createGameObject();
    group.transform.translation = {50.f, 0.f, 0.f};
    group.transform.scale = {2.f, 2.f, 2.f};
    CrsGameObject &steps = scene.createGameObject();
    steps.parentId = group.getId();
    steps.collisionMesh = stepsMesh();
    simulate(physics, scene, STEP);  //o corpo nasce

    //o grupo anda depois: a colisão vai junto. Com escala 2, o degrau baixo fica na altura 2
    group.transform.translation.z = 10.f;
    CrsGameObject &ball = addBall(scene, {50.f + 6.f, 8.f, 10.f + 2.f});
    simulate(physics, scene, 2.f);
    CHECK(ball.transform.translation.y == doctest::Approx(2.25f).epsilon(0.03));
}

TEST_CASE("física: com o card Física no objeto, ele manda e a colisão do modelo sai") {
    CrsScene scene;
    PhysicsSystem physics;
    CrsGameObject &steps = scene.createGameObject();
    steps.collisionMesh = stepsMesh();
    simulate(physics, scene, STEP);

    //o card Física: uma esfera pequena lá embaixo, longe dos degraus
    steps.rigidBody.emplace();
    steps.rigidBody->type = BodyType::Static;
    steps.rigidBody->shape = ColliderShape::Sphere;
    steps.rigidBody->radius = 0.1f;
    steps.rigidBody->center = {0.f, -50.f, 0.f};
    CrsGameObject &ball = addBall(scene, {3.f, 6.f, 1.f});
    simulate(physics, scene, 2.f);
    CHECK(ball.transform.translation.y < 0.f);  //o degrau sumiu: caiu
}
