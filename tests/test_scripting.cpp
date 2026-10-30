#include "engine/scripting/crs_scripting.hpp"
#include "engine/core/crs_input.hpp"
#include "engine/scene/crs_scene.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace crsdk;

static std::string tempPath(const std::string &name) {
    return (std::filesystem::temp_directory_path() / name).generic_string();
}

static void writeText(const std::string &path, const std::string &text) {
    std::ofstream out{path};
    out << text;
}

static std::string readText(const std::string &path) {
    std::ifstream in{path};
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

TEST_CASE("Python: o script roda de verdade") {
    CrsScripting scripting;
    std::string script = tempPath("ge_teste_script.py");
    std::string result = tempPath("ge_teste_resultado.txt");
    //o script escreve um arquivo: se ele existir com o texto certo, o Python rodou
    writeText(script, "with open(r'" + result + "', 'w') as f:\n    f.write(str(6 * 7))\n");

    scripting.runFile(script);

    CHECK(readText(result) == "42");
    std::filesystem::remove(script);
    std::filesystem::remove(result);
}

TEST_CASE("Python: erro no script vira exceção com a mensagem do Python") {
    CrsScripting scripting;
    std::string script = tempPath("ge_teste_erro.py");
    writeText(script, "x = 1 / 0\n");

    CHECK_THROWS_WITH_AS(scripting.runFile(script), doctest::Contains("ZeroDivisionError"), std::runtime_error);
    std::filesystem::remove(script);
}

TEST_CASE("Python: script que não existe é erro") {
    CrsScripting scripting;
    CHECK_THROWS(scripting.runFile(tempPath("ge_nao_existe.py")));
}

//API crsdk: os asserts ficam no Python (assert falhando = exceção = teste falha com a mensagem)

TEST_CASE("API Python: crsdk.scene acha objetos e mexe no transform de verdade") {
    CrsScripting scripting;
    CrsScene scene;
    auto &player = scene.createGameObject();
    player.name = "Player";
    player.transform.translation = {1.f, 0.f, 0.f};
    auto &mesh = scene.createGameObject();
    mesh.name = "PlayerMesh";
    mesh.parentId = player.getId();
    mesh.transform.translation = {0.f, 2.f, 0.f};
    scripting.setScene(&scene);

    scripting.runString(
        "import crsdk\n"
        "player = crsdk.scene.find('Player')\n"
        "assert player.name == 'Player'\n"
        "assert crsdk.scene.find('nao existe') is None\n"
        "assert [o.name for o in crsdk.scene.objects] == ['Player', 'PlayerMesh']\n"
        "mesh = player.children[0]\n"
        "assert mesh.parent == player\n"
        "assert mesh.worldPosition == crsdk.Vec3(1, 2, 0)\n"
        "player.transform.translation.y += 3\n"
        "player.transform.rotation = crsdk.Vec3(0, 1.5, 0)\n");

    CHECK(player.transform.translation.y == doctest::Approx(3.f));
    CHECK(player.transform.rotation.y == doctest::Approx(1.5f));
}

TEST_CASE("API Python: Vec3 faz as contas de vetor") {
    CrsScripting scripting;
    CHECK_NOTHROW(scripting.runString(
        "from crsdk import Vec3\n"
        "a = Vec3(1, 2, 3)\n"
        "assert a + Vec3(1, 1, 1) == Vec3(2, 3, 4)\n"
        "assert a - a == Vec3()\n"
        "assert 2 * a == a * 2 == Vec3(2, 4, 6)\n"
        "assert -a == Vec3(-1, -2, -3)\n"
        "assert Vec3(3, 0, 4).length() == 5\n"
        "assert Vec3(0, 0, 9).normalized() == Vec3(0, 0, 1)\n"
        "assert Vec3().normalized() == Vec3()\n"
        "assert Vec3(1, 0, 0).cross(Vec3(0, 1, 0)) == Vec3(0, 0, 1)\n"
        "assert a.dot(Vec3(1, 1, 1)) == 6\n"
        "b = a.copy(); b.x = 10\n"
        "assert a.x == 1\n"));
}

TEST_CASE("API Python: criar objeto e trocar o pai (sem deixar fazer ciclo)") {
    CrsScripting scripting;
    CrsScene scene;
    scripting.setScene(&scene);

    scripting.runString(
        "import crsdk\n"
        "a = crsdk.scene.createObject('A')\n"
        "b = crsdk.scene.createObject('B')\n"
        "b.parent = a\n"
        "assert b.parentId == a.id\n"
        "try:\n"
        "    a.parent = b\n"
        "    raise AssertionError('deixou fazer ciclo')\n"
        "except ValueError:\n"
        "    pass\n"
        "b.parent = None\n"
        "assert b.parentId is None\n");

    CHECK(scene.getGameObjects().size() == 2);
}

TEST_CASE("API Python: raycast acha a caixa de colisão e respeita o ignore") {
    CrsScripting scripting;
    CrsScene scene;
    auto &wall = scene.createGameObject();
    wall.name = "Parede";
    wall.transform.translation = {0.f, 0.f, 5.f};
    wall.rigidBody.emplace();
    wall.rigidBody->halfExtents = {1.f, 1.f, 1.f};
    scripting.setScene(&scene);

    CHECK_NOTHROW(scripting.runString(
        "import crsdk\n"
        "from crsdk import Vec3\n"
        "hit = crsdk.scene.raycast(Vec3(), Vec3(0, 0, 10), 100)\n"
        "assert hit.object.name == 'Parede'\n"
        "assert abs(hit.distance - 4) < 1e-5\n"
        "assert abs(hit.point.z - 4) < 1e-5\n"
        "assert crsdk.scene.raycast(Vec3(), Vec3(0, 0, 1), 3) is None\n"
        "assert crsdk.scene.raycast(Vec3(), Vec3(0, 0, 1), 100, ignore=hit.object) is None\n"));
}

TEST_CASE("API Python: animator.play troca a animação no C++") {
    CrsScripting scripting;
    CrsScene scene;
    auto &mesh = scene.createGameObject();
    mesh.name = "Mesh";
    mesh.animator.emplace();
    mesh.animator->clip = "idle";
    scene.createGameObject().name = "SemAnimacao";
    scripting.setScene(&scene);

    scripting.runString(
        "import crsdk\n"
        "assert crsdk.scene.find('SemAnimacao').animator is None\n"
        "anim = crsdk.scene.find('Mesh').animator\n"
        "anim.play('walk', fade=0.5)\n"
        "assert anim.isBlending()\n");

    CHECK(mesh.animator->clip == "walk");
    CHECK(mesh.animator->previousClip == "idle");
    CHECK(mesh.animator->fadeDuration == doctest::Approx(0.5f));
}

TEST_CASE("API Python: input e nomes das teclas") {
    CrsScripting scripting;
    CrsInput input;
    scripting.setInput(&input);

    CHECK_NOTHROW(scripting.runString(
        "import crsdk\n"
        "assert crsdk.keys.A == 65 and crsdk.keys.SPACE == 32 and crsdk.keys.NUM0 == 48\n"
        "assert crsdk.mouse.LEFT == 0\n"
        "assert not crsdk.input.isDown(crsdk.keys.W)\n"
        "assert crsdk.input.mouseDelta == crsdk.Vec2(0, 0)\n"));
}

TEST_CASE("API Python: sem cena ligada o crsdk.scene dá erro claro") {
    CrsScripting scripting;
    CHECK_THROWS_WITH_AS(scripting.runString("import crsdk\ncrsdk.scene\n"),
        doctest::Contains("crsdk.scene"), std::runtime_error);
}
