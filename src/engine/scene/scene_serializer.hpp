#pragma once

#include "engine/scene/lve_scene.hpp"

#include <functional>
#include <memory>
#include <string>

namespace lve {
    //grava a cena num arquivo .json (só o .cpp conhece o formato)
    void saveScene(const LveScene &scene, const std::string &filepath);

    //quem transforma o caminho de um modelo em modelo carregado (no programa: o LveModelCache)
    using ModelLoader = std::function<std::shared_ptr<LveModel>(const std::string &filepath)>;

    //lê uma cena gravada pelo saveScene; os ids do arquivo são mantidos (o parentId continua valendo)
    LveScene loadScene(const std::string &filepath, const ModelLoader &loadModel);
}
