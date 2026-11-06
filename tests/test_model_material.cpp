#include "engine/rendering/crs_model.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace crsdk;

namespace {
    //grava um .glb mínimo: 1 triângulo com UV e 1 material com textura de cor embutida.
    //alphaMode vazio = o campo não vai pro arquivo (o padrão do glTF, OPAQUE).
    //A "imagem" são 4 bytes quaisquer: o loadModel só copia os bytes, quem decodifica é a CrsTexture.
    std::string writeTexturedTriangle(const std::string &name, const std::string &alphaMode) {
        const float positions[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
        const float uvs[6] = {0, 0, 1, 0, 0, 1};
        const unsigned char image[4] = {1, 2, 3, 4};

        std::vector<unsigned char> bin(sizeof(positions) + sizeof(uvs) + sizeof(image));
        std::memcpy(bin.data(), positions, sizeof(positions));
        std::memcpy(bin.data() + 36, uvs, sizeof(uvs));
        std::memcpy(bin.data() + 60, image, sizeof(image));

        std::string material = R"({"pbrMetallicRoughness":{"baseColorTexture":{"index":0}})";
        if (!alphaMode.empty()) material += R"(,"alphaMode":")" + alphaMode + "\"";
        material += "}";

        std::string json = R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],"nodes":[{"mesh":0}],)"
            R"("meshes":[{"primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":1},"material":0}]}],)"
            R"("materials":[)" + material + "],"
            R"("textures":[{"source":0}],"images":[{"bufferView":2,"mimeType":"image/png"}],)"
            R"("accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},)"
            R"({"bufferView":1,"componentType":5126,"count":3,"type":"VEC2"}],)"
            R"("bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":24},)"
            R"({"buffer":0,"byteOffset":60,"byteLength":4}],)"
            R"("buffers":[{"byteLength":64}]})";
        while (json.size() % 4 != 0) json += ' ';

        auto write32 = [](std::ofstream &out, uint32_t value) { out.write(reinterpret_cast<const char *>(&value), 4); };
        const auto path = (std::filesystem::temp_directory_path() / name).string();
        std::ofstream out{path, std::ios::binary};
        write32(out, 0x46546C67);  //"glTF"
        write32(out, 2);
        write32(out, static_cast<uint32_t>(12 + 8 + json.size() + 8 + bin.size()));
        write32(out, static_cast<uint32_t>(json.size()));
        write32(out, 0x4E4F534A);  //"JSON"
        out.write(json.data(), static_cast<std::streamsize>(json.size()));
        write32(out, static_cast<uint32_t>(bin.size()));
        write32(out, 0x004E4942);  //"BIN"
        out.write(reinterpret_cast<const char *>(bin.data()), static_cast<std::streamsize>(bin.size()));
        return path;
    }
}

TEST_CASE("material: alphaMode MASK ou BLEND liga o recorte; OPAQUE (ou sem o campo) não") {
    CrsModel::Builder builder;

    builder.loadModel(writeTexturedTriangle("crs_test_mask.glb", "MASK"));
    CHECK(builder.baseColorImage.size() == 4);
    CHECK(builder.alphaCutout);

    builder.loadModel(writeTexturedTriangle("crs_test_blend.glb", "BLEND"));
    CHECK(builder.alphaCutout);

    builder.loadModel(writeTexturedTriangle("crs_test_opaque.glb", "OPAQUE"));
    CHECK(builder.baseColorImage.size() == 4);
    CHECK_FALSE(builder.alphaCutout);

    //o mesmo Builder recarregado: o recorte do arquivo anterior não pode sobrar
    builder.loadModel(writeTexturedTriangle("crs_test_mask2.glb", "MASK"));
    builder.loadModel(writeTexturedTriangle("crs_test_default.glb", ""));
    CHECK_FALSE(builder.alphaCutout);
}
