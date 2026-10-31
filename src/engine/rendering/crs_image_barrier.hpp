#pragma once

#include <vulkan/vulkan.h>

namespace crsdk {
    //barreira de imagem (synchronization2): espera o que veio ANTES (src: estágio + tipo de acesso)
    //terminar antes de começar o que vem DEPOIS (dst), e troca o layout da imagem no meio do caminho.
    //oldLayout UNDEFINED = o conteúdo atual pode ser descartado (ex: a imagem vai ser limpa).
    //baseMip/mipCount: só esses mips (padrão: todos) — o gerar mipmaps troca um nível por vez
    void imageBarrier(
        VkCommandBuffer commandBuffer, VkImage image, VkImageAspectFlags aspectMask,
        VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess,
        VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess,
        VkImageLayout oldLayout, VkImageLayout newLayout,
        uint32_t baseMip = 0, uint32_t mipCount = VK_REMAINING_MIP_LEVELS);
}
