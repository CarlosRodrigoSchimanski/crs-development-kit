#pragma once

#include "engine/scene/crs_scene.hpp"

#include <functional>
#include <memory>
#include <string>

namespace crsdk {
    //grava a cena num arquivo .json (só o .cpp conhece o formato)
    void saveScene(const CrsScene &scene, const std::string &filepath);

    //quem transforma o caminho de um modelo em modelo carregado (no programa: o CrsModelCache)
    using ModelLoader = std::function<std::shared_ptr<CrsModel>(const std::string &filepath)>;

    //lê uma cena gravada pelo saveScene; os ids do arquivo são mantidos (o parentId continua valendo)
    CrsScene loadScene(const std::string &filepath, const ModelLoader &loadModel);
}
