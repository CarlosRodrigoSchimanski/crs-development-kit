#pragma once

#include "engine/core/crs_device.hpp"
#include "engine/rendering/crs_descriptors.hpp"
#include "engine/rendering/crs_frameInfo.hpp"
#include "engine/rendering/crs_pipeline.hpp"

#include <memory>

namespace crsdk {
    //aura laranja em volta do objeto selecionado no editor, como no Blender. Dois passos:
    //1) renderMask, ANTES do passe principal: desenha só o selecionado numa imagem à parte (máscara);
    //2) renderOutline, DENTRO do passe principal: um triângulo do tamanho da tela pinta de laranja
    //   os pixels perto da borda da máscara (linha firme + brilho fraco)
    //Funciona com qualquer modelo (inclusive animado); a aura aparece por cima do que estiver na frente
    class SelectionOutlineSystem {
        public:
            //color/depthFormat: os da tela (o passo 2 é desenhado dentro do passe dela)
            SelectionOutlineSystem(CrsDevice &device, VkFormat colorFormat, VkFormat depthFormat, VkDescriptorSetLayout globalSetLayout);

            SelectionOutlineSystem(const SelectionOutlineSystem&) = delete;
            SelectionOutlineSystem& operator=(const SelectionOutlineSystem&) = delete;

            //selected = nullptr: nada selecionado, e o renderOutline deste frame não desenha nada.
            //screen = tamanho da imagem da tela (a máscara acompanha); sceneArea = onde a cena é desenhada
            void renderMask(FrameInfo &frameInfo, const CrsGameObject *selected, VkExtent2D screen, VkRect2D sceneArea);
            void renderOutline(FrameInfo &frameInfo);

        private:
            void createPipelines(VkFormat colorFormat, VkFormat depthFormat);
            //máscara do tamanho da tela; refeita quando a janela muda de tamanho
            void createMask(VkExtent2D extent);

            CrsDevice &crsDevice;

            CrsPipelineLayout maskPipelineLayout;
            std::unique_ptr<CrsPipeline> maskPipeline;
            std::unique_ptr<CrsPipeline> skinnedMaskPipeline;

            //o passo 2 lê a máscara por um descriptor set próprio (o global não sabe dela)
            std::unique_ptr<CrsDescriptorSetLayout> maskSetLayout;
            std::unique_ptr<CrsDescriptorPool> maskPool;
            VkDescriptorSet maskSet = VK_NULL_HANDLE;
            CrsPipelineLayout outlinePipelineLayout;
            std::unique_ptr<CrsPipeline> outlinePipeline;

            CrsAllocatedImage maskImage;
            CrsImageView maskImageView;
            CrsSampler maskSampler;
            VkExtent2D maskExtent{0, 0};

            //o renderMask deste frame desenhou alguém: o renderOutline tem o que pintar
            bool hasSelection = false;
    };
}
