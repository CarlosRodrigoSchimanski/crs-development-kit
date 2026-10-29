#include "engine/animation/lve_animation.hpp"
#include "engine/animation/lve_animator_system.hpp"
#include "engine/gameplay/character_animation_system.hpp"

#include <doctest/doctest.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace lve;

//quatérnio no formato do glTF (x, y, z, w), como o loader vai guardar nos keyframes
static glm::vec4 gltfQuat(const glm::quat &q) {
    return {q.x, q.y, q.z, q.w};
}

//2 ossos: a raiz na origem e o filho 1 à direita dela (+X)
static Skeleton twoJointSkeleton() {
    Skeleton skeleton;
    Joint root;
    root.name = "root";
    Joint child;
    child.name = "child";
    child.parent = 0;
    child.restTranslation = {1.f, 0.f, 0.f};
    skeleton.joints = {root, child};
    return skeleton;
}

static void checkSameMatrix(const glm::mat4 &a, const glm::mat4 &b) {
    for (int column = 0; column < 4; column++) {
        for (int row = 0; row < 4; row++) {
            CHECK(a[column][row] == doctest::Approx(b[column][row]).epsilon(1e-4));
        }
    }
}

TEST_CASE("sampleClip: no keyframe dá o valor dele, no meio mistura, fora das pontas usa a ponta") {
    Skeleton skeleton = twoJointSkeleton();
    AnimationClip clip;
    AnimationChannel move;
    move.joint = 0;
    move.path = AnimationPath::Translation;
    move.times = {1.f, 3.f};
    move.values = {glm::vec4{0.f, 0.f, 0.f, 0.f}, glm::vec4{4.f, 0.f, 0.f, 0.f}};
    clip.channels = {move};

    CHECK(sampleClip(skeleton, clip, 1.f)[0].translation.x == doctest::Approx(0.f));
    CHECK(sampleClip(skeleton, clip, 2.f)[0].translation.x == doctest::Approx(2.f));  //metade do caminho
    CHECK(sampleClip(skeleton, clip, 3.f)[0].translation.x == doctest::Approx(4.f));
    CHECK(sampleClip(skeleton, clip, 0.f)[0].translation.x == doctest::Approx(0.f));  //antes do primeiro
    CHECK(sampleClip(skeleton, clip, 9.f)[0].translation.x == doctest::Approx(4.f));  //depois do último
    //o osso que a animação não mexe fica na pose de descanso
    CHECK(sampleClip(skeleton, clip, 2.f)[1].translation.x == doctest::Approx(1.f));
}

TEST_CASE("sampleClip: a rotação avança o ângulo na mesma proporção do tempo (slerp)") {
    Skeleton skeleton = twoJointSkeleton();
    AnimationChannel turn;
    turn.joint = 0;
    turn.path = AnimationPath::Rotation;
    turn.times = {0.f, 1.f};
    turn.values = {
        gltfQuat(glm::quat{1.f, 0.f, 0.f, 0.f}),
        gltfQuat(glm::angleAxis(glm::half_pi<float>(), glm::vec3{0.f, 1.f, 0.f}))};  //90 graus em Y
    AnimationClip clip;
    clip.channels = {turn};

    glm::quat half = sampleClip(skeleton, clip, 0.5f)[0].rotation;

    CHECK(glm::angle(half) == doctest::Approx(glm::quarter_pi<float>()));  //45 graus

    //um quarto do tempo = um quarto do ângulo; a mistura em linha reta daria ~21.6 em vez de 22.5
    glm::quat quarter = sampleClip(skeleton, clip, 0.25f)[0].rotation;
    CHECK(glm::angle(quarter) == doctest::Approx(glm::radians(22.5f)).epsilon(1e-3));
}

TEST_CASE("sampleClip: canal em degrau (STEP) não mistura") {
    Skeleton skeleton = twoJointSkeleton();
    AnimationChannel jump;
    jump.joint = 0;
    jump.path = AnimationPath::Translation;
    jump.step = true;
    jump.times = {0.f, 1.f};
    jump.values = {glm::vec4{0.f}, glm::vec4{10.f, 0.f, 0.f, 0.f}};
    AnimationClip clip;
    clip.channels = {jump};

    CHECK(sampleClip(skeleton, clip, 0.9f)[0].translation.x == doctest::Approx(0.f));
}

TEST_CASE("computeSkinMatrices: o filho herda a rotação do pai") {
    Skeleton skeleton = twoJointSkeleton();
    std::vector<JointPose> pose = sampleClip(skeleton, AnimationClip{}, 0.f);
    pose[0].rotation = glm::angleAxis(glm::half_pi<float>(), glm::vec3{0.f, 1.f, 0.f});  //raiz gira 90 em Y

    std::vector<glm::mat4> skin = computeSkinMatrices(skeleton, pose);

    //sem inverseBind (identidade), a coluna 3 é a posição do osso: o "1 à direita" girou pra Z
    glm::vec3 childPosition{skin[1][3]};
    CHECK(childPosition.x == doctest::Approx(0.f).epsilon(1e-4));
    CHECK(std::abs(childPosition.z) == doctest::Approx(1.f));
}

