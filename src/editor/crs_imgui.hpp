#pragma once

#include "engine/core/crs_window.hpp"
#include "engine/core/crs_device.hpp"

#include <vulkan/vulkan.h>

namespace crsdk {

    //encapsula a configuração do Dear ImGui (UI imediata, usada pra ferramentas de editor)
    class CrsImgui {
        public:
            CrsImgui(CrsWindow &window, CrsDevice &device, VkFormat colorFormat, VkFormat depthFormat, uint32_t imageCount);
            ~CrsImgui();

            CrsImgui(const CrsImgui&) = delete;
            CrsImgui& operator=(const CrsImgui&) = delete;

            //chamar no início do frame, antes de montar qualquer janela/texto do ImGui
            void newFrame();
            //chamar dentro do render pass, depois de montar as janelas/textos do frame
            void render(VkCommandBuffer commandBuffer);

        private:
            CrsDevice &crsDevice;
            //pool de descriptor separado só pro ImGui: ele gerencia a própria textura
            //de fonte internamente, não compartilha o pool da aplicação
            CrsDescriptorPoolHandle imguiPool;
            //o ImGui guarda um ponteiro pra ele (formato da imagem em que desenha): tem que viver junto
            VkFormat colorFormat;
    };

}
