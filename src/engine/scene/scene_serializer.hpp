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

    //um texto que muda quando qualquer coisa da cena muda (objetos, componentes, a paleta e as alturas
    //em uso do terreno): o histórico do Ctrl+Z compara dois deles pra saber se uma ação mudou algo
    std::string sceneFingerprint(const CrsScene &scene);
    //lê uma cena gravada pelo saveScene; os ids do arquivo são mantidos (o parentId continua valendo)
    CrsScene loadScene(const std::string &filepath, const ModelLoader &loadModel);
}
