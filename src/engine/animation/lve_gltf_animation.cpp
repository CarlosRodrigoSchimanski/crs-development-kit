#include "engine/animation/lve_gltf_animation.hpp"

#include <algorithm>

namespace lve {
    std::vector<AnimationClip> readGltfAnimations(const cgltf_data &data, const std::function<int(const cgltf_node *)> &jointOf) {
        std::vector<AnimationClip> clips;
        for (cgltf_size a = 0; a < data.animations_count; a++) {
            const cgltf_animation &animation = data.animations[a];
            AnimationClip clip;
            clip.name = animation.name ? animation.name : "animation";

            for (cgltf_size c = 0; c < animation.channels_count; c++) {
                const cgltf_animation_channel &source = animation.channels[c];
                int joint = jointOf(source.target_node);
                if (joint < 0) continue;  //não é osso

                AnimationChannel channel;
                channel.joint = joint;
                if (source.target_path == cgltf_animation_path_type_translation) channel.path = AnimationPath::Translation;
                else if (source.target_path == cgltf_animation_path_type_rotation) channel.path = AnimationPath::Rotation;
                else if (source.target_path == cgltf_animation_path_type_scale) channel.path = AnimationPath::Scale;
                else continue;  //morph targets (shape keys) ficam de fora

                const cgltf_animation_sampler &sampler = *source.sampler;
                channel.step = sampler.interpolation == cgltf_interpolation_type_step;
                //CUBICSPLINE guarda 3 valores por keyframe (tangente, valor, tangente): usa só o do meio
                bool cubic = sampler.interpolation == cgltf_interpolation_type_cubic_spline;
                cgltf_size components = channel.path == AnimationPath::Rotation ? 4 : 3;

                cgltf_size keys = sampler.input->count;
                channel.times.resize(keys);
                channel.values.resize(keys);
                for (cgltf_size k = 0; k < keys; k++) {
                    cgltf_accessor_read_float(sampler.input, k, &channel.times[k], 1);
                    float v[4] = {0.f, 0.f, 0.f, 0.f};
                    cgltf_accessor_read_float(sampler.output, cubic ? k * 3 + 1 : k, v, components);
                    channel.values[k] = glm::vec4{v[0], v[1], v[2], v[3]};
                }
                if (!channel.times.empty()) clip.duration = std::max(clip.duration, channel.times.back());
                clip.channels.push_back(std::move(channel));
            }
            clips.push_back(std::move(clip));
        }
        return clips;
    }
}
