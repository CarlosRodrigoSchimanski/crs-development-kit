#include "engine/scene/scene_serializer.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace crsdk;

//arquivo na pasta temporária do sistema (não suja o projeto)
static std::string tempScenePath(const std::string &name) {
    return (std::filesystem::temp_directory_path() / name).string();
}

//sem GPU nos testes: nenhum modelo é carregado
static std::shared_ptr<CrsModel> noModel(const std::string &) {
    return nullptr;
}

TEST_CASE("cena: salvar e carregar devolve a mesma cena") {
    CrsScene original;
    CrsGameObject &pivot = original.createGameObject();
    pivot.name = "CameraPivot";
    pivot.transform.translation = {1.f, 1.6f, -2.f};
    pivot.transform.rotation = {0.3f, -1.2f, 0.f};
    pivot.transform.scale = {1.f, 2.f, 3.f};

    //id com buraco: o carregar tem que manter o 10, não renumerar
    CrsGameObject &camera = original.createGameObject(10);
    camera.name = "Camera";
    camera.parentId = pivot.getId();
    camera.editorOnly = true;
    original.setActiveCamera(camera.getId());

    std::string path = tempScenePath("ge_test_ida_e_volta.json");
    saveScene(original, path);
    CrsScene loaded = loadScene(path, noModel);

    REQUIRE(loaded.getGameObjects().size() == 2);
    CrsGameObject *loadedPivot = loaded.findGameObjectById(pivot.getId());
    CrsGameObject *loadedCamera = loaded.findGameObjectById(10);
    REQUIRE(loadedPivot != nullptr);
    REQUIRE(loadedCamera != nullptr);

    CHECK(loadedPivot->name == "CameraPivot");
    CHECK(loadedPivot->transform.translation.y == doctest::Approx(1.6f));
    CHECK(loadedPivot->transform.translation.z == doctest::Approx(-2.f));
    CHECK(loadedPivot->transform.rotation.y == doctest::Approx(-1.2f));
    CHECK(loadedPivot->transform.scale.z == doctest::Approx(3.f));
    CHECK_FALSE(loadedPivot->parentId.has_value());
    CHECK_FALSE(loadedPivot->editorOnly);

    CHECK(loadedCamera->name == "Camera");
    REQUIRE(loadedCamera->parentId.has_value());
    CHECK(*loadedCamera->parentId == pivot.getId());
    CHECK(loadedCamera->editorOnly);
    CHECK(loaded.getActiveCamera() == std::optional<CrsGameObject::id_t>{10});

    //objeto novo depois de carregar não pode repetir um id do arquivo
    CHECK(loaded.createGameObject().getId() == 11);

    std::filesystem::remove(path);
}

TEST_CASE("cena: o carregar pede cada modelo pelo caminho gravado") {
    std::string path = tempScenePath("ge_test_modelo.json");
    {
        std::ofstream out{path};
        out << R"({
            "version": 2,
            "objects": [
                {"id": 0, "name": "cube", "model": "assets/models/iron_cube.glb",
                 "transform": {"translation": [0, 0, 0], "rotation": [0, 0, 0], "scale": [1, 1, 1]}}
            ]
        })";
    }

    std::vector<std::string> requested;
    loadScene(path, [&](const std::string &modelPath) {
        requested.push_back(modelPath);
        return std::shared_ptr<CrsModel>{};
    });

    REQUIRE(requested.size() == 1);
    CHECK(requested[0] == "assets/models/iron_cube.glb");

    std::filesystem::remove(path);
}

