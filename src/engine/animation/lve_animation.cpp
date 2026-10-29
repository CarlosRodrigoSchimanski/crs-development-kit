#include "engine/animation/lve_animation.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>

namespace lve {

    //quatérnio guardado como (x, y, z, w), no formato do glTF; o glm constrói como (w, x, y, z)
    static glm::quat toQuat(const glm::vec4 &v) {
        return glm::quat(v.w, v.x, v.y, v.z);
    }

    std::vector<JointPose> sampleClip(const Skeleton &skeleton, const AnimationClip &clip, float time) {
        //começa todo mundo na pose de descanso
        std::vector<JointPose> pose(skeleton.joints.size());
        for (size_t i = 0; i < skeleton.joints.size(); i++) {
            pose[i] = {skeleton.joints[i].restTranslation, skeleton.joints[i].restRotation, skeleton.joints[i].restScale};
        }

        for (const AnimationChannel &channel : clip.channels) {
            if (channel.times.empty()) continue;
            if (channel.joint < 0 || static_cast<size_t>(channel.joint) >= pose.size()) continue;

            //os dois keyframes em volta do tempo e quanto do caminho entre eles já foi (0 a 1)
            glm::vec4 value;
            if (time <= channel.times.front()) {
                value = channel.values.front();
            } else if (time >= channel.times.back()) {
                value = channel.values.back();
            } else {
                //o primeiro keyframe DEPOIS do tempo; o de antes é o anterior a ele
                size_t next = std::upper_bound(channel.times.begin(), channel.times.end(), time) - channel.times.begin();
                size_t prev = next - 1;
                float span = channel.times[next] - channel.times[prev];
                float factor = span > 0.f ? (time - channel.times[prev]) / span : 0.f;
                if (channel.step) {
                    value = channel.values[prev];
                } else if (channel.path == AnimationPath::Rotation) {
                    //rotação: mistura pelo arco (slerp), não em linha reta
                    glm::quat q = glm::slerp(toQuat(channel.values[prev]), toQuat(channel.values[next]), factor);
                    value = glm::vec4{q.x, q.y, q.z, q.w};
                } else {
                    value = glm::mix(channel.values[prev], channel.values[next], factor);
                }
            }

            JointPose &joint = pose[channel.joint];
            if (channel.path == AnimationPath::Translation) joint.translation = glm::vec3{value};
            if (channel.path == AnimationPath::Rotation) joint.rotation = glm::normalize(toQuat(value));
            if (channel.path == AnimationPath::Scale) joint.scale = glm::vec3{value};
        }
        return pose;
    }

    std::vector<glm::mat4> computeSkinMatrices(const Skeleton &skeleton, const std::vector<JointPose> &pose) {
        std::vector<glm::mat4> global(skeleton.joints.size());
        std::vector<glm::mat4> skin(skeleton.joints.size());
        for (size_t i = 0; i < skeleton.joints.size(); i++) {
            const Joint &joint = skeleton.joints[i];
            //local do osso: translação x rotação x escala (a mesma ordem do transform dos objetos)
            glm::mat4 local = glm::translate(glm::mat4{1.f}, pose[i].translation)
                * glm::mat4_cast(pose[i].rotation)
                * glm::scale(glm::mat4{1.f}, pose[i].scale);
            //pai antes do filho no vetor: a global do pai já está pronta
            glm::mat4 parentGlobal = joint.parent >= 0 ? global[joint.parent] : skeleton.rootTransform;
            global[i] = parentGlobal * local;
            skin[i] = global[i] * joint.inverseBind;
        }
        return skin;
    }

    std::vector<JointPose> blendPoses(const std::vector<JointPose> &a, const std::vector<JointPose> &b, float weight) {
        std::vector<JointPose> result(a.size());
        for (size_t i = 0; i < a.size() && i < b.size(); i++) {
            result[i].translation = glm::mix(a[i].translation, b[i].translation, weight);
            result[i].rotation = glm::slerp(a[i].rotation, b[i].rotation, weight);
            result[i].scale = glm::mix(a[i].scale, b[i].scale, weight);
        }
        return result;
    }
}
