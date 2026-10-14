#pragma once

#include "engine/core/lve_window.hpp"
#include "engine/core/lve_device.hpp"

#include <vulkan/vulkan.h>

namespace lve {

    //encapsula a configuração do Dear ImGui (UI imediata, usada pra ferramentas de editor)
    class LveImgui {
        public:
            LveImgui(LveWindow &window, LveDevice &device, VkFormat colorFormat, VkFormat depthFormat, uint32_t imageCount);
            ~LveImgui();

            LveImgui(const LveImgui&) = delete;
            LveImgui& operator=(const LveImgui&) = delete;

            //chamar no início do frame, antes de montar qualquer janela/texto do ImGui
            void newFrame();
            //chamar dentro do render pass, depois de montar as janelas/textos do frame
            void render(VkCommandBuffer commandBuffer);

        private:
            LveDevice &lveDevice;
            //pool de descriptor separado só pro ImGui: ele gerencia a própria textura
            //de fonte internamente, não compartilha o pool da aplicação
            VkDescriptorPool imguiPool;
            //o ImGui guarda um ponteiro pra ele (formato da imagem em que desenha): tem que viver junto
            VkFormat colorFormat;
    };

}
