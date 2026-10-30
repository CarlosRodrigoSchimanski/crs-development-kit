#include "engine/rendering/crs_model_cache.hpp"

namespace crsdk {
    CrsModelCache::CrsModelCache(CrsDevice &device) : crsDevice{device} {}

    std::shared_ptr<CrsModel> CrsModelCache::load(const std::string &filepath) {
        //já carregado: devolve o mesmo
        auto found = models.find(filepath);
        if (found != models.end()) return found->second;

        std::shared_ptr<CrsModel> model = CrsModel::createModelFromFile(crsDevice, filepath);
        models[filepath] = model;
        return model;
    }
}
