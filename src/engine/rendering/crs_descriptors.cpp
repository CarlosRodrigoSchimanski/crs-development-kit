#include "engine/rendering/crs_descriptors.hpp"

#include <cassert>
#include <stdexcept>

namespace crsdk {

    // *************** Descriptor Set Layout Builder *********************

    CrsDescriptorSetLayout::Builder::Builder(CrsDevice &crsDevice) : crsDevice{crsDevice} {}

    CrsDescriptorSetLayout::Builder &CrsDescriptorSetLayout::Builder::addBinding(
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

    std::unique_ptr<CrsDescriptorSetLayout> CrsDescriptorSetLayout::Builder::build() const {
        return std::make_unique<CrsDescriptorSetLayout>(crsDevice, bindings);
    }

    // *************** Descriptor Set Layout *********************

    CrsDescriptorSetLayout::CrsDescriptorSetLayout(
        CrsDevice &crsDevice, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings)
        : crsDevice{crsDevice}, bindings{std::move(bindings)} {
        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
        for (const auto &kv : this->bindings) {
            setLayoutBindings.push_back(kv.second);
        }

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
        descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
        descriptorSetLayoutInfo.pBindings = setLayoutBindings.data();

        VkDescriptorSetLayout setLayout;
        if (vkCreateDescriptorSetLayout(crsDevice.device(), &descriptorSetLayoutInfo, nullptr, &setLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
        descriptorSetLayout = CrsDescriptorSetLayoutHandle{crsDevice.device(), setLayout};
    }

    VkDescriptorSetLayout CrsDescriptorSetLayout::getDescriptorSetLayout() const { return descriptorSetLayout.get(); }

    // *************** Descriptor Pool Builder *********************

    CrsDescriptorPool::Builder::Builder(CrsDevice &crsDevice) : crsDevice{crsDevice} {}

    CrsDescriptorPool::Builder &CrsDescriptorPool::Builder::addPoolSize(
        VkDescriptorType descriptorType, uint32_t count) {
        poolSizes.push_back({descriptorType, count});
        return *this;
    }

    CrsDescriptorPool::Builder &CrsDescriptorPool::Builder::setMaxSets(uint32_t count) {
        maxSets = count;
        return *this;
    }

    CrsDescriptorPool::Builder &CrsDescriptorPool::Builder::allowFreeingSets() {
        flags |= VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        return *this;
    }

    std::unique_ptr<CrsDescriptorPool> CrsDescriptorPool::Builder::build() const {
        return std::make_unique<CrsDescriptorPool>(crsDevice, maxSets, poolSizes, flags);
    }

    // *************** Descriptor Pool *********************

    CrsDescriptorPool::CrsDescriptorPool(
        CrsDevice &crsDevice,
        uint32_t maxSets,
        const std::vector<VkDescriptorPoolSize> &poolSizes,
        VkDescriptorPoolCreateFlags flags)
        : crsDevice{crsDevice} {
        VkDescriptorPoolCreateInfo descriptorPoolInfo{};
        descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolInfo.flags = flags;
        descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        descriptorPoolInfo.pPoolSizes = poolSizes.data();
        descriptorPoolInfo.maxSets = maxSets;

        VkDescriptorPool pool;
        if (vkCreateDescriptorPool(crsDevice.device(), &descriptorPoolInfo, nullptr, &pool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool!");
        }
        descriptorPool = CrsDescriptorPoolHandle{crsDevice.device(), pool};
    }

    bool CrsDescriptorPool::allocateDescriptor(
        VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet &descriptor) const {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool.get();
        allocInfo.pSetLayouts = &descriptorSetLayout;
        allocInfo.descriptorSetCount = 1;

        //poderia criar uma classe "DescriptorPoolManager" pra esse caso, que cria um novo pool
        //quando um pool antigo enche, mas isso está fora do escopo atual
        return vkAllocateDescriptorSets(crsDevice.device(), &allocInfo, &descriptor) == VK_SUCCESS;
    }

    void CrsDescriptorPool::freeDescriptor(VkDescriptorSet descriptor) const {
        vkFreeDescriptorSets(crsDevice.device(), descriptorPool.get(), 1, &descriptor);
    }

    // *************** Descriptor Writer *********************

    CrsDescriptorWriter::CrsDescriptorWriter(CrsDescriptorSetLayout &setLayout, CrsDescriptorPool &pool)
        : setLayout{setLayout}, pool{pool} {}
    
    VkWriteDescriptorSet CrsDescriptorWriter::makeWrite(uint32_t binding, uint32_t count) const {
        assert(setLayout.bindings.count(binding) == 1 && "Layout does not contain specified binding");
        //.at() e não [] : o [] CRIA a entrada se ela não existir, e o assert acima some no build de
        //release; o .at() lança exceção nos dois builds
        const auto &bindingDescription = setLayout.bindings.at(binding);

        assert(
            bindingDescription.descriptorCount == count &&
            "Number of descriptor infos doesn't match the binding's descriptor count");

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.descriptorType = bindingDescription.descriptorType;
        write.dstBinding = binding;
        write.descriptorCount = count;
        return write;
    }

    CrsDescriptorWriter &CrsDescriptorWriter::writeImage(
        uint32_t binding, VkDescriptorImageInfo *imageInfo) {
        VkWriteDescriptorSet write = makeWrite(binding);
        write.pImageInfo = imageInfo;
        writes.push_back(write);
        return *this;
    }

    CrsDescriptorWriter &CrsDescriptorWriter::writeImages(
        uint32_t binding, VkDescriptorImageInfo *imageInfos, uint32_t count) {
        VkWriteDescriptorSet write = makeWrite(binding, count);
        write.pImageInfo = imageInfos;
        writes.push_back(write);
        return *this;
    }

    CrsDescriptorWriter &CrsDescriptorWriter::writeBuffer(
        uint32_t binding, VkDescriptorBufferInfo *bufferInfo) {
        VkWriteDescriptorSet write = makeWrite(binding);
        write.pBufferInfo = bufferInfo;
        writes.push_back(write);
        return *this;
    }

    bool CrsDescriptorWriter::build(VkDescriptorSet &set) {
        bool success = pool.allocateDescriptor(setLayout.getDescriptorSetLayout(), set);
        if (!success) {
            return false;
        }
        overwrite(set);
        return true;
    }

    void CrsDescriptorWriter::overwrite(VkDescriptorSet set) {
        for (auto &write : writes) {
            write.dstSet = set;
        }
        vkUpdateDescriptorSets(pool.crsDevice.device(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

}
