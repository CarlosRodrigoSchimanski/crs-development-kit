#include "engine/rendering/crs_material_files.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>

using namespace crsdk;

namespace fs = std::filesystem;

//pasta temporária com arquivos vazios (o achar olha só os nomes)
static fs::path makeFolder(const std::string &name, std::initializer_list<const char *> files) {
    const fs::path folder = fs::temp_directory_path() / name;
    fs::remove_all(folder);
    fs::create_directories(folder);
    for (const char *file : files) std::ofstream{folder / file};
    return folder;
}

TEST_CASE("material: acha os mapas do Poly Haven pelo nome (e ignora o normal do DirectX)") {
    const fs::path folder = makeFolder("crs_test_material", {
        "piso_ao_2k.jpg", "piso_diff_2k.jpg", "piso_disp_2k.png",
        "piso_nor_dx_2k.jpg", "piso_nor_gl_2k.jpg", "piso_rough_2k.jpg", "leia-me.txt"});

    MaterialFiles files = findMaterialFiles(folder.string());

    CHECK(fs::path{files.albedo}.filename() == "piso_diff_2k.jpg");
    CHECK(fs::path{files.normal}.filename() == "piso_nor_gl_2k.jpg");
    CHECK(fs::path{files.roughness}.filename() == "piso_rough_2k.jpg");
    CHECK(fs::path{files.ao}.filename() == "piso_ao_2k.jpg");
    CHECK(fs::path{files.displacement}.filename() == "piso_disp_2k.png");
    fs::remove_all(folder);
}

TEST_CASE("material: outros nomes comuns (BaseColor, Normal, maiúsculas) também valem") {
    const fs::path folder = makeFolder("crs_test_material_nomes", {"Rock_BaseColor.PNG", "Rock_Normal.png", "Rock_Roughness.png"});

    MaterialFiles files = findMaterialFiles(folder.string());

    CHECK(fs::path{files.albedo}.filename() == "Rock_BaseColor.PNG");
    CHECK(fs::path{files.normal}.filename() == "Rock_Normal.png");
    CHECK(fs::path{files.roughness}.filename() == "Rock_Roughness.png");
    CHECK(files.ao.empty());
    fs::remove_all(folder);
}

TEST_CASE("material: a lista só traz as pastas que têm a cor, em ordem alfabética") {
    const fs::path root = fs::temp_directory_path() / "crs_test_materiais";
    fs::remove_all(root);
    for (const char *name : {"pedra", "areia", "vazia"}) fs::create_directories(root / name);
    std::ofstream{root / "pedra" / "pedra_diff_2k.jpg"};
    std::ofstream{root / "areia" / "areia_diff_2k.jpg"};
    std::ofstream{root / "vazia" / "vazia_rough_2k.jpg"};  //sem a cor: não é material

    auto materials = listMaterials(root.string());

    REQUIRE(materials.size() == 2);
    CHECK(fs::path{materials[0]}.filename() == "areia");
    CHECK(fs::path{materials[1]}.filename() == "pedra");
    CHECK(listMaterials((root / "nao_existe").string()).empty());
    fs::remove_all(root);
}
