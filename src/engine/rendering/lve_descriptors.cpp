#include "engine/rendering/lve_descriptors.hpp"

#include <cassert>
#include <stdexcept>

namespace lve {

    // *************** Descriptor Set Layout Builder *********************

    LveDescriptorSetLayout::Builder::Builder(LveDevice &lveDevice) : lveDevice{lveDevice} {}

    LveDescriptorSetLayout::Builder &LveDescriptorSetLayout::Builder::addBinding(
        uint32_t binding,
        VkDescriptorType descriptorType,
        VkShaderStageFlags stageFlags,
        uint32_t count) {
        assert(bindings.count(binding) == 0 && "Binding already in use");
        VkDescriptorSetLayoutBinding layoutBinding{};
        layoutBinding.binding = binding;
        layoutBinding.descriptorType = descriptorType;
        layoutBinding.descriptorCount = count;
        layoutBinding.stageFlags = stageFlags;
        bindings[binding] = layoutBinding;
        return *this;
    }

    std::unique_ptr<LveDescriptorSetLayout> LveDescriptorSetLayout::Builder::build() const {
        return std::make_unique<LveDescriptorSetLayout>(lveDevice, bindings);
    }

    // *************** Descriptor Set Layout *********************

    LveDescriptorSetLayout::LveDescriptorSetLayout(
        LveDevice &lveDevice, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings)
        : lveDevice{lveDevice}, bindings{std::move(bindings)} {
        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
        for (const auto &kv : this->bindings) {
            setLayoutBindings.push_back(kv.second);
        }

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
        descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
        descriptorSetLayoutInfo.pBindings = setLayoutBindings.data();

        VkDescriptorSetLayout setLayout;
        if (vkCreateDescriptorSetLayout(lveDevice.device(), &descriptorSetLayoutInfo, nullptr, &setLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
        descriptorSetLayout = LveDescriptorSetLayoutHandle{lveDevice.device(), setLayout};
    }

    VkDescriptorSetLayout LveDescriptorSetLayout::getDescriptorSetLayout() const { return descriptorSetLayout.get(); }

    // *************** Descriptor Pool Builder *********************

    LveDescriptorPool::Builder::Builder(LveDevice &lveDevice) : lveDevice{lveDevice} {}

    LveDescriptorPool::Builder &LveDescriptorPool::Builder::addPoolSize(
        VkDescriptorType descriptorType, uint32_t count) {
        poolSizes.push_back({descriptorType, count});
        return *this;
    }

    LveDescriptorPool::Builder &LveDescriptorPool::Builder::setMaxSets(uint32_t count) {
        maxSets = count;
        return *this;
    }

    std::unique_ptr<LveDescriptorPool> LveDescriptorPool::Builder::build() const {
        return std::make_unique<LveDescriptorPool>(lveDevice, maxSets, poolSizes);
    }

    // *************** Descriptor Pool *********************

    LveDescriptorPool::LveDescriptorPool(
        LveDevice &lveDevice,
        uint32_t maxSets,
        const std::vector<VkDescriptorPoolSize> &poolSizes)
        : lveDevice{lveDevice} {
        VkDescriptorPoolCreateInfo descriptorPoolInfo{};
        descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        descriptorPoolInfo.pPoolSizes = poolSizes.data();
        descriptorPoolInfo.maxSets = maxSets;

        VkDescriptorPool pool;
        if (vkCreateDescriptorPool(lveDevice.device(), &descriptorPoolInfo, nullptr, &pool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool!");
        }
        descriptorPool = LveDescriptorPoolHandle{lveDevice.device(), pool};
    }

    bool LveDescriptorPool::allocateDescriptor(
        VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet &descriptor) const {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool.get();
        allocInfo.pSetLayouts = &descriptorSetLayout;
        allocInfo.descriptorSetCount = 1;

        //poderia criar uma classe "DescriptorPoolManager" pra esse caso, que cria um novo pool
        //quando um pool antigo enche, mas isso está fora do escopo atual
        return vkAllocateDescriptorSets(lveDevice.device(), &allocInfo, &descriptor) == VK_SUCCESS;
    }

    // *************** Descriptor Writer *********************

    LveDescriptorWriter::LveDescriptorWriter(LveDescriptorSetLayout &setLayout, LveDescriptorPool &pool)
        : setLayout{setLayout}, pool{pool} {}
    
    VkWriteDescriptorSet LveDescriptorWriter::makeWrite(uint32_t binding) const {
        assert(setLayout.bindings.count(binding) == 1 && "Layout does not contain specified binding");
        //.at() e não [] : o [] CRIA a entrada se ela não existir, e o assert acima some no build de
        //release; o .at() lança exceção nos dois builds
        const auto &bindingDescription = setLayout.bindings.at(binding);

        assert(
            bindingDescription.descriptorCount == 1 &&
            "Binding single descriptor info, but binding expects multiple");

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.descriptorType = bindingDescription.descriptorType;
        write.dstBinding = binding;
        write.descriptorCount = 1;
        return write;
    }

    LveDescriptorWriter &LveDescriptorWriter::writeImage(
        uint32_t binding, VkDescriptorImageInfo *imageInfo) {
        VkWriteDescriptorSet write = makeWrite(binding);
        write.pImageInfo = imageInfo;
        writes.push_back(write);
        return *this;
    }

    LveDescriptorWriter &LveDescriptorWriter::writeBuffer(
        uint32_t binding, VkDescriptorBufferInfo *bufferInfo) {
        VkWriteDescriptorSet write = makeWrite(binding);
        write.pBufferInfo = bufferInfo;
        writes.push_back(write);
        return *this;
    }

    bool LveDescriptorWriter::build(VkDescriptorSet &set) {
        bool success = pool.allocateDescriptor(setLayout.getDescriptorSetLayout(), set);
        if (!success) {
            return false;
        }
        overwrite(set);
        return true;
    }

    void LveDescriptorWriter::overwrite(VkDescriptorSet set) {
        for (auto &write : writes) {
            write.dstSet = set;
        }
        vkUpdateDescriptorSets(pool.lveDevice.device(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

}
