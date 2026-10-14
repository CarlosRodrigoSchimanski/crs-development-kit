#include "engine/rendering/lve_upload_batch.hpp"

#include <cassert>

namespace lve {

    LveUploadBatch::LveUploadBatch(LveDevice &device) : lveDevice{device} {
        commandBuffer_ = lveDevice.beginSingleTimeCommands();
    }

    LveUploadBatch::~LveUploadBatch() {
        if (!submitted) {
            //nada foi enviado: a GPU nunca viu esse command buffer, pode liberar direto
            vkFreeCommandBuffers(lveDevice.device(), lveDevice.getCommandPool(), 1, &commandBuffer_);
        }
    }

    VkBuffer LveUploadBatch::stage(const void *data, VkDeviceSize size) {
        assert(!submitted && "Cannot stage data in an upload batch that was already submitted");

        auto stagingBuffer = std::make_unique<LveBuffer>(
            lveDevice,
            size,
            1,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        stagingBuffer->map();
        stagingBuffer->writeToBuffer(data);

        VkBuffer buffer = stagingBuffer->getBuffer();
        stagingBuffers.push_back(std::move(stagingBuffer));
        return buffer;
    }

    void LveUploadBatch::copyToBuffer(VkBuffer dst, const void *data, VkDeviceSize size) {
        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer_, stage(data, size), dst, 1, &copyRegion);
    }

    VkCommandBuffer LveUploadBatch::commandBuffer() const { return commandBuffer_; }

    void LveUploadBatch::submit() {
        assert(!submitted && "Upload batch was already submitted");
        //envia, espera e libera o command buffer (ver LveDevice::endSingleTimeCommands)
        lveDevice.endSingleTimeCommands(commandBuffer_);
        submitted = true;
        //a GPU já terminou de ler dos staging buffers: agora podem ser destruídos
        stagingBuffers.clear();
    }
}
