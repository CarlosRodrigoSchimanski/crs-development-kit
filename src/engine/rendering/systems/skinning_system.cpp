#include "engine/rendering/systems/skinning_system.hpp"

#include "engine/animation/lve_animation.hpp"
#include "engine/rendering/lve_swap_chain.hpp"

#include <cmath>

namespace lve {

    //quantas matrizes de osso cabem no buffer de cada frame no começo (cresce sozinho se precisar)
    constexpr size_t INITIAL_BONE_CAPACITY = 256;

    SkinningSystem::SkinningSystem(LveDevice &device) : lveDevice{device} {
        boneBuffers.resize(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
        for (auto &boneBuffer : boneBuffers) {
            boneBuffer = createBoneBuffer(INITIAL_BONE_CAPACITY);
        }
    }

    std::unique_ptr<LveBuffer> SkinningSystem::createBoneBuffer(size_t capacity) {
        auto boneBuffer = std::make_unique<LveBuffer>(
            lveDevice,
            sizeof(glm::mat4),
            static_cast<uint32_t>(capacity),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        boneBuffer->map();
        return boneBuffer;
    }

    VkDescriptorBufferInfo SkinningSystem::boneBufferInfo(int frameIndex) const {
        return boneBuffers[frameIndex]->descriptorInfo();
    }

    //a animação pedida pelo nome; vazio ou não achou = a primeira do modelo (nullptr = o modelo não tem)
    static const AnimationClip *findClip(const LveModel &model, const std::string &name) {
        const std::vector<AnimationClip> &clips = model.getAnimations();
        for (const AnimationClip &clip : clips) {
            if (clip.name == name) return &clip;
        }
        return clips.empty() ? nullptr : &clips.front();
    }

    bool SkinningSystem::update(LveScene &scene, int frameIndex) {
        //todas as matrizes do frame, uma fatia por objeto animado
        std::vector<glm::mat4> bones;
        for (auto &kv : scene.getGameObjects()) {
            LveGameObject &obj = kv.second;
            if (!obj.animator || obj.model == nullptr || !obj.model->hasSkin()) continue;
            const Skeleton *skeleton = obj.model->getSkeleton();
            if (skeleton == nullptr) continue;

            //pose do instante atual da animação (sem animação: a pose de descanso)
            std::vector<JointPose> pose;
            if (const AnimationClip *clip = findClip(*obj.model, obj.animator->clip)) {
                float time = obj.animator->time;
                if (obj.animator->loop && clip->duration > 0.f) time = std::fmod(time, clip->duration);
                pose = sampleClip(*skeleton, *clip, time);
            } else {
                pose = sampleClip(*skeleton, AnimationClip{}, 0.f);
            }

            obj.animator->boneOffset = static_cast<uint32_t>(bones.size());
            std::vector<glm::mat4> skin = computeSkinMatrices(*skeleton, pose);
            bones.insert(bones.end(), skin.begin(), skin.end());
        }

        //não cabe: troca o buffer deste frame por um maior (seguro pelo mesmo motivo do das luzes: o
        //beginFrame já esperou a GPU terminar de ler este frame, e ninguém fez o bind dele ainda)
        auto &boneBuffer = boneBuffers[frameIndex];
        size_t capacity = boneBuffer->getBufferSize() / sizeof(glm::mat4);
        bool recreated = false;
        if (bones.size() > capacity) {
            while (capacity < bones.size()) capacity *= 2;
            boneBuffer = createBoneBuffer(capacity);
            recreated = true;
        }

        if (!bones.empty()) {
            boneBuffer->writeToBuffer(bones.data(), bones.size() * sizeof(glm::mat4));
        }
        return recreated;
    }
}
