#include "engine/rendering/lve_upload_batch.hpp"

#include <cassert>
#include <stdexcept>

namespace lve {

    LveUploadBatch::LveUploadBatch(LveDevice &device) : lveDevice{device} {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = lveDevice.getCommandPool();
        allocInfo.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(lveDevice.device(), &allocInfo, &commandBuffer_) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate upload command buffer!");
        }

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        //avisa o driver que esse command buffer só vai ser submetido uma vez
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(commandBuffer_, &beginInfo);
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
        vkEndCommandBuffer(commandBuffer_);

        VkCommandBufferSubmitInfo commandBufferInfo{};
        commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        commandBufferInfo.commandBuffer = commandBuffer_;

        VkSubmitInfo2 submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submitInfo.commandBufferInfoCount = 1;
        submitInfo.pCommandBufferInfos = &commandBufferInfo;

        vkQueueSubmit2(lveDevice.graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        //espera o lote inteiro terminar antes de seguir; simples, mas bloqueia a fila até acabar
        //(no futuro: uma fence, pra carregar em segundo plano sem travar)
        vkQueueWaitIdle(lveDevice.graphicsQueue());

        vkFreeCommandBuffers(lveDevice.device(), lveDevice.getCommandPool(), 1, &commandBuffer_);
        submitted = true;
        //a GPU já terminou de ler dos staging buffers: agora podem ser destruídos
        stagingBuffers.clear();
    }
}
