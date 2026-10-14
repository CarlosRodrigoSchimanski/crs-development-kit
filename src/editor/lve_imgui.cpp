#include "editor/lve_imgui.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <vulkan/vk_enum_string_helper.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace lve {

    //o ImGui chama isto depois de cada chamada do Vulkan dele. Não dá pra lançar exceção aqui
    //(ela atravessaria o código do ImGui, que não espera isso): avisa e encerra se for erro
    static void checkImguiVkResult(VkResult result) {
        if (result < 0) {
            std::cerr << "Vulkan (ImGui): " << string_VkResult(result) << std::endl;
            std::abort();
        }
    }

    LveImgui::LveImgui(LveWindow &window, LveDevice &device, VkFormat colorFormat, VkFormat depthFormat, uint32_t imageCount)
        : lveDevice{device}, colorFormat{colorFormat} {
        //o backend Vulkan do ImGui só usa combined image sampler: 1 pra fonte + 1 por imagem mostrada na UI
        //(a lista antiga com 11 tipos x 1000 veio de um exemplo velho do ImGui que reservava "de tudo")
        constexpr uint32_t MAX_UI_IMAGES = 100;
        VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_UI_IMAGES};

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        //o ImGui libera os sets dele individualmente, então o pool precisa permitir isso
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = MAX_UI_IMAGES;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;

        VkDescriptorPool pool;
        if (vkCreateDescriptorPool(lveDevice.device(), &poolInfo, nullptr, &pool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create imgui descriptor pool!");
        }
        imguiPool = LveDescriptorPoolHandle{lveDevice.device(), pool};

        ImGui::CreateContext();

        //"true" instala os callbacks de mouse/teclado/scroll do GLFW; o ImGui encadeia
        //com os callbacks que a LveWindow já registrou, em vez de substituí-los
        ImGui_ImplGlfw_InitForVulkan(window.getGLFWwindow(), true);

        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.Instance = lveDevice.instance();
        initInfo.PhysicalDevice = lveDevice.physicalDevice();
        initInfo.Device = lveDevice.device();
        initInfo.QueueFamily = lveDevice.findPhysicalQueueFamilies().graphicsFamily.value();
        initInfo.Queue = lveDevice.graphicsQueue();
        initInfo.PipelineCache = VK_NULL_HANDLE;
        initInfo.DescriptorPool = imguiPool.get();
        //dynamic rendering: o ImGui desenha dentro do begin/end rendering da tela, então o pipeline
        //dele precisa dos mesmos formatos (inclusive o da profundidade, mesmo sem usar)
        initInfo.UseDynamicRendering = true;
        initInfo.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        initInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
        initInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &this->colorFormat;
        initInfo.PipelineRenderingCreateInfo.depthAttachmentFormat = depthFormat;
        initInfo.MinImageCount = imageCount;
        initInfo.ImageCount = imageCount;
        initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        initInfo.Allocator = nullptr;
        initInfo.CheckVkResultFn = checkImguiVkResult;

        ImGui_ImplVulkan_Init(&initInfo);
    }

    //o corpo do destrutor roda ANTES de os membros serem destruídos: o ImGui desliga (devolvendo os
    //descriptor sets dele ao pool) e só depois o imguiPool (handle RAII) é destruído
    LveImgui::~LveImgui() {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void LveImgui::newFrame() {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void LveImgui::render(VkCommandBuffer commandBuffer) {
        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
    }

}
