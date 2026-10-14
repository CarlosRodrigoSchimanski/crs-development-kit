//implementação Windows da camada de plataforma (o CMake só compila este arquivo no Windows).
//Único lugar do projeto que inclui <windows.h>
#include "engine/platform/lve_platform.hpp"

//corta partes raramente usadas do windows.h, e NOMINMAX impede que ele defina as macros min/max
//(que quebrariam std::min/std::max/glm::min em qualquer arquivo que viesse depois)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <timeapi.h>

namespace lve::platform {

    PlatformScope::PlatformScope() {
        timeBeginPeriod(1);
    }

    PlatformScope::~PlatformScope() {
        timeEndPeriod(1);
    }
}
