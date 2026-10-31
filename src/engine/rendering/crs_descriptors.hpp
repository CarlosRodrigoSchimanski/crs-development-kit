#pragma once

#include "engine/core/crs_device.hpp"

#include <memory>
#include <unordered_map>
#include <vector>

namespace crsdk {

    class CrsDescriptorSetLayout {
        public:
            class Builder {
                public:
                    Builder(CrsDevice &crsDevice);

                    Builder &addBinding(
                        uint32_t binding,
                        VkDescriptorType descriptorType,
                        VkShaderStageFlags stageFlags,
                        uint32_t count = 1);
                    std::unique_ptr<CrsDescriptorSetLayout> build() const;

                private:
                    CrsDevice &crsDevice;
                    std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings{};
            };

            CrsDescriptorSetLayout(
                CrsDevice &crsDevice, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings);
            CrsDescriptorSetLayout(const CrsDescriptorSetLayout &) = delete;
            CrsDescriptorSetLayout &operator=(const CrsDescriptorSetLayout &) = delete;

            VkDescriptorSetLayout getDescriptorSetLayout() const;

        private:
            CrsDevice &crsDevice;
            CrsDescriptorSetLayoutHandle descriptorSetLayout;
            std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings;

            friend class CrsDescriptorWriter;
    };

    class CrsDescriptorPool {
        public:
            class Builder {
                public:
                    Builder(CrsDevice &crsDevice);

                    Builder &addPoolSize(VkDescriptorType descriptorType, uint32_t count);
                    Builder &setMaxSets(uint32_t count);
                    std::unique_ptr<CrsDescriptorPool> build() const;

                private:
                    CrsDevice &crsDevice;
                    std::vector<VkDescriptorPoolSize> poolSizes{};
                    uint32_t maxSets = 1000;
            };

            CrsDescriptorPool(
                CrsDevice &crsDevice,
                uint32_t maxSets,
                const std::vector<VkDescriptorPoolSize> &poolSizes);
            CrsDescriptorPool(const CrsDescriptorPool &) = delete;
            CrsDescriptorPool &operator=(const CrsDescriptorPool &) = delete;

            bool allocateDescriptor(VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet &descriptor) const;

        private:
            CrsDevice &crsDevice;
            CrsDescriptorPoolHandle descriptorPool;

            friend class CrsDescriptorWriter;
    };

    class CrsDescriptorWriter {
        public:
            CrsDescriptorWriter(CrsDescriptorSetLayout &setLayout, CrsDescriptorPool &pool);

            CrsDescriptorWriter &writeImage(uint32_t binding, VkDescriptorImageInfo *imageInfo);
            //binding que é um array (sampler2D mapas[8]): 'count' imagens seguidas, todas de uma vez
            CrsDescriptorWriter &writeImages(uint32_t binding, VkDescriptorImageInfo *imageInfos, uint32_t count);
            CrsDescriptorWriter &writeBuffer(uint32_t binding, VkDescriptorBufferInfo *bufferInfo);

            bool build(VkDescriptorSet &set);
            void overwrite(VkDescriptorSet set);

        private:
            //parte comum de writeImage/writeBuffer: valida o binding e monta o VkWriteDescriptorSet
            VkWriteDescriptorSet makeWrite(uint32_t binding, uint32_t count = 1) const;

            CrsDescriptorSetLayout &setLayout;
            CrsDescriptorPool &pool;
            std::vector<VkWriteDescriptorSet> writes;
    };

}
