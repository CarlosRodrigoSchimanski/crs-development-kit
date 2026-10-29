#pragma once

#include "engine/scene/lve_scene.hpp"

#include <string>

namespace lve {
    //grava a cena num arquivo .json (só o .cpp conhece o formato)
    void saveScene(const LveScene &scene, const std::string &filepath);
}
