#pragma once

#include "engine/core/lve_device.hpp"

#include <string>
#include <vector>

namespace lve {

    // Dados de configuração do pipeline, preenchido pelo código da aplicação
    struct PipelineConfigInfo {
        //declarar o construtor de cópia (mesmo deletado) suprime o construtor padrão
        //automático; precisa devolver ele explicitamente
        PipelineConfigInfo() = default;
        PipelineConfigInfo(const PipelineConfigInfo&) = delete;
        PipelineConfigInfo& operator=(const PipelineConfigInfo&) = delete;

        std::vector<VkVertexInputBindingDescription> bindingDescriptions{};
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
        //{} em tudo: cada campo nasce zerado, então o defaultPipelineConfigInfo só precisa
        //preencher o que não é zero
        VkPipelineViewportStateCreateInfo viewportInfo{};
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
        VkPipelineRasterizationStateCreateInfo rasterizationInfo{};
        VkPipelineMultisampleStateCreateInfo multisampleInfo{};
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        VkPipelineColorBlendStateCreateInfo colorBlendInfo{};
        VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};
        std::vector<VkDynamicState> dynamicStateEnables{};
        VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
        VkPipelineLayout pipelineLayout = nullptr;
        //dynamic rendering: formatos das imagens em que o pipeline desenha (têm que bater com as
        //imagens que o vkCmdBeginRendering recebe na hora de desenhar)
        std::vector<VkFormat> colorAttachmentFormats{};
        VkFormat depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    };

    //pipeline gráfico: shaders + todo o estado fixo de como desenhar (rasterização, depth, blending...)
    class LvePipeline {
        public:
            //recebe o device (referência) e a configuração do pipeline
            LvePipeline(
                LveDevice &device,
                const std::string& vertFilepath,
                const std::string& fragFilepath,
                const PipelineConfigInfo& configInfo
            );
            ~LvePipeline();

            //evita duplicar os ponteiros dos objetos Vulkan que essa classe gerencia
            LvePipeline(const LvePipeline&) = delete;
            LvePipeline& operator=(const LvePipeline&) = delete;

            //função que grava o comando de bind do pipeline no command buffer
            void bind(VkCommandBuffer commandBuffer);

            //preenche a configuração padrão: triângulos, depth test ligado, sem blending, viewport dinâmico
            static void defaultPipelineConfigInfo(PipelineConfigInfo& configInfo);
            //liga o alpha blending numa configuração já preenchida (ex: objetos transparentes)
            static void enableAlphaBlending(PipelineConfigInfo& configInfo);
            //pipeline layout = quais descriptor sets e qual push constant os shaders recebem;
            //quem chama é dono do layout e destrói com vkDestroyPipelineLayout
            static VkPipelineLayout createPipelineLayout(
                LveDevice &device,
                const std::vector<VkDescriptorSetLayout> &setLayouts,
                VkShaderStageFlags pushStages,
                uint32_t pushSize);

        private:
            //função que lê o arquivo do shader e retorna um vetor de char
            static std::vector<char> readFile(const std::string& filepath);

            //função que cria o pipeline gráfico a partir dos shaders
            void createGraphicsPipeline(
                const std::string& vertFilepath,
                const std::string& fragFilepath,
                const PipelineConfigInfo& configInfo
            );

            VkShaderModule createShaderModule(const std::vector<char> &code);

            //referência: o device precisa existir durante toda a vida do pipeline
            LveDevice &lveDevice;
            VkPipeline graphicsPipeline;
    };
}
