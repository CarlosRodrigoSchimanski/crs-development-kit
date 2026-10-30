//implementação genérica da camada de plataforma: SO sem nenhum ajuste especial (Linux/macOS, por enquanto).
//O CMake compila este arquivo em todo SO que não tem implementação própria
#include "engine/platform/crs_platform.hpp"

namespace crsdk::platform {

    PlatformScope::PlatformScope() {}

    PlatformScope::~PlatformScope() {}

    bool needsRedrawDuringResize() { return false; }
}
