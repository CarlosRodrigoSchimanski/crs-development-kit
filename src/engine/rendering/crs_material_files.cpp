#include "engine/rendering/crs_material_files.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <initializer_list>

namespace crsdk {

    static std::string lowercase(std::string text) {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return text;
    }

    static bool hasAny(const std::string &name, std::initializer_list<const char *> parts) {
        for (const char *part : parts) {
            if (name.find(part) != std::string::npos) return true;
        }
        return false;
    }

    MaterialFiles findMaterialFiles(const std::string &folder) {
        MaterialFiles files{};
        std::error_code error;
        if (!std::filesystem::is_directory(folder, error)) return files;

        //em ordem alfabética: com dois arquivos do mesmo mapa (2k e 4k), a escolha não muda a cada vez
        std::vector<std::filesystem::path> paths;
        for (const auto &entry : std::filesystem::directory_iterator{folder, error}) {
            if (entry.is_regular_file()) paths.push_back(entry.path());
        }
        std::sort(paths.begin(), paths.end());

        for (const auto &path : paths) {
            const std::string extension = lowercase(path.extension().string());
            if (extension != ".png" && extension != ".jpg" && extension != ".jpeg" && extension != ".tga") continue;
            const std::string name = lowercase(path.stem().string());
            const std::string full = path.generic_string();
            auto take = [&](std::string &slot) { if (slot.empty()) slot = full; };

            if (hasAny(name, {"_nor_dx", "_normal_dx"})) continue;  //normal do DirectX: o relevo sairia invertido
            if (hasAny(name, {"_nor_gl", "_normal_gl", "_normal", "_nor"})) take(files.normal);
            else if (hasAny(name, {"_rough"})) take(files.roughness);
            else if (hasAny(name, {"_ao", "_occlusion"})) take(files.ao);
            else if (hasAny(name, {"_disp", "_height"})) take(files.displacement);
            else if (hasAny(name, {"_diff", "_albedo", "_basecolor", "_base_color", "_color", "_col"})) take(files.albedo);
        }
        return files;
    }

    std::vector<std::string> listMaterials(const std::string &root) {
        std::vector<std::string> materials;
        std::error_code error;
        if (!std::filesystem::is_directory(root, error)) return materials;
        for (const auto &entry : std::filesystem::directory_iterator{root, error}) {
            if (!entry.is_directory()) continue;
            const std::string folder = entry.path().generic_string();
            if (!findMaterialFiles(folder).albedo.empty()) materials.push_back(folder);
        }
        std::sort(materials.begin(), materials.end());
        return materials;
    }
}
