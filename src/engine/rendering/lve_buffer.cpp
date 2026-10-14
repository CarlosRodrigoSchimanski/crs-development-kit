#include "engine/rendering/lve_buffer.hpp"

#include <vk_mem_alloc.h>

#include <cassert>
#include <cstring>
#include <stdexcept>

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

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = usageFlags;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        //as mesmas flags de antes (DEVICE_LOCAL, HOST_VISIBLE...): a VMA acha o tipo de memória
        //que tem todas elas (o que o findMemoryType fazia) e encaixa o buffer num bloco dela
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.requiredFlags = memoryPropertyFlags;

        //cria o buffer, aloca e liga a memória, tudo numa chamada
        if (vmaCreateBuffer(lveDevice.allocator(), &bufferInfo, &allocInfo, &buffer, &allocation, nullptr) != VK_SUCCESS) {
            throw std::runtime_error("failed to create buffer!");
        }
    }

    LveBuffer::~LveBuffer() {
        unmap();
        //destrói o buffer e devolve o pedaço de memória pra VMA
        vmaDestroyBuffer(lveDevice.allocator(), buffer, allocation);
    }

    //se der certo, "mapped" passa a apontar pro começo do buffer
    VkResult LveBuffer::map() {
        assert(buffer && allocation && "Called map on buffer before create");
        return vmaMapMemory(lveDevice.allocator(), allocation, &mapped);
    }

    void LveBuffer::unmap() {
        if (mapped) {
            vmaUnmapMemory(lveDevice.allocator(), allocation);
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
    //(em memória coerente a VMA simplesmente não faz nada)
    VkResult LveBuffer::flush(VkDeviceSize size, VkDeviceSize offset) const {
        return vmaFlushAllocation(lveDevice.allocator(), allocation, offset, size);
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
