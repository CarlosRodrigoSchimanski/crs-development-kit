#include "engine/rendering/lve_buffer.hpp"

#include <cassert>
#include <cstring>

namespace lve {

    VkDeviceSize LveBuffer::getAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment) {
        if (minOffsetAlignment > 0) {
            return (instanceSize + minOffsetAlignment - 1) & ~(minOffsetAlignment - 1);
        }
        return instanceSize;
    }

    LveBuffer::LveBuffer(
        LveDevice &device,
        VkDeviceSize instanceSize,
        uint32_t instanceCount,
        VkBufferUsageFlags usageFlags,
        VkMemoryPropertyFlags memoryPropertyFlags,
        VkDeviceSize minOffsetAlignment)
        : lveDevice{device} {
        VkDeviceSize alignmentSize = getAlignment(instanceSize, minOffsetAlignment);
        bufferSize = alignmentSize * instanceCount;
        device.createBuffer(bufferSize, usageFlags, memoryPropertyFlags, buffer, memory);
    }

    LveBuffer::~LveBuffer() {
        unmap();
        vkDestroyBuffer(lveDevice.device(), buffer, nullptr);
        vkFreeMemory(lveDevice.device(), memory, nullptr);
    }

    //mapeia um intervalo de memória deste buffer; se der certo, "mapped" passa a apontar pra ele
    VkResult LveBuffer::map(VkDeviceSize size, VkDeviceSize offset) {
        assert(buffer && memory && "Called map on buffer before create");
        return vkMapMemory(lveDevice.device(), memory, offset, size, 0, &mapped);
    }

    //desmapeia o intervalo mapeado (vkUnmapMemory não tem como falhar, por isso não retorna nada)
    void LveBuffer::unmap() {
        if (mapped) {
            vkUnmapMemory(lveDevice.device(), memory);
            mapped = nullptr;
        }
    }

    //copia os dados pro buffer já mapeado; por padrão copia o buffer inteiro
    void LveBuffer::writeToBuffer(const void *data, VkDeviceSize size, VkDeviceSize offset) {
        assert(mapped && "Cannot copy to unmapped buffer");

        if (size == VK_WHOLE_SIZE) {
            memcpy(mapped, data, bufferSize);
        } else {
            //char* pra poder somar o offset em bytes (aritmética com void* não existe em C++)
            memcpy(static_cast<char *>(mapped) + offset, data, size);
        }
    }

    //só necessário pra memória não-coerente: avisa a GPU que a CPU escreveu nesse intervalo
    VkResult LveBuffer::flush(VkDeviceSize size, VkDeviceSize offset) const {
        VkMappedMemoryRange mappedRange{};
        mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        mappedRange.memory = memory;
        mappedRange.offset = offset;
        mappedRange.size = size;
        return vkFlushMappedMemoryRanges(lveDevice.device(), 1, &mappedRange);
    }

    VkDescriptorBufferInfo LveBuffer::descriptorInfo(VkDeviceSize size, VkDeviceSize offset) const {
        return VkDescriptorBufferInfo{
            buffer,
            offset,
            size,
        };
    }

    VkBuffer LveBuffer::getBuffer() const { return buffer; }

}
