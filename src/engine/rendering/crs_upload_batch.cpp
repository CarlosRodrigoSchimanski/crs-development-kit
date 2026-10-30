#include "engine/rendering/crs_upload_batch.hpp"
#include "engine/core/crs_vk_check.hpp"

#include <cassert>
#include <stdexcept>

namespace crsdk {

    CrsUploadBatch::CrsUploadBatch(CrsDevice &device) : crsDevice{device} {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = crsDevice.getCommandPool();
        allocInfo.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(crsDevice.device(), &allocInfo, &commandBuffer_) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate upload command buffer!");
        }

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        //avisa o driver que esse command buffer só vai ser submetido uma vez
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkCheck(vkBeginCommandBuffer(commandBuffer_, &beginInfo), "início da gravação do lote de envio");
    }

    CrsUploadBatch::~CrsUploadBatch() {
        if (!submitted) {
            //nada foi enviado: a GPU nunca viu esse command buffer, pode liberar direto
            vkFreeCommandBuffers(crsDevice.device(), crsDevice.getCommandPool(), 1, &commandBuffer_);
        }
    }

    VkBuffer CrsUploadBatch::stage(const void *data, VkDeviceSize size) {
        assert(!submitted && "Cannot stage data in an upload batch that was already submitted");

        auto stagingBuffer = std::make_unique<CrsBuffer>(
            crsDevice,
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

    void CrsUploadBatch::copyToBuffer(VkBuffer dst, const void *data, VkDeviceSize size) {
        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer_, stage(data, size), dst, 1, &copyRegion);
    }

    VkCommandBuffer CrsUploadBatch::commandBuffer() const { return commandBuffer_; }

    void CrsUploadBatch::submit() {
        assert(!submitted && "Upload batch was already submitted");
        vkCheck(vkEndCommandBuffer(commandBuffer_), "fim da gravação do lote de envio");

        VkCommandBufferSubmitInfo commandBufferInfo{};
        commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        commandBufferInfo.commandBuffer = commandBuffer_;

        VkSubmitInfo2 submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submitInfo.commandBufferInfoCount = 1;
        submitInfo.pCommandBufferInfos = &commandBufferInfo;

        vkCheck(vkQueueSubmit2(crsDevice.graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE), "submit do lote de envio");
        //espera o lote inteiro terminar antes de seguir; simples, mas bloqueia a fila até acabar
        //(no futuro: uma fence, pra carregar em segundo plano sem travar)
        vkCheck(vkQueueWaitIdle(crsDevice.graphicsQueue()), "espera do lote de envio");

        vkFreeCommandBuffers(crsDevice.device(), crsDevice.getCommandPool(), 1, &commandBuffer_);
        submitted = true;
        //a GPU já terminou de ler dos staging buffers: agora podem ser destruídos
        stagingBuffers.clear();
    }
}
