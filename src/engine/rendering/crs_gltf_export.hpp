#pragma once

#include "engine/rendering/crs_model.hpp"
#include "engine/scene/crs_scene.hpp"

#include <functional>
#include <string>
#include <vector>

namespace crsdk {
    //uma malha pra gravar num .glb: só a forma (posições, normais, triângulos), com um nome
    struct ExportMesh {
        std::string name;
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;  //vazio ou um por posição
        std::vector<uint32_t> indices;   //de 3 em 3
    };

    //grava as malhas num .glb (glTF binário): um objeto por malha, com o nome dela. Sem material nem
    //textura: é o "molde" pra modelar em cima no Blender. Lança exceção se não der pra gravar
    void writeGlb(const std::string &filepath, const std::vector<ExportMesh> &meshes);

    //a geometria de um objeto: o Builder do modelo dele (nullptr = sem geometria)
    using GeometryOf = std::function<const CrsModel::Builder *(const CrsGameObject &)>;

    //as malhas do grupo (o objeto e todos os descendentes), na posição em que estão na cena, com o
    //objeto do grupo na ORIGEM: é o que sai pro Blender pra modelar a colisão em volta. Cada peça vira
    //uma malha com o nome dela no Outliner; a colisão que já veio num modelo sai como UCX_<nome> (dá
    //pra editar e mandar de volta). Gizmos do editor (Empty, câmera) ficam de fora
    std::vector<ExportMesh> collectGroupMeshes(CrsScene &scene, CrsGameObject::id_t rootId, const GeometryOf &geometryOf);
}
