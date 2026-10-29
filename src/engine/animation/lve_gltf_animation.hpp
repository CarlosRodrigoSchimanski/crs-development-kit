#pragma once

#include "engine/animation/lve_skeleton.hpp"

#include <cgltf.h>

#include <functional>
#include <vector>

namespace lve {
    //as animações de um glTF já aberto; jointOf diz qual osso cada nó é (-1 = não é osso: o canal fica de fora)
    std::vector<AnimationClip> readGltfAnimations(const cgltf_data &data, const std::function<int(const cgltf_node *)> &jointOf);
}
