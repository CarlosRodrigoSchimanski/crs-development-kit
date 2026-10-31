//implementação Windows da camada de plataforma (o CMake só compila este arquivo no Windows).
//Único lugar do projeto que inclui <windows.h>
#include "engine/platform/crs_platform.hpp"

//corta partes raramente usadas do windows.h, e NOMINMAX impede que ele defina as macros min/max
//(que quebrariam std::min/std::max/glm::min em qualquer arquivo que viesse depois)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <timeapi.h>

#include <iterator>
#include <string>

namespace crsdk::platform {

    PlatformScope::PlatformScope() {
        timeBeginPeriod(1);
    }

    PlatformScope::~PlatformScope() {
        timeEndPeriod(1);
    }

    bool needsRedrawDuringResize() { return true; }

    std::filesystem::path executablePath() {
        //versão W (UTF-16): funciona com acento e qualquer caractere no caminho
        wchar_t buffer[MAX_PATH * 4];
        const DWORD size = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
        if (size == 0 || size == std::size(buffer)) return {};
        return std::filesystem::path{std::wstring{buffer, size}};
    }
}
