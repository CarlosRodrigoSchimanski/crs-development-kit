#pragma once

#include "engine/animation/crs_skeleton.hpp"

#include <string>
#include <vector>

namespace crsdk {
    //animação de um arquivo separado (uma por .glb): os canais apontam pros ossos pelo NOME,
    //porque o índice de cada osso só existe dentro de um esqueleto
    struct AnimationAsset {
        AnimationClip clip;                   //nome = nome do arquivo; channel.joint = posição em jointNames
        std::vector<std::string> jointNames;
    };

    //lê o arquivo; com mais de uma animação dentro, usa a de mesmo nome do arquivo (senão a primeira)
    AnimationAsset loadAnimationFile(const std::string &filepath);

    //a animação pronta pra um esqueleto
    struct BoundClip {
        AnimationClip clip;
        int missingJoints = 0;  //ossos da animação que o esqueleto não tem (os canais deles ficam de fora)
    };

    //liga cada canal ao osso de mesmo nome no esqueleto
    BoundClip bindAnimation(const AnimationAsset &asset, const Skeleton &skeleton);
}
