#pragma once

#include "engine/animation/lve_animation_file.hpp"

#include <map>
#include <optional>
#include <string>
#include <utility>

namespace lve {
    //animações de arquivo: cada arquivo é lido uma vez, e ligado uma vez a cada esqueleto que usa
    class LveAnimationLibrary {
        public:
            //a animação do arquivo pronta pro esqueleto; nullptr = o arquivo não abriu (o erro sai uma vez no console)
            const AnimationClip *get(const std::string &filepath, const Skeleton &skeleton);

        private:
            //nullopt = deu erro ao ler (não tenta de novo todo frame)
            std::map<std::string, std::optional<AnimationAsset>> assets;
            std::map<std::pair<std::string, const Skeleton *>, AnimationClip> bound;
    };
}
