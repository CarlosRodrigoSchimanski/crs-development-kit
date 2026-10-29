#include "engine/rendering/lve_model_cache.hpp"

namespace lve {
    LveModelCache::LveModelCache(LveDevice &device) : lveDevice{device} {}

    std::shared_ptr<LveModel> LveModelCache::load(const std::string &filepath) {
        //já carregado: devolve o mesmo
        auto found = models.find(filepath);
        if (found != models.end()) return found->second;

        std::shared_ptr<LveModel> model = LveModel::createModelFromFile(lveDevice, filepath);
        models[filepath] = model;
        return model;
    }
}
