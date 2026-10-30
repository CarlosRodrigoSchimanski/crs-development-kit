#pragma once

#include "engine/animation/crs_skeleton.hpp"

#include <vector>

namespace crsdk {

    //pose local de um osso num instante (relativa ao pai)
    struct JointPose {
        glm::vec3 translation{0.f};
        glm::quat rotation{1.f, 0.f, 0.f, 0.f};
        glm::vec3 scale{1.f};
    };

    //a pose local de cada osso no tempo t da animação (quem a animação não mexe fica na pose de descanso)
    std::vector<JointPose> sampleClip(const Skeleton &skeleton, const AnimationClip &clip, float time);

    //mistura duas poses do mesmo esqueleto: weight 0 = só a, 1 = só b (a transição entre animações)
    std::vector<JointPose> blendPoses(const std::vector<JointPose> &a, const std::vector<JointPose> &b, float weight);

    //a matriz de skinning de cada osso (global x inverseBind): move o vértice da pose de descanso pra pose
    std::vector<glm::mat4> computeSkinMatrices(const Skeleton &skeleton, const std::vector<JointPose> &pose);
}
