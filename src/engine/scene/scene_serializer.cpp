#include "engine/scene/scene_serializer.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace lve {
    //ordered_json: as chaves ficam na ordem em que foram escritas (id e nome primeiro)
    using json = nlohmann::ordered_json;

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

    //um objeto da cena: o básico (os componentes entram depois, um por vez)
    static json objectToJson(const LveGameObject &obj) {
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
        j["transform"] = {
            {"translation", vec3ToJson(obj.transform.translation)},
            {"rotation", vec3ToJson(obj.transform.rotation)},
            {"scale", vec3ToJson(obj.transform.scale)},
        };
        return j;
    }

    void saveScene(const LveScene &scene, const std::string &filepath) {
        //em ordem de id: o arquivo não muda de ordem a cada salvar (o git mostra só o que mudou)
        std::vector<const LveGameObject *> sorted;
        for (const auto &kv : scene.getGameObjects()) {
            sorted.push_back(&kv.second);
        }
        std::sort(sorted.begin(), sorted.end(),
            [](const LveGameObject *a, const LveGameObject *b) { return a->getId() < b->getId(); });

        json objects = json::array();
        for (const LveGameObject *obj : sorted) {
            objects.push_back(objectToJson(*obj));
        }

        json file;
        file["version"] = 1;
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
    static void objectFromJson(LveScene &scene, const json &j, const ModelLoader &loadModel) {
        LveGameObject &obj = scene.createGameObject(j.at("id").get<LveGameObject::id_t>());
        obj.name = j.value("name", "");
        if (j.contains("model")) {
            obj.model = loadModel(j["model"].get<std::string>());
        }
        if (j.contains("parent")) {
            obj.parentId = j["parent"].get<LveGameObject::id_t>();
        }
        obj.editorOnly = j.value("editorOnly", false);

        const json &transform = j.at("transform");
        obj.transform.translation = vec3FromJson(transform.at("translation"));
        obj.transform.rotation = vec3FromJson(transform.at("rotation"));
        obj.transform.scale = vec3FromJson(transform.at("scale"));
    }

    LveScene loadScene(const std::string &filepath, const ModelLoader &loadModel) {
        std::ifstream in{filepath};
        if (!in) {
            throw std::runtime_error("não consegui abrir a cena: " + filepath);
        }

        //arquivo quebrado ou campo faltando: o nlohmann lança exceção, quem chamou decide o que fazer
        json file = json::parse(in);
        int version = file.value("version", 0);
        if (version != 1) {
            throw std::runtime_error("versão de cena desconhecida (" + std::to_string(version) + "): " + filepath);
        }

        LveScene scene;
        for (const json &j : file.at("objects")) {
            objectFromJson(scene, j, loadModel);
        }
        if (file.contains("activeCamera")) {
            scene.setActiveCamera(file["activeCamera"].get<LveGameObject::id_t>());
        }
        return scene;
    }
}