TEST_CASE("cena: os componentes voltam com as configurações, o estado do momento recomeça") {
    CrsScene original;
    CrsGameObject &player = original.createGameObject();
    player.color = {0.2f, 0.4f, 0.6f};
    player.useTexture = false;
    player.rigidBody.emplace();
    player.rigidBody->type = BodyType::Static;
    player.rigidBody->halfExtents = {0.5f, 0.87f, 0.49f};
    player.rigidBody->center = {0.f, 0.87f, 0.f};
    player.rigidBody->velocity = {1.f, 2.f, 3.f};  //estado: não vai pro arquivo
    player.character.emplace();
    player.character->moveSpeed = 6.f;
    player.character->jumpSpeed = 7.f;
    player.character->runSpeed = 8.f;

    CrsGameObject &rig = original.createGameObject();
    rig.cameraRig.emplace();
    rig.cameraRig->sensitivity = 0.01f;
    rig.cameraRig->minPitch = -1.f;
    rig.cameraRig->maxPitch = 0.5f;
    rig.cameraRig->distance = 6.f;
    rig.cameraRig->wallMargin = 0.3f;
    rig.camera.emplace();
    rig.camera->fovY = 1.1f;
    rig.camera->nearPlane = 0.05f;
    rig.camera->farPlane = 500.f;
    rig.pointLight.emplace();
    rig.pointLight->lightIntensity = 3.f;

    CrsGameObject &visual = original.createGameObject();
    visual.faceMovement.emplace();
    visual.faceMovement->turnSpeed = 9.f;
    visual.faceMovement->yawOffset = 0.25f;
    visual.animator.emplace();
    visual.animator->clip = "idle";
    visual.animator->animations = {"assets/animations/idle_01.glb", "assets/animations/walking_01.glb"};
    visual.animator->speed = 1.5f;
    visual.animator->loop = false;
    visual.animator->time = 2.f;  //estado: não vai pro arquivo
    visual.characterAnimation.emplace();
    visual.characterAnimation->idle = "idle_01";
    visual.characterAnimation->walk = "walking_01";
    visual.characterAnimation->run = "running_01";
    visual.characterAnimation->fall = "fall_01";
    visual.characterAnimation->fadeTime = 0.35f;

    std::string path = tempScenePath("ge_test_componentes.json");
    saveScene(original, path);
    CrsScene loaded = loadScene(path, noModel);
    std::filesystem::remove(path);

    CrsGameObject &p = *loaded.findGameObjectById(player.getId());
    CHECK(p.color.z == doctest::Approx(0.6f));
    CHECK_FALSE(p.useTexture);
    REQUIRE(p.rigidBody.has_value());
    CHECK(p.rigidBody->type == BodyType::Static);
    CHECK(p.rigidBody->halfExtents.y == doctest::Approx(0.87f));
    CHECK(p.rigidBody->center.y == doctest::Approx(0.87f));
    CHECK(p.rigidBody->velocity.x == doctest::Approx(0.f));
    REQUIRE(p.character.has_value());
    CHECK(p.character->moveSpeed == doctest::Approx(6.f));
    CHECK(p.character->jumpSpeed == doctest::Approx(7.f));
    CHECK(p.character->runSpeed == doctest::Approx(8.f));
    CHECK_FALSE(p.camera.has_value());

    CrsGameObject &r = *loaded.findGameObjectById(rig.getId());
    REQUIRE(r.cameraRig.has_value());
    CHECK(r.cameraRig->sensitivity == doctest::Approx(0.01f));
    CHECK(r.cameraRig->minPitch == doctest::Approx(-1.f));
    CHECK(r.cameraRig->maxPitch == doctest::Approx(0.5f));
    CHECK(r.cameraRig->distance == doctest::Approx(6.f));
    CHECK(r.cameraRig->wallMargin == doctest::Approx(0.3f));
    REQUIRE(r.camera.has_value());
    CHECK(r.camera->fovY == doctest::Approx(1.1f));
    CHECK(r.camera->nearPlane == doctest::Approx(0.05f));
    CHECK(r.camera->farPlane == doctest::Approx(500.f));
    REQUIRE(r.pointLight.has_value());
    CHECK(r.pointLight->lightIntensity == doctest::Approx(3.f));

    CrsGameObject &v = *loaded.findGameObjectById(visual.getId());
    REQUIRE(v.faceMovement.has_value());
    CHECK(v.faceMovement->turnSpeed == doctest::Approx(9.f));
    CHECK(v.faceMovement->yawOffset == doctest::Approx(0.25f));
    REQUIRE(v.animator.has_value());
    CHECK(v.animator->clip == "idle");
    REQUIRE(v.animator->animations.size() == 2);
    CHECK(v.animator->animations[1] == "assets/animations/walking_01.glb");
    CHECK(v.animator->speed == doctest::Approx(1.5f));
    CHECK_FALSE(v.animator->loop);
    CHECK(v.animator->time == doctest::Approx(0.f));
    REQUIRE(v.characterAnimation.has_value());
    CHECK(v.characterAnimation->idle == "idle_01");
    CHECK(v.characterAnimation->walk == "walking_01");
    CHECK(v.characterAnimation->run == "running_01");
    CHECK(v.characterAnimation->fall == "fall_01");
    CHECK(v.characterAnimation->fadeTime == doctest::Approx(0.35f));
}

TEST_CASE("cena: campo que falta no arquivo fica com o padrão do componente") {
    std::string path = tempScenePath("ge_test_padrao.json");
    {
        std::ofstream out{path};
        out << R"({
            "version": 2,
            "objects": [
                {"id": 0, "character": {}, "rigidBody": {"type": "static"},
                 "transform": {"translation": [0, 0, 0], "rotation": [0, 0, 0], "scale": [1, 1, 1]}}
            ]
        })";
    }
    CrsScene loaded = loadScene(path, noModel);
    std::filesystem::remove(path);

    CrsGameObject &obj = *loaded.findGameObjectById(0);
    REQUIRE(obj.character.has_value());
    CHECK(obj.character->moveSpeed == doctest::Approx(CharacterComponent{}.moveSpeed));
    REQUIRE(obj.rigidBody.has_value());
    CHECK(obj.rigidBody->type == BodyType::Static);
    CHECK(obj.rigidBody->halfExtents.x == doctest::Approx(1.f));
    //cena de antes das formas: sem "shape" é caixa
    CHECK(obj.rigidBody->shape == ColliderShape::Box);
    CHECK(obj.useTexture);
}

