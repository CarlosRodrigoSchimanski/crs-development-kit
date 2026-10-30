#pragma once

#include "engine/core/crs_device.hpp"

namespace crsdk {

    class CrsBuffer {
        public:
            CrsBuffer(
                CrsDevice& device,
                VkDeviceSize instanceSize,
                uint32_t instanceCount,
                VkBufferUsageFlags usageFlags,
                VkMemoryPropertyFlags memoryPropertyFlags,
                VkDeviceSize minOffsetAlignment = 1);
            ~CrsBuffer();

            CrsBuffer(const CrsBuffer&) = delete;
            CrsBuffer& operator=(const CrsBuffer&) = delete;

            //mapeia o buffer inteiro na memória da CPU (a VMA sempre mapeia a alocação toda);
            //lança exceção se falhar (antes devolvia um VkResult que ninguém conferia)
            void map();
            void unmap();

            void writeToBuffer(const void* data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
            void flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) const;
            VkDescriptorBufferInfo descriptorInfo(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) const;

            VkBuffer getBuffer() const;
            VkDeviceSize getBufferSize() const;

        private:
            //acha o menor tamanho de instância compatível com o alinhamento mínimo exigido pelo device
            static VkDeviceSize getAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment);

            CrsDevice& crsDevice;
            void* mapped = nullptr;
            VkBuffer buffer = VK_NULL_HANDLE;
            //memória do buffer, gerenciada pela VMA (pode ser um pedaço de um bloco maior)
            VmaAllocation allocation = VK_NULL_HANDLE;

            VkDeviceSize bufferSize;
    };

}
