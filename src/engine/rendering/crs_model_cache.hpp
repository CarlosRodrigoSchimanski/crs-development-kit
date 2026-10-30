#pragma once

#include "engine/rendering/crs_model.hpp"

#include <map>
#include <memory>
#include <string>

namespace crsdk {
    //cada arquivo de modelo é carregado uma vez só; os objetos que usam o mesmo arquivo dividem a malha
    class CrsModelCache {
        public:
            explicit CrsModelCache(CrsDevice &device);

            CrsModelCache(const CrsModelCache&) = delete;
            CrsModelCache& operator=(const CrsModelCache&) = delete;

            std::shared_ptr<CrsModel> load(const std::string &filepath);

        private:
            CrsDevice &crsDevice;
            std::map<std::string, std::shared_ptr<CrsModel>> models;
    };
}
