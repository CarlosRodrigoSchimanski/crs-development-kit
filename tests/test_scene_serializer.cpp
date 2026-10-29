#include "engine/scene/scene_serializer.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace lve;

//arquivo na pasta temporária do sistema (não suja o projeto)
static std::string tempScenePath(const std::string &name) {
    return (std::filesystem::temp_directory_path() / name).string();
}

//sem GPU nos testes: nenhum modelo é carregado
static std::shared_ptr<LveModel> noModel(const std::string &) {
    return nullptr;
}

TEST_CASE("cena: salvar e carregar devolve a mesma cena") {
    LveScene original;
    LveGameObject &pivot = original.createGameObject();
    pivot.name = "CameraPivot";
    pivot.transform.translation = {1.f, 1.6f, -2.f};
    pivot.transform.rotation = {0.3f, -1.2f, 0.f};
    pivot.transform.scale = {1.f, 2.f, 3.f};

    //id com buraco: o carregar tem que manter o 10, não renumerar
    LveGameObject &camera = original.createGameObject(10);
    camera.name = "Camera";
    camera.parentId = pivot.getId();
    camera.editorOnly = true;
    original.setActiveCamera(camera.getId());

    std::string path = tempScenePath("ge_test_ida_e_volta.json");
    saveScene(original, path);
    LveScene loaded = loadScene(path, noModel);

    REQUIRE(loaded.getGameObjects().size() == 2);
    LveGameObject *loadedPivot = loaded.findGameObjectById(pivot.getId());
    LveGameObject *loadedCamera = loaded.findGameObjectById(10);
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
    CHECK(loaded.getActiveCamera() == std::optional<LveGameObject::id_t>{10});

    //objeto novo depois de carregar não pode repetir um id do arquivo
    CHECK(loaded.createGameObject().getId() == 11);

    std::filesystem::remove(path);
}

TEST_CASE("cena: o carregar pede cada modelo pelo caminho gravado") {
    std::string path = tempScenePath("ge_test_modelo.json");
    {
        std::ofstream out{path};
        out << R"({
            "version": 1,
            "objects": [
                {"id": 0, "name": "cube", "model": "models/iron_cube.glb",
                 "transform": {"translation": [0, 0, 0], "rotation": [0, 0, 0], "scale": [1, 1, 1]}}
            ]
        })";
    }

    std::vector<std::string> requested;
    loadScene(path, [&](const std::string &modelPath) {
        requested.push_back(modelPath);
        return std::shared_ptr<LveModel>{};
    });

    REQUIRE(requested.size() == 1);
    CHECK(requested[0] == "models/iron_cube.glb");

    std::filesystem::remove(path);
}

TEST_CASE("cena: arquivo que não existe é erro") {
    CHECK_THROWS(loadScene(tempScenePath("ge_nao_existe.json"), noModel));
}
