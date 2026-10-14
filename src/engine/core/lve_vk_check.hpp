#pragma once

#include <vulkan/vulkan.h>

namespace lve {
    //lança exceção se uma chamada do Vulkan devolveu ERRO, com o nome do erro na mensagem
    //(ex: "Vulkan: submit do lote de envio falhou (VK_ERROR_DEVICE_LOST)").
    //No Vulkan só resultado NEGATIVO é erro: os positivos (VK_SUBOPTIMAL_KHR, VK_INCOMPLETE,
    //VK_TIMEOUT...) significam "funcionou, mas preste atenção" e não lançam nada.
    //Não usar dentro de destrutor: exceção saindo de destrutor encerra o programa na hora.
    void vkCheck(VkResult result, const char *what);
}