TEST_CASE("computeSkinMatrices: na pose de descanso o skinning não mexe no vértice (identidade)") {
    Skeleton skeleton = twoJointSkeleton();
    skeleton.joints[0].restRotation = glm::angleAxis(0.3f, glm::vec3{0.f, 0.f, 1.f});
    skeleton.rootTransform = glm::scale(glm::mat4{1.f}, glm::vec3{0.01f});  //como o Armature do Mixamo
    //inverseBind = inversa da global de descanso (o que o exportador grava no glb)
    std::vector<glm::mat4> restGlobal = computeSkinMatrices(skeleton, sampleClip(skeleton, AnimationClip{}, 0.f));
    for (size_t i = 0; i < skeleton.joints.size(); i++) {
        skeleton.joints[i].inverseBind = glm::inverse(restGlobal[i]);
    }

    std::vector<glm::mat4> skin = computeSkinMatrices(skeleton, sampleClip(skeleton, AnimationClip{}, 0.f));

    checkSameMatrix(skin[0], glm::mat4{1.f});
    checkSameMatrix(skin[1], glm::mat4{1.f});
}

TEST_CASE("blendPoses: no meio da transição fica no meio das duas poses") {
    JointPose a;
    a.translation = {0.f, 0.f, 0.f};
    JointPose b;
    b.translation = {2.f, 4.f, 0.f};
    b.rotation = glm::angleAxis(glm::radians(90.f), glm::vec3{0.f, 1.f, 0.f});

    std::vector<JointPose> half = blendPoses({a}, {b}, 0.5f);

    CHECK(half[0].translation.x == doctest::Approx(1.f));
    CHECK(half[0].translation.y == doctest::Approx(2.f));
    //metade do giro: 45 graus
    CHECK(glm::degrees(glm::angle(half[0].rotation)) == doctest::Approx(45.f));
    //nas pontas, cada uma das poses
    CHECK(blendPoses({a}, {b}, 0.f)[0].translation.x == doctest::Approx(0.f));
    CHECK(blendPoses({a}, {b}, 1.f)[0].translation.x == doctest::Approx(2.f));
}

TEST_CASE("animator.play: a anterior some aos poucos e a transição termina") {
    LveScene scene;
    LveGameObject &obj = scene.createGameObject();
    obj.animator.emplace();
    obj.animator->clip = "idle";
    obj.animator->time = 3.f;
    AnimatorSystem animatorSystem;

    obj.animator->play("walk", 0.2f);

    CHECK(obj.animator->clip == "walk");
    CHECK(obj.animator->time == doctest::Approx(0.f));
    CHECK(obj.animator->previousClip == "idle");
    CHECK(obj.animator->previousTime == doctest::Approx(3.f));
    CHECK(obj.animator->blendWeight() == doctest::Approx(0.f));

    animatorSystem.update(scene, 0.1f);
    CHECK(obj.animator->blendWeight() == doctest::Approx(0.5f));

    animatorSystem.update(scene, 0.15f);
    CHECK_FALSE(obj.animator->isBlending());
    CHECK(obj.animator->blendWeight() == doctest::Approx(1.f));
}

TEST_CASE("animator.play: pedir a mesma animação não recomeça nem mistura") {
    AnimatorComponent animator;
    animator.clip = "walk";
    animator.time = 0.7f;

    animator.play("walk", 0.2f);

    CHECK(animator.time == doctest::Approx(0.7f));
    CHECK_FALSE(animator.isBlending());
}

//cápsula com física + visual filho com as animações, como no loadGameObjects
static LveGameObject &addAnimatedCharacter(LveScene &scene, LveGameObject *&body) {
    body = &scene.createGameObject();
    body->rigidBody.emplace();
    body->character.emplace();
    LveGameObject &visual = scene.createGameObject();
    visual.parentId = body->getId();
    visual.animator.emplace();
    visual.characterAnimation.emplace();
    visual.characterAnimation->idle = "idle";
    visual.characterAnimation->walk = "walk";
    visual.characterAnimation->run = "run";
    visual.characterAnimation->fall = "fall";
    return visual;
}

TEST_CASE("CharacterAnimationSystem: parado, andando, correndo e no ar") {
    LveScene scene;
    LveGameObject *body = nullptr;
    LveGameObject &visual = addAnimatedCharacter(scene, body);
    CharacterAnimationSystem system;
    float walkSpeed = body->character->moveSpeed;

    body->rigidBody->onGround = true;
    system.update(scene);
    CHECK(visual.animator->clip == "idle");

    body->rigidBody->velocity = {walkSpeed, 0.f, 0.f};
    system.update(scene);
    CHECK(visual.animator->clip == "walk");
    CHECK(visual.animator->loop);

    body->rigidBody->velocity = {0.f, 0.f, body->character->runSpeed};
    system.update(scene);
    CHECK(visual.animator->clip == "run");

    //no ar: a queda, em loop até pousar
    body->rigidBody->onGround = false;
    system.update(scene);
    CHECK(visual.animator->clip == "fall");
    CHECK(visual.animator->loop);
}

TEST_CASE("animator: progress, frame e isFinished") {
    AnimatorComponent animator;
    animator.duration = 2.f;

    //com loop: 2.5 s numa animação de 2 s = 0.5 s da segunda volta
    animator.loop = true;
    animator.time = 2.5f;
    CHECK(animator.clipTime() == doctest::Approx(0.5f));
    CHECK(animator.progress() == doctest::Approx(0.25f));
    CHECK(animator.frame() == 15);
    CHECK(animator.frame(24.f) == 12);
    CHECK_FALSE(animator.isFinished());

    //sem loop: para no fim
    animator.loop = false;
    animator.time = 1.f;
    CHECK(animator.progress() == doctest::Approx(0.5f));
    CHECK_FALSE(animator.isFinished());
    animator.time = 2.5f;
    CHECK(animator.progress() == doctest::Approx(1.f));
    CHECK(animator.isFinished());

    //trocar de animação zera: a duração da nova ainda não é conhecida
    animator.play("outra", 0.2f, false);
    CHECK(animator.progress() == doctest::Approx(0.f));
    CHECK_FALSE(animator.isFinished());
}
