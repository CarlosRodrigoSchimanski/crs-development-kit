#include "engine/animation/lve_animation_file.hpp"
#include "engine/animation/lve_gltf_animation.hpp"

#include <algorithm>
#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>

namespace lve {
    AnimationAsset loadAnimationFile(const std::string &filepath) {
        cgltf_options options{};
        cgltf_data *rawData = nullptr;
        if (cgltf_parse_file(&options, filepath.c_str(), &rawData) != cgltf_result_success) {
            throw std::runtime_error("não consegui ler a animação: " + filepath);
        }
        std::unique_ptr<cgltf_data, decltype(&cgltf_free)> data{rawData, &cgltf_free};
        if (cgltf_load_buffers(&options, data.get(), filepath.c_str()) != cgltf_result_success) {
            throw std::runtime_error("não consegui ler os dados da animação: " + filepath);
        }

        //cada nó animado vira um osso pelo nome (na ordem em que aparece)
        AnimationAsset asset;
        std::map<std::string, int> nameIndex;
        auto jointOf = [&](const cgltf_node *node) {
            if (node == nullptr || node->name == nullptr) return -1;
            auto [it, inserted] = nameIndex.emplace(node->name, static_cast<int>(asset.jointNames.size()));
            if (inserted) asset.jointNames.push_back(node->name);
            return it->second;
        };
        std::vector<AnimationClip> clips = readGltfAnimations(*data, jointOf);
        if (clips.empty()) {
            throw std::runtime_error("o arquivo não tem animação: " + filepath);
        }

        //o Mixamo chama toda animação de "mixamo.com": quem dá o nome é o arquivo
        std::string name = std::filesystem::path{filepath}.stem().string();
        auto sameName = std::find_if(clips.begin(), clips.end(), [&](const AnimationClip &clip) { return clip.name == name; });
        asset.clip = std::move(sameName != clips.end() ? *sameName : clips.front());
        asset.clip.name = name;
        return asset;
    }

    BoundClip bindAnimation(const AnimationAsset &asset, const Skeleton &skeleton) {
        std::map<std::string, int> skeletonIndex;
        for (size_t i = 0; i < skeleton.joints.size(); i++) {
            skeletonIndex[skeleton.joints[i].name] = static_cast<int>(i);
        }

        BoundClip bound;
        bound.clip.name = asset.clip.name;
        bound.clip.duration = asset.clip.duration;
        std::set<int> missing;
        for (const AnimationChannel &channel : asset.clip.channels) {
            auto found = skeletonIndex.find(asset.jointNames.at(channel.joint));
            if (found == skeletonIndex.end()) {
                missing.insert(channel.joint);
                continue;
            }
            AnimationChannel copy = channel;
            copy.joint = found->second;
            bound.clip.channels.push_back(std::move(copy));
        }
        bound.missingJoints = static_cast<int>(missing.size());
        return bound;
    }
}
