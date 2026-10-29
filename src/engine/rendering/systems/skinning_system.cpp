#include "engine/rendering/systems/skinning_system.hpp"

#include "engine/animation/lve_animation.hpp"
#include "engine/rendering/lve_swap_chain.hpp"

#include <cmath>
#include <filesystem>

namespace lve {

    //quantas matrizes de osso cabem no buffer de cada frame no começo (cresce sozinho se precisar)
    constexpr size_t INITIAL_BONE_CAPACITY = 256;

    SkinningSystem::SkinningSystem(LveDevice &device, LveAnimationLibrary &animationLibrary)
        : lveDevice{device}, animationLibrary{animationLibrary} {
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

    //primeiro os arquivos do animator (pelo nome do arquivo), depois as do modelo;
    //vazio ou não achou = o primeiro arquivo, ou a primeira do modelo
    const AnimationClip *SkinningSystem::findClip(const LveModel &model, const AnimatorComponent &animator, const std::string &name) {
        const Skeleton &skeleton = *model.getSkeleton();
        for (const std::string &file : animator.animations) {
            if (std::filesystem::path{file}.stem().string() == name) return animationLibrary.get(file, skeleton);
        }
        const std::vector<AnimationClip> &clips = model.getAnimations();
        for (const AnimationClip &clip : clips) {
            if (clip.name == name) return &clip;
        }
        if (!animator.animations.empty()) return animationLibrary.get(animator.animations.front(), skeleton);
        return clips.empty() ? nullptr : &clips.front();
    }

    //pose de uma animação num instante (no loop, o tempo dá a volta pela duração; sem animação: a de descanso)
    static std::vector<JointPose> samplePose(const Skeleton &skeleton, const AnimationClip *clip, float time, bool loop) {
        if (clip == nullptr) return sampleClip(skeleton, AnimationClip{}, 0.f);
        if (loop && clip->duration > 0.f) time = std::fmod(time, clip->duration);
        return sampleClip(skeleton, *clip, time);
    }

    bool SkinningSystem::update(LveScene &scene, int frameIndex) {
        //todas as matrizes do frame, uma fatia por objeto animado
        std::vector<glm::mat4> bones;
        for (auto &kv : scene.getGameObjects()) {
            LveGameObject &obj = kv.second;
            if (!obj.animator || obj.model == nullptr || !obj.model->hasSkin()) continue;
            const Skeleton *skeleton = obj.model->getSkeleton();
            if (skeleton == nullptr) continue;

            //pose do instante atual da animação
            const AnimatorComponent &animator = *obj.animator;
            std::vector<JointPose> pose = samplePose(
                *skeleton, findClip(*obj.model, animator, animator.clip), animator.time, animator.loop);
            //transição: mistura com a anterior, que vai sumindo
            if (animator.isBlending()) {
                std::vector<JointPose> previous = samplePose(
                    *skeleton, findClip(*obj.model, animator, animator.previousClip), animator.previousTime, animator.previousLoop);
                pose = blendPoses(previous, pose, animator.blendWeight());
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
