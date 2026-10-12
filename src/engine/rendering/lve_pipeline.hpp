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
        VkPipelineViewportStateCreateInfo viewportInfo;
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
        VkPipelineRasterizationStateCreateInfo rasterizationInfo;
        VkPipelineMultisampleStateCreateInfo multisampleInfo;
        VkPipelineColorBlendAttachmentState colorBlendAttachment;
        VkPipelineColorBlendStateCreateInfo colorBlendInfo;
        VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
        std::vector<VkDynamicState>dynamicStateEnables;
        VkPipelineDynamicStateCreateInfo dynamicStateInfo;
        VkPipelineLayout pipelineLayout = nullptr;
        VkRenderPass renderPass = nullptr;
        uint32_t subpass = 0;
    };

    //clase responsevel pelo pipelide dos shaders
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

            //monta uma configuração padrão de pipeline pro tamanho de janela informado
            static void defaultPipelineConfigInfo(PipelineConfigInfo& configInfo);
            //monta uma configuração padrão de pipeline pro tamanho de janela informado, com blending ativado
            static void enableAlphaBlending(PipelineConfigInfo& configInfo);

        private:
            //função que lê o arquivo do shader e retorna um vetor de char
            static std::vector<char> readFile(const std::string& filepath);

            //função que cria o pipeline gráfico a partir dos shaders
            void createGraphicsPipeline(
                const std::string& vertFilepath,
                const std::string& fragFilepath,
                const PipelineConfigInfo& configInfo
            );

            void createShaderModule(const std::vector<char> &code, VkShaderModule *shaderModule);

            //referência ao device: assim como o pipeline depende da janela existir,
            //aqui o device precisa existir durante toda a vida do pipeline
            LveDevice &lveDevice;
            VkPipeline graphicsPipeline;
            VkShaderModule vertShaderModule;
            VkShaderModule fragShaderModule;
    };
}
