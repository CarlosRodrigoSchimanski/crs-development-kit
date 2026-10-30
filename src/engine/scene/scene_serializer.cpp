#include "engine/scene/scene_serializer.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace crsdk {
    //ordered_json: as chaves ficam na ordem em que foram escritas (id e nome primeiro)
    using json = nlohmann::ordered_json;

    //versão do formato: muda quando o arquivo muda de jeito (2 = com os componentes)
    constexpr int SCENE_VERSION = 2;

    //número do arquivo: 6 casas (1.6 em vez de 1.600000023841858, que é o float visto como double)
    static double rounded(float value) {
        return std::round(static_cast<double>(value) * 1e6) / 1e6;
    }

    //vec3 vira a lista [x, y, z]
    static json vec3ToJson(const glm::vec3 &v) {
        return json::array({rounded(v.x), rounded(v.y), rounded(v.z)});
    }

    //a lista [x, y, z] volta a ser vec3
    static glm::vec3 vec3FromJson(const json &j) {
        return {j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>()};
    }

    //vec3 de um campo; campo que falta fica com o padrão
    static glm::vec3 vec3Or(const json &j, const char *key, const glm::vec3 &fallback) {
        return j.contains(key) ? vec3FromJson(j.at(key)) : fallback;
    }

    //os componentes do objeto: só as configurações (velocidade, tempo da animação... recomeçam ao carregar)
    static void componentsToJson(json &j, const CrsGameObject &obj) {
        if (obj.pointLight) {
            j["pointLight"] = {{"intensity", rounded(obj.pointLight->lightIntensity)}};
        }
        if (obj.camera) {
            j["camera"] = {
                {"fovY", rounded(obj.camera->fovY)},
                {"nearPlane", rounded(obj.camera->nearPlane)},
                {"farPlane", rounded(obj.camera->farPlane)},
            };
        }
        if (obj.rigidBody) {
            j["rigidBody"] = {
                {"type", obj.rigidBody->type == BodyType::Static ? "static" : "dynamic"},
                {"halfExtents", vec3ToJson(obj.rigidBody->halfExtents)},
                {"center", vec3ToJson(obj.rigidBody->center)},
            };
        }
        if (obj.character) {
            j["character"] = {
                {"moveSpeed", rounded(obj.character->moveSpeed)},
                {"runSpeed", rounded(obj.character->runSpeed)},
                {"jumpSpeed", rounded(obj.character->jumpSpeed)},
            };
        }
        if (obj.cameraRig) {
            j["cameraRig"] = {
                {"sensitivity", rounded(obj.cameraRig->sensitivity)},
                {"minPitch", rounded(obj.cameraRig->minPitch)},
                {"maxPitch", rounded(obj.cameraRig->maxPitch)},
                {"distance", rounded(obj.cameraRig->distance)},
                {"wallMargin", rounded(obj.cameraRig->wallMargin)},
            };
        }
        if (obj.faceMovement) {
            j["faceMovement"] = {
                {"turnSpeed", rounded(obj.faceMovement->turnSpeed)},
                {"yawOffset", rounded(obj.faceMovement->yawOffset)},
            };
        }
        if (obj.characterAnimation) {
            j["characterAnimation"] = {
                {"idle", obj.characterAnimation->idle},
                {"walk", obj.characterAnimation->walk},
                {"run", obj.characterAnimation->run},
                {"fall", obj.characterAnimation->fall},
                {"fadeTime", rounded(obj.characterAnimation->fadeTime)},
            };
        }
        if (obj.animator) {
            j["animator"] = {
                {"clip", obj.animator->clip},
                {"animations", obj.animator->animations},
                {"speed", rounded(obj.animator->speed)},
                {"loop", obj.animator->loop},
            };
        }
    }

    //o caminho inverso; campo que falta fica com o valor padrão do componente
    static void componentsFromJson(const json &j, CrsGameObject &obj) {
        if (j.contains("pointLight")) {
            const json &c = j.at("pointLight");
            PointLightComponent light{};
            light.lightIntensity = c.value("intensity", light.lightIntensity);
            obj.pointLight = light;
        }
        if (j.contains("camera")) {
            const json &c = j.at("camera");
            CameraComponent camera{};
            camera.fovY = c.value("fovY", camera.fovY);
            camera.nearPlane = c.value("nearPlane", camera.nearPlane);
            camera.farPlane = c.value("farPlane", camera.farPlane);
            obj.camera = camera;
        }
        if (j.contains("rigidBody")) {
            const json &c = j.at("rigidBody");
            RigidBodyComponent body{};
            body.type = c.value("type", std::string{"dynamic"}) == "static" ? BodyType::Static : BodyType::Dynamic;
            body.halfExtents = vec3Or(c, "halfExtents", body.halfExtents);
            body.center = vec3Or(c, "center", body.center);
            obj.rigidBody = body;
        }
        if (j.contains("character")) {
            const json &c = j.at("character");
            CharacterComponent character{};
            character.moveSpeed = c.value("moveSpeed", character.moveSpeed);
            character.runSpeed = c.value("runSpeed", character.runSpeed);
            character.jumpSpeed = c.value("jumpSpeed", character.jumpSpeed);
            obj.character = character;
        }
        if (j.contains("cameraRig")) {
            const json &c = j.at("cameraRig");
            CameraRigComponent rig{};
            rig.sensitivity = c.value("sensitivity", rig.sensitivity);
            rig.minPitch = c.value("minPitch", rig.minPitch);
            rig.maxPitch = c.value("maxPitch", rig.maxPitch);
            rig.distance = c.value("distance", rig.distance);
            rig.wallMargin = c.value("wallMargin", rig.wallMargin);
            obj.cameraRig = rig;
        }
        if (j.contains("faceMovement")) {
            const json &c = j.at("faceMovement");
            FaceMovementComponent face{};
            face.turnSpeed = c.value("turnSpeed", face.turnSpeed);
            face.yawOffset = c.value("yawOffset", face.yawOffset);
            obj.faceMovement = face;
        }
        if (j.contains("characterAnimation")) {
            const json &c = j.at("characterAnimation");
            CharacterAnimationComponent anims{};
            anims.idle = c.value("idle", anims.idle);
            anims.walk = c.value("walk", anims.walk);
            anims.run = c.value("run", anims.run);
            anims.fall = c.value("fall", anims.fall);
            anims.fadeTime = c.value("fadeTime", anims.fadeTime);
            obj.characterAnimation = anims;
        }
        if (j.contains("animator")) {
            const json &c = j.at("animator");
            AnimatorComponent animator{};
            animator.clip = c.value("clip", animator.clip);
            animator.animations = c.value("animations", animator.animations);
            animator.speed = c.value("speed", animator.speed);
            animator.loop = c.value("loop", animator.loop);
            obj.animator = animator;
        }
    }

    //um objeto da cena: o básico e os componentes
    static json objectToJson(const CrsGameObject &obj) {
        json j;
        j["id"] = obj.getId();
        j["name"] = obj.name;
        if (obj.model && !obj.model->getFilePath().empty()) {
            j["model"] = obj.model->getFilePath();
        }
        if (obj.parentId) {
            j["parent"] = *obj.parentId;
        }
        j["editorOnly"] = obj.editorOnly;
        j["color"] = vec3ToJson(obj.color);
        j["useTexture"] = obj.useTexture;
        j["transform"] = {
            {"translation", vec3ToJson(obj.transform.translation)},
            {"rotation", vec3ToJson(obj.transform.rotation)},
            {"scale", vec3ToJson(obj.transform.scale)},
        };
        componentsToJson(j, obj);
        return j;
    }

    void saveScene(const CrsScene &scene, const std::string &filepath) {
        //em ordem de id: o arquivo não muda de ordem a cada salvar (o git mostra só o que mudou)
        std::vector<const CrsGameObject *> sorted;
        for (const auto &kv : scene.getGameObjects()) {
            sorted.push_back(&kv.second);
        }
        std::sort(sorted.begin(), sorted.end(),
            [](const CrsGameObject *a, const CrsGameObject *b) { return a->getId() < b->getId(); });

        json objects = json::array();
        for (const CrsGameObject *obj : sorted) {
            objects.push_back(objectToJson(*obj));
        }

        json file;
        file["version"] = SCENE_VERSION;
        if (auto camera = scene.getActiveCamera()) {
            file["activeCamera"] = *camera;
        }
        file["objects"] = objects;

        //cria a pasta se ainda não existe (ex: scenes/)
        std::filesystem::path path{filepath};
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }
        std::ofstream out{path};
        if (!out) {
            throw std::runtime_error("não consegui abrir o arquivo pra gravar: " + filepath);
        }
        out << file.dump(4);  //4 = indentação: fica legível
    }

    //um objeto do arquivo, criado na cena com o mesmo id
    static void objectFromJson(CrsScene &scene, const json &j, const ModelLoader &loadModel) {
        CrsGameObject &obj = scene.createGameObject(j.at("id").get<CrsGameObject::id_t>());
        obj.name = j.value("name", "");
        if (j.contains("model")) {
            obj.model = loadModel(j["model"].get<std::string>());
        }
        if (j.contains("parent")) {
            obj.parentId = j["parent"].get<CrsGameObject::id_t>();
        }
        obj.editorOnly = j.value("editorOnly", false);
        obj.color = vec3Or(j, "color", obj.color);
        obj.useTexture = j.value("useTexture", obj.useTexture);

        const json &transform = j.at("transform");
        obj.transform.translation = vec3FromJson(transform.at("translation"));
        obj.transform.rotation = vec3FromJson(transform.at("rotation"));
        obj.transform.scale = vec3FromJson(transform.at("scale"));

        componentsFromJson(j, obj);
    }

    CrsScene loadScene(const std::string &filepath, const ModelLoader &loadModel) {
        std::ifstream in{filepath};
        if (!in) {
            throw std::runtime_error("não consegui abrir a cena: " + filepath);
        }

        //arquivo quebrado ou campo faltando: o nlohmann lança exceção, quem chamou decide o que fazer
        json file = json::parse(in);
        int version = file.value("version", 0);
        if (version != SCENE_VERSION) {
            throw std::runtime_error("versão de cena desconhecida (" + std::to_string(version) + "): " + filepath);
        }

        CrsScene scene;
        for (const json &j : file.at("objects")) {
            objectFromJson(scene, j, loadModel);
        }
        if (file.contains("activeCamera")) {
            scene.setActiveCamera(file["activeCamera"].get<CrsGameObject::id_t>());
        }
        return scene;
    }
}
