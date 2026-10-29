#include "engine/animation/lve_animation_file.hpp"

#include <doctest/doctest.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace lve;

//bytes -> texto base64 (o glTF aceita os dados embutidos assim, sem arquivo .bin)
static std::string toBase64(const std::vector<float> &values) {
    static const char *table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<unsigned char> bytes(values.size() * sizeof(float));
    std::memcpy(bytes.data(), values.data(), bytes.size());
    std::string out;
    for (size_t i = 0; i < bytes.size(); i += 3) {
        unsigned int chunk = bytes[i] << 16;
        if (i + 1 < bytes.size()) chunk |= bytes[i + 1] << 8;
        if (i + 2 < bytes.size()) chunk |= bytes[i + 2];
        out += table[(chunk >> 18) & 63];
        out += table[(chunk >> 12) & 63];
        out += i + 1 < bytes.size() ? table[(chunk >> 6) & 63] : '=';
        out += i + 2 < bytes.size() ? table[chunk & 63] : '=';
    }
    return out;
}

//glTF com 2 ossos e 2 animações: "outra" mexe no Hips, "teste_anim" mexe no Spine
static std::string writeTwoAnimationsFile(const std::string &fileName) {
    //tempos [0, 1]; posições do Spine (0,0,0)->(1,2,3); do Hips (5,5,5)->(5,5,5)
    std::vector<float> data = {0, 1,  0, 0, 0, 1, 2, 3,  5, 5, 5, 5, 5, 5};
    std::string path = (std::filesystem::temp_directory_path() / fileName).string();
    std::ofstream out{path};
    out << R"({
        "asset": {"version": "2.0"},
        "scenes": [{"nodes": [0]}],
        "nodes": [{"name": "Hips", "children": [1]}, {"name": "Spine"}],
        "buffers": [{"byteLength": 56, "uri": "data:application/octet-stream;base64,)" << toBase64(data) << R"("}],
        "bufferViews": [{"buffer": 0, "byteLength": 56}],
        "accessors": [
            {"bufferView": 0, "byteOffset": 0, "componentType": 5126, "count": 2, "type": "SCALAR", "min": [0], "max": [1]},
            {"bufferView": 0, "byteOffset": 8, "componentType": 5126, "count": 2, "type": "VEC3"},
            {"bufferView": 0, "byteOffset": 32, "componentType": 5126, "count": 2, "type": "VEC3"}
        ],
        "animations": [
            {"name": "outra", "samplers": [{"input": 0, "output": 2}],
             "channels": [{"sampler": 0, "target": {"node": 0, "path": "translation"}}]},
            {"name": "teste_anim", "samplers": [{"input": 0, "output": 1}],
             "channels": [{"sampler": 0, "target": {"node": 1, "path": "translation"}}]}
        ]
    })";
    return path;
}

TEST_CASE("animação de arquivo: usa a de mesmo nome do arquivo, e o nome vem do arquivo") {
    std::string path = writeTwoAnimationsFile("teste_anim.gltf");
    AnimationAsset asset = loadAnimationFile(path);
    std::filesystem::remove(path);

    CHECK(asset.clip.name == "teste_anim");
    CHECK(asset.clip.duration == doctest::Approx(1.f));
    REQUIRE(asset.clip.channels.size() == 1);
    //o canal aponta pro osso pelo nome: o Spine
    CHECK(asset.jointNames.at(asset.clip.channels[0].joint) == "Spine");
    CHECK(asset.clip.channels[0].values[1].z == doctest::Approx(3.f));
}

TEST_CASE("animação de arquivo: sem uma de mesmo nome, usa a primeira (com o nome do arquivo)") {
    std::string path = writeTwoAnimationsFile("idle_01.gltf");
    AnimationAsset asset = loadAnimationFile(path);
    std::filesystem::remove(path);

    CHECK(asset.clip.name == "idle_01");
    REQUIRE(asset.clip.channels.size() == 1);
    CHECK(asset.jointNames.at(asset.clip.channels[0].joint) == "Hips");
}

TEST_CASE("bindAnimation: liga cada canal ao osso de mesmo nome, em qualquer ordem") {
    AnimationAsset asset;
    asset.jointNames = {"Hips", "Spine", "Tail"};
    for (int joint = 0; joint < 3; joint++) {
        AnimationChannel channel;
        channel.joint = joint;
        channel.times = {0.f};
        channel.values = {glm::vec4{static_cast<float>(joint)}};
        asset.clip.channels.push_back(channel);
    }

    //o esqueleto tem os ossos em outra ordem e não tem rabo
    Skeleton skeleton;
    Joint spine;
    spine.name = "Spine";
    Joint hips;
    hips.name = "Hips";
    skeleton.joints = {spine, hips};

    BoundClip bound = bindAnimation(asset, skeleton);

    REQUIRE(bound.clip.channels.size() == 2);
    CHECK(bound.missingJoints == 1);
    for (const AnimationChannel &channel : bound.clip.channels) {
        //o valor de cada canal era o índice do osso no arquivo: 0 = Hips, 1 = Spine
        const std::string &name = skeleton.joints[channel.joint].name;
        CHECK(name == (channel.values[0].x == 0.f ? "Hips" : "Spine"));
    }
}

TEST_CASE("animação de arquivo: arquivo que não existe é erro") {
    CHECK_THROWS(loadAnimationFile((std::filesystem::temp_directory_path() / "ge_nao_existe.glb").string()));
}
