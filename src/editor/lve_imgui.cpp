#include "editor/lve_imgui.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <stdexcept>

namespace lve {

    LveImgui::LveImgui(LveWindow &window, LveDevice &device, VkRenderPass renderPass, uint32_t imageCount)
        : lveDevice{device} {
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

        if (vkCreateDescriptorPool(lveDevice.device(), &poolInfo, nullptr, &imguiPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create imgui descriptor pool!");
        }

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
        initInfo.DescriptorPool = imguiPool;
        initInfo.RenderPass = renderPass;
        initInfo.Subpass = 0;
        initInfo.MinImageCount = imageCount;
        initInfo.ImageCount = imageCount;
        initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        initInfo.Allocator = nullptr;
        initInfo.CheckVkResultFn = nullptr;

        ImGui_ImplVulkan_Init(&initInfo);
    }

    LveImgui::~LveImgui() {
        vkDeviceWaitIdle(lveDevice.device());
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        vkDestroyDescriptorPool(lveDevice.device(), imguiPool, nullptr);
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
