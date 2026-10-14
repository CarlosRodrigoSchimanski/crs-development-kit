#pragma once

#include "engine/core/lve_device.hpp"

namespace lve {

    class LveBuffer {
        public:
            LveBuffer(
                LveDevice& device,
                VkDeviceSize instanceSize,
                uint32_t instanceCount,
                VkBufferUsageFlags usageFlags,
                VkMemoryPropertyFlags memoryPropertyFlags,
                VkDeviceSize minOffsetAlignment = 1);
            ~LveBuffer();

            LveBuffer(const LveBuffer&) = delete;
            LveBuffer& operator=(const LveBuffer&) = delete;

            //mapeia o buffer inteiro na memória da CPU (a VMA sempre mapeia a alocação toda)
            VkResult map();
            void unmap();

            void writeToBuffer(const void* data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
            VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) const;
            VkDescriptorBufferInfo descriptorInfo(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) const;

            VkBuffer getBuffer() const;

        private:
            //acha o menor tamanho de instância compatível com o alinhamento mínimo exigido pelo device
            static VkDeviceSize getAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment);

            LveDevice& lveDevice;
            void* mapped = nullptr;
            VkBuffer buffer = VK_NULL_HANDLE;
            //memória do buffer, gerenciada pela VMA (pode ser um pedaço de um bloco maior)
            VmaAllocation allocation = VK_NULL_HANDLE;

            VkDeviceSize bufferSize;
    };

}
