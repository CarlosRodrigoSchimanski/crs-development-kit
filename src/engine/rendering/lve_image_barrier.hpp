#pragma once

#include <vulkan/vulkan.h>

namespace lve {
    //barreira de imagem (synchronization2): espera o que veio ANTES (src: estágio + tipo de acesso)
    //terminar antes de começar o que vem DEPOIS (dst), e troca o layout da imagem no meio do caminho.
    //oldLayout UNDEFINED = o conteúdo atual pode ser descartado (ex: a imagem vai ser limpa)
    void imageBarrier(
        VkCommandBuffer commandBuffer, VkImage image, VkImageAspectFlags aspectMask,
        VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess,
        VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess,
        VkImageLayout oldLayout, VkImageLayout newLayout);
}
