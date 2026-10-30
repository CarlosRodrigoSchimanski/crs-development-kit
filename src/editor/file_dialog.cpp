#include "editor/file_dialog.hpp"

#include <nfd.h>

#include <filesystem>
#include <stdexcept>

namespace crsdk {
    //o nfd trabalha em UTF-8
    static std::filesystem::path fromUtf8(const nfdu8char_t *text) {
        return std::filesystem::path{std::u8string{reinterpret_cast<const char8_t *>(text)}};
    }

    static std::string toUtf8(const std::filesystem::path &path) {
        std::u8string text = path.u8string();
        return std::string{reinterpret_cast<const char *>(text.c_str()), text.size()};
    }

    //caminho relativo à pasta do programa quando dá (scenes/x.json em vez de C:/.../scenes/x.json)
    static std::string toProjectPath(const std::filesystem::path &path) {
        return std::filesystem::proximate(path).generic_string();
    }

    //abre a janela, devolve o caminho escolhido e libera o que o nfd alocou
    template <typename ShowDialog>
    static std::optional<std::string> runDialog(ShowDialog showDialog) {
        if (NFD_Init() != NFD_OKAY) {
            throw std::runtime_error(std::string{"janela de arquivo: "} + NFD_GetError());
        }
        nfdu8char_t *outPath = nullptr;
        nfdresult_t result = showDialog(&outPath);

        std::optional<std::string> chosen{};
        std::string error{};
        if (result == NFD_OKAY) {
            chosen = toProjectPath(fromUtf8(outPath));
            NFD_FreePathU8(outPath);
        } else if (result == NFD_ERROR) {
            error = NFD_GetError();
        }
        NFD_Quit();

        if (result == NFD_ERROR) {
            throw std::runtime_error("janela de arquivo: " + error);
        }
        return chosen;
    }

    std::optional<std::string> openFileDialog(const char *filterName, const char *extension, const std::string &startFolder) {
        std::string folder = toUtf8(std::filesystem::absolute(startFolder));
        nfdu8filteritem_t filter{filterName, extension};
        return runDialog([&](nfdu8char_t **outPath) {
            return NFD_OpenDialogU8(outPath, &filter, 1, folder.c_str());
        });
    }

    std::optional<std::string> saveFileDialog(const char *filterName, const char *extension,
        const std::string &startFolder, const std::string &defaultName) {
        std::string folder = toUtf8(std::filesystem::absolute(startFolder));
        nfdu8filteritem_t filter{filterName, extension};
        auto chosen = runDialog([&](nfdu8char_t **outPath) {
            return NFD_SaveDialogU8(outPath, &filter, 1, folder.c_str(), defaultName.c_str());
        });
        //digitou o nome sem a extensão
        if (chosen && std::filesystem::path{*chosen}.extension().empty()) {
            *chosen += std::string{"."} + extension;
        }
        return chosen;
    }
}
