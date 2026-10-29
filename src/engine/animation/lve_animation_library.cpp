#include "engine/animation/lve_animation_library.hpp"

#include <iostream>

namespace lve {
    const AnimationClip *LveAnimationLibrary::get(const std::string &filepath, const Skeleton &skeleton) {
        auto key = std::make_pair(filepath, &skeleton);
        auto foundBound = bound.find(key);
        if (foundBound != bound.end()) return &foundBound->second;

        auto foundAsset = assets.find(filepath);
        if (foundAsset == assets.end()) {
            try {
                foundAsset = assets.emplace(filepath, loadAnimationFile(filepath)).first;
            } catch (const std::exception &e) {
                std::cerr << "Animação: " << e.what() << std::endl;
                foundAsset = assets.emplace(filepath, std::nullopt).first;
            }
        }
        if (!foundAsset->second) return nullptr;

        BoundClip result = bindAnimation(*foundAsset->second, skeleton);
        if (result.missingJoints > 0) {
            std::cerr << "Animação " << filepath << ": " << result.missingJoints
                      << " ossos não existem no esqueleto deste modelo" << std::endl;
        }
        return &bound.emplace(key, std::move(result.clip)).first->second;
    }
}
