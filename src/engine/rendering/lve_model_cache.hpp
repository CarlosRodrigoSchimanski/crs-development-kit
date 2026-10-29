#pragma once

#include "engine/rendering/lve_model.hpp"

#include <map>
#include <memory>
#include <string>

namespace lve {
    //cada arquivo de modelo é carregado uma vez só; os objetos que usam o mesmo arquivo dividem a malha
    class LveModelCache {
        public:
            explicit LveModelCache(LveDevice &device);

            LveModelCache(const LveModelCache&) = delete;
            LveModelCache& operator=(const LveModelCache&) = delete;

            std::shared_ptr<LveModel> load(const std::string &filepath);

        private:
            LveDevice &lveDevice;
            std::map<std::string, std::shared_ptr<LveModel>> models;
    };
}
