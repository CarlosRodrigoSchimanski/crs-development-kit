//implementação genérica da camada de plataforma: SO sem nenhum ajuste especial (Linux/macOS, por enquanto).
//O CMake compila este arquivo em todo SO que não tem implementação própria
#include "engine/platform/crs_platform.hpp"

namespace crsdk::platform {

    PlatformScope::PlatformScope() {}

    PlatformScope::~PlatformScope() {}

    bool needsRedrawDuringResize() { return false; }

    std::filesystem::path executablePath() {
        //Linux: /proc/self/exe é um link pro executável; nos outros SO não existe e volta vazio
        std::error_code error;
        std::filesystem::path path = std::filesystem::read_symlink("/proc/self/exe", error);
        return error ? std::filesystem::path{} : path;
    }
}
