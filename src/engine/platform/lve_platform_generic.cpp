//implementação genérica da camada de plataforma: SO sem nenhum ajuste especial (Linux/macOS, por enquanto).
//O CMake compila este arquivo em todo SO que não tem implementação própria
#include "engine/platform/lve_platform.hpp"

namespace lve::platform {

    PlatformScope::PlatformScope() {}

    PlatformScope::~PlatformScope() {}

    bool needsRedrawDuringResize() { return false; }
}
