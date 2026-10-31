#include "engine/rendering/crs_image_barrier.hpp"

namespace crsdk {

    void imageBarrier(
        VkCommandBuffer commandBuffer, VkImage image, VkImageAspectFlags aspectMask,
        VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess,
        VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess,
        VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t baseMip, uint32_t mipCount) {
        VkImageMemoryBarrier2 barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.srcStageMask = srcStage;
        barrier.srcAccessMask = srcAccess;
        barrier.dstStageMask = dstStage;
        barrier.dstAccessMask = dstAccess;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        //os mips pedidos, todas as camadas (a engine ainda só usa 1)
        barrier.subresourceRange = {aspectMask, baseMip, mipCount, 0, VK_REMAINING_ARRAY_LAYERS};

        VkDependencyInfo dependencyInfo{};
        dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.imageMemoryBarrierCount = 1;
        dependencyInfo.pImageMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
    }
}