TEST_CASE("cena: a forma de colisão volta com tipo, tamanhos e massa") {
    CrsScene scene;
    CrsGameObject &ball = scene.createGameObject();
    ball.rigidBody.emplace();
    ball.rigidBody->shape = ColliderShape::Sphere;
    ball.rigidBody->radius = 0.25f;
    ball.rigidBody->mass = 3.f;
    CrsGameObject &player = scene.createGameObject();
    player.rigidBody.emplace();
    player.rigidBody->shape = ColliderShape::Capsule;
    player.rigidBody->radius = 0.4f;
    player.rigidBody->height = 1.74f;
    player.rigidBody->center = {0.f, 0.87f, 0.f};

    std::string path = tempScenePath("ge_test_formas.json");
    saveScene(scene, path);
    CrsScene loaded = loadScene(path, noModel);
    std::filesystem::remove(path);

    const RigidBodyComponent &loadedBall = *loaded.findGameObjectById(ball.getId())->rigidBody;
    CHECK(loadedBall.shape == ColliderShape::Sphere);
    CHECK(loadedBall.radius == doctest::Approx(0.25f));
    CHECK(loadedBall.mass == doctest::Approx(3.f));
    const RigidBodyComponent &loadedPlayer = *loaded.findGameObjectById(player.getId())->rigidBody;
    CHECK(loadedPlayer.shape == ColliderShape::Capsule);
    CHECK(loadedPlayer.height == doctest::Approx(1.74f));
    CHECK(loadedPlayer.center.y == doctest::Approx(0.87f));
}

TEST_CASE("cena: arquivo de versão antiga é erro (quem chamou decide o que fazer)") {
    std::string path = tempScenePath("ge_test_versao.json");
    {
        std::ofstream out{path};
        out << R"({"version": 1, "objects": []})";
    }
    CHECK_THROWS(loadScene(path, noModel));
    std::filesystem::remove(path);
}

TEST_CASE("cena: arquivo que não existe é erro") {
    CHECK_THROWS(loadScene(tempScenePath("ge_nao_existe.json"), noModel));
}

TEST_CASE("cena: componentes Python voltam na mesma ordem; objeto sem nenhum não grava a lista") {
    CrsScene original;
    CrsGameObject &cube = original.createGameObject();
    cube.scripts.push_back({"girar", "Girar"});
    cube.scripts.push_back({"comum.pulo", "PuloDuplo"});
    CrsGameObject &empty = original.createGameObject();
    empty.name = "SemComponente";

    std::string path = tempScenePath("ge_test_scripts.json");
    saveScene(original, path);
    CrsScene loaded = loadScene(path, noModel);

    CrsGameObject *loadedCube = loaded.findGameObjectById(cube.getId());
    REQUIRE(loadedCube != nullptr);
    REQUIRE(loadedCube->scripts.size() == 2);
    CHECK(loadedCube->scripts[0].module == "girar");
    CHECK(loadedCube->scripts[0].className == "Girar");
    CHECK(loadedCube->scripts[1].module == "comum.pulo");
    CHECK(loadedCube->scripts[1].className == "PuloDuplo");
    CHECK(loaded.findGameObjectById(empty.getId())->scripts.empty());

    //o arquivo fica limpo: "scripts" só aparece no objeto que tem componente
    std::ifstream in{path};
    std::string text{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    in.close();
    size_t first = text.find("\"scripts\"");
    CHECK(first != std::string::npos);
    CHECK(text.find("\"scripts\"", first + 1) == std::string::npos);

    std::filesystem::remove(path);
}

TEST_CASE("cena: args mudados dos componentes Python voltam com o mesmo tipo") {
    CrsScene original;
    CrsGameObject &obj = original.createGameObject();
    ScriptComponent script{"pulo", "Pulo"};
    script.args["altura"] = 2.5f;
    script.args["redondo"] = 90.f;  //float com valor inteiro continua float
    script.args["pulos"] = 3;
    script.args["duplo"] = true;
    script.args["som"] = std::string{"pulo.wav"};
    obj.scripts.push_back(script);
    obj.scripts.push_back({"girar", "Girar"});  //sem args mudados

    std::string path = tempScenePath("ge_test_script_args.json");
    saveScene(original, path);
    CrsScene loaded = loadScene(path, noModel);

    CrsGameObject *loadedObj = loaded.findGameObjectById(obj.getId());
    REQUIRE(loadedObj != nullptr);
    REQUIRE(loadedObj->scripts.size() == 2);
    const auto &args = loadedObj->scripts[0].args;
    REQUIRE(args.size() == 5);
    CHECK(std::get<float>(args.at("altura")) == doctest::Approx(2.5f));
    CHECK(std::get<float>(args.at("redondo")) == doctest::Approx(90.f));
    CHECK(std::get<int>(args.at("pulos")) == 3);
    CHECK(std::get<bool>(args.at("duplo")) == true);
    CHECK(std::get<std::string>(args.at("som")) == "pulo.wav");
    CHECK(loadedObj->scripts[1].args.empty());

    std::filesystem::remove(path);
}
