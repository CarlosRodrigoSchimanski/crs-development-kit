#include "engine/rendering/crs_buffer.hpp"
#include "engine/core/crs_vk_check.hpp"

#include <vk_mem_alloc.h>

#include <cassert>
#include <cstring>
#include <stdexcept>

namespace crsdk {

    VkDeviceSize CrsBuffer::getAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment) {
        if (minOffsetAlignment > 0) {
            return (instanceSize + minOffsetAlignment - 1) & ~(minOffsetAlignment - 1);
        }
        return instanceSize;
    }

    CrsBuffer::CrsBuffer(
        CrsDevice &device,
        VkDeviceSize instanceSize,
        uint32_t instanceCount,
        VkBufferUsageFlags usageFlags,
        VkMemoryPropertyFlags memoryPropertyFlags,
        VkDeviceSize minOffsetAlignment)
        : crsDevice{device} {
        VkDeviceSize alignmentSize = getAlignment(instanceSize, minOffsetAlignment);
        bufferSize = alignmentSize * instanceCount;

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = usageFlags;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        //as mesmas flags de antes (DEVICE_LOCAL, HOST_VISIBLE...): a VMA acha o tipo de memória
        //que tem todas elas e encaixa o buffer num bloco dela
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.requiredFlags = memoryPropertyFlags;

        //cria o buffer, aloca e liga a memória, tudo numa chamada
        if (vmaCreateBuffer(crsDevice.allocator(), &bufferInfo, &allocInfo, &buffer, &allocation, nullptr) != VK_SUCCESS) {
            throw std::runtime_error("failed to create buffer!");
        }
    }

    CrsBuffer::~CrsBuffer() {
        unmap();
        //destrói o buffer e devolve o pedaço de memória pra VMA
        vmaDestroyBuffer(crsDevice.allocator(), buffer, allocation);
    }

    //se der certo, "mapped" passa a apontar pro começo do buffer
    void CrsBuffer::map() {
        assert(buffer && allocation && "Called map on buffer before create");
        vkCheck(vmaMapMemory(crsDevice.allocator(), allocation, &mapped), "map de buffer");
    }

    void CrsBuffer::unmap() {
        if (mapped) {
            vmaUnmapMemory(crsDevice.allocator(), allocation);
            mapped = nullptr;
        }
    }

    //copia os dados pro buffer já mapeado; por padrão copia o buffer inteiro
    void CrsBuffer::writeToBuffer(const void *data, VkDeviceSize size, VkDeviceSize offset) {
        assert(mapped && "Cannot copy to unmapped buffer");

        if (size == VK_WHOLE_SIZE) {
            memcpy(mapped, data, bufferSize);
        } else {
            //char* pra poder somar o offset em bytes (aritmética com void* não existe em C++)
            memcpy(static_cast<char *>(mapped) + offset, data, size);
        }
    }

    //só necessário pra memória não-coerente: avisa a GPU que a CPU escreveu nesse intervalo
    //(em memória coerente a VMA simplesmente não faz nada)
    void CrsBuffer::flush(VkDeviceSize size, VkDeviceSize offset) const {
        vkCheck(vmaFlushAllocation(crsDevice.allocator(), allocation, offset, size), "flush de buffer");
    }

    VkDescriptorBufferInfo CrsBuffer::descriptorInfo(VkDeviceSize size, VkDeviceSize offset) const {
        return VkDescriptorBufferInfo{
            buffer,
            offset,
            size,
        };
    }

    VkBuffer CrsBuffer::getBuffer() const { return buffer; }
    VkDeviceSize CrsBuffer::getBufferSize() const { return bufferSize; }

}
