#pragma once

#include <optional>
#include <string>

namespace crsdk {
    //janela de abrir arquivo do sistema; vazio = cancelou
    std::optional<std::string> openFileDialog(const char *filterName, const char *extension, const std::string &startFolder);
    //janela de salvar arquivo do sistema; vazio = cancelou
    std::optional<std::string> saveFileDialog(const char *filterName, const char *extension,
        const std::string &startFolder, const std::string &defaultName);
}
