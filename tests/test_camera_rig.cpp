#include "engine/gameplay/camera_rig_system.hpp"

#include <doctest/doctest.h>

using namespace lve;

//personagem (com colisor) > pivô (rig) > câmera, como no loadGameObjects; o pivô olha pra -Z,
//então a câmera fica no +Z dele
struct RigScene {
    LveScene scene;
    LveGameObject *player = nullptr;
    LveGameObject *pivot = nullptr;
    LveGameObject *camera = nullptr;

    RigScene() {
        player = &scene.createGameObject();
        player->rigidBody.emplace();  //o raio sai de dentro desta caixa
        pivot = &scene.createGameObject();
        pivot->parentId = player->getId();
        pivot->cameraRig.emplace();
        camera = &scene.createGameObject();
        camera->parentId = pivot->getId();
        camera->camera.emplace();
    }
};

TEST_CASE("rig sem nada no caminho: câmera na distância cheia") {
    RigScene rig;
    CameraRigSystem system;

    system.placeCamera(rig.scene);

    CHECK(rig.camera->transform.translation.z == doctest::Approx(4.f));
}

TEST_CASE("rig com parede entre o pivô e a câmera: a câmera fica antes dela") {
    RigScene rig;
    //parede de lado 2 centrada em z = 3: a face de frente pro pivô fica em z = 2
    LveGameObject &wall = rig.scene.createGameObject();
    wall.transform.translation = {0.f, 0.f, 3.f};
    wall.rigidBody.emplace();
    wall.rigidBody->type = BodyType::Static;
    CameraRigSystem system;

    system.placeCamera(rig.scene);

    //raio bate a 2 do pivô, menos a folga de 0.2
    CHECK(rig.camera->transform.translation.z == doctest::Approx(1.8f));
}

TEST_CASE("rig: a cápsula do próprio personagem não puxa a câmera") {
    //o raio sai de DENTRO da caixa do personagem: sem ignorar o pai, a câmera grudaria no pivô
    RigScene rig;
    rig.player->rigidBody->halfExtents = {1.f, 1.f, 1.f};
    CameraRigSystem system;

    system.placeCamera(rig.scene);

    CHECK(rig.camera->transform.translation.z == doctest::Approx(4.f));
}
