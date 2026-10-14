#include "engine/rendering/lve_swap_chain.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace lve {

    LveSwapChain::LveSwapChain(LveDevice &deviceRef, VkExtent2D extent, const LveSwapChain *previous)
        : device{deviceRef}, windowExtent{extent} {
        createSwapChain(previous == nullptr ? VK_NULL_HANDLE : previous->swapChain.get());
        createImageViews();
        createDepthResources();
        createSyncObjects();
    }

    bool LveSwapChain::compareSwapFormats(const LveSwapChain &other) const {
        return other.swapChainDepthFormat == swapChainDepthFormat &&
            other.swapChainImageFormat == swapChainImageFormat;
    }

    VkImage LveSwapChain::getImage(uint32_t index) const { return swapChainImages[index]; }
    VkImageView LveSwapChain::getImageView(uint32_t index) const { return swapChainImageViews[index].get(); }
    VkImage LveSwapChain::getDepthImage(uint32_t index) const { return depthImages[index].get(); }
    VkImageView LveSwapChain::getDepthImageView(uint32_t index) const { return depthImageViews[index].get(); }
    size_t LveSwapChain::imageCount() const { return swapChainImages.size(); }
    VkFormat LveSwapChain::getSwapChainImageFormat() const { return swapChainImageFormat; }
    VkFormat LveSwapChain::getSwapChainDepthFormat() const { return swapChainDepthFormat; }
    VkExtent2D LveSwapChain::getSwapChainExtent() const { return swapChainExtent; }

    float LveSwapChain::extentAspectRatio() const {
        return static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height);
    }

    VkResult LveSwapChain::acquireNextImage(uint32_t *imageIndex) {
        //espera o frame anterior (do mesmo "slot") terminar antes de reaproveitar seus recursos
        VkFence frameFence = inFlightFences[currentFrame].get();
        vkWaitForFences(device.device(), 1, &frameFence, VK_TRUE, UINT64_MAX);

        VkResult result = vkAcquireNextImageKHR(
            device.device(),
            swapChain.get(),
            UINT64_MAX,
            imageAvailableSemaphores[currentFrame].get(),
            VK_NULL_HANDLE,
            imageIndex);

        return result;
    }

    VkResult LveSwapChain::submitCommandBuffers(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
            vkWaitForFences(device.device(), 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
        }
        VkFence frameFence = inFlightFences[currentFrame].get();
        imagesInFlight[imageIndex] = frameFence;

        //espera a imagem ser liberada pela tela, só no estágio de escrever cor: o que vem antes
        //(vértices, sombra...) pode rodar enquanto isso. É o mesmo estágio da barreira de cor do
        //LveRenderer::beginSwapChainRendering, que se encadeia com esta espera
        VkSemaphoreSubmitInfo waitInfo{};
        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        waitInfo.semaphore = imageAvailableSemaphores[currentFrame].get();
        waitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkCommandBufferSubmitInfo commandBufferInfo{};
        commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        commandBufferInfo.commandBuffer = commandBuffer;

        //sinaliza quando TUDO terminar; indexado pela imagem, não pelo frame: ver comentário no header
        VkSemaphoreSubmitInfo signalInfo{};
        signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signalInfo.semaphore = renderFinishedSemaphores[imageIndex].get();
        signalInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

        VkSubmitInfo2 submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submitInfo.waitSemaphoreInfoCount = 1;
        submitInfo.pWaitSemaphoreInfos = &waitInfo;
        submitInfo.commandBufferInfoCount = 1;
        submitInfo.pCommandBufferInfos = &commandBufferInfo;
        submitInfo.signalSemaphoreInfoCount = 1;
        submitInfo.pSignalSemaphoreInfos = &signalInfo;

        vkResetFences(device.device(), 1, &frameFence);
        if (vkQueueSubmit2(device.graphicsQueue(), 1, &submitInfo, frameFence) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        VkSemaphore renderFinished = renderFinishedSemaphores[imageIndex].get();
        presentInfo.pWaitSemaphores = &renderFinished;

        VkSwapchainKHR swapChains[] = {swapChain.get()};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        auto result = vkQueuePresentKHR(device.presentQueue(), &presentInfo);

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

        return result;
    }

    void LveSwapChain::createSwapChain(VkSwapchainKHR oldSwapchain) {
        SwapChainSupportDetails swapChainSupport = device.getSwapChainSupport();

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        //pede 1 imagem a mais que o mínimo, pra não ficar esperando o driver liberar uma
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 &&
            imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = device.surface();

        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        QueueFamilyIndices indices = device.findPhysicalQueueFamilies();
        uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

        if (indices.graphicsFamily != indices.presentFamily) {
            //filas diferentes: precisa permitir que as duas acessem a mesma imagem
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            //mesma fila pras duas coisas: modo exclusivo, mais performático
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = nullptr;
        }

        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        //se existe um swap chain anterior (redimensionamento), informa ele aqui;
        //senão, VK_NULL_HANDLE (primeira criação)
        createInfo.oldSwapchain = oldSwapchain;

        VkSwapchainKHR newSwapChain;
        if (vkCreateSwapchainKHR(device.device(), &createInfo, nullptr, &newSwapChain) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }
        swapChain = LveSwapchainHandle{device.device(), newSwapChain};

        //só pedimos um número mínimo de imagens; o driver pode ter criado mais.
        //por isso consultamos de novo o número real antes de pegar os handles.
        vkGetSwapchainImagesKHR(device.device(), swapChain.get(), &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device.device(), swapChain.get(), &imageCount, swapChainImages.data());

        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;
    }

    void LveSwapChain::createImageViews() {
        swapChainImageViews.resize(swapChainImages.size());
        for (size_t i = 0; i < swapChainImages.size(); i++) {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = swapChainImages[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = swapChainImageFormat;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            VkImageView imageView;
            if (vkCreateImageView(device.device(), &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
                throw std::runtime_error("failed to create swap chain image view!");
            }
            swapChainImageViews[i] = LveImageView{device.device(), imageView};
        }
    }

    void LveSwapChain::createDepthResources() {
        VkFormat depthFormat = findDepthFormat();
        swapChainDepthFormat = depthFormat;
        VkExtent2D extent = getSwapChainExtent();

        depthImages.resize(imageCount());
        depthImageViews.resize(imageCount());

        for (size_t i = 0; i < depthImages.size(); i++) {
            VkImageCreateInfo imageInfo{};
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            imageInfo.extent.width = extent.width;
            imageInfo.extent.height = extent.height;
            imageInfo.extent.depth = 1;
            imageInfo.mipLevels = 1;
            imageInfo.arrayLayers = 1;
            imageInfo.format = depthFormat;
            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            imageInfo.flags = 0;

            depthImages[i] = device.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = depthImages[i].get();
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = depthFormat;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            VkImageView imageView;
            if (vkCreateImageView(device.device(), &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
                throw std::runtime_error("failed to create depth image view!");
            }
            depthImageViews[i] = LveImageView{device.device(), imageView};
        }
    }

    void LveSwapChain::createSyncObjects() {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(imageCount());
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        imagesInFlight.resize(imageCount(), VK_NULL_HANDLE);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        //já nasce "sinalizado", senão o primeiro frame ficaria esperando um fence que nunca dispararia
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        auto createSemaphore = [&]() {
            VkSemaphore semaphore;
            if (vkCreateSemaphore(device.device(), &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS) {
                throw std::runtime_error("failed to create semaphore!");
            }
            return LveSemaphore{device.device(), semaphore};
        };

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            imageAvailableSemaphores[i] = createSemaphore();
            VkFence fence;
            if (vkCreateFence(device.device(), &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
                throw std::runtime_error("failed to create fence!");
            }
            inFlightFences[i] = LveFence{device.device(), fence};
        }
        for (auto &semaphore : renderFinishedSemaphores) {
            semaphore = createSemaphore();
        }
    }

    VkSurfaceFormatKHR LveSwapChain::chooseSwapSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR> &availableFormats) {
        for (const auto &availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }

    VkPresentModeKHR LveSwapChain::chooseSwapPresentMode(
        const std::vector<VkPresentModeKHR> &availablePresentModes) {
        for (const auto &availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                std::cout << "modo de apresentação: Mailbox" << std::endl;
                return availablePresentMode;
            }
        }

        std::cout << "modo de apresentação: V-Sync (FIFO)" << std::endl;
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D LveSwapChain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities) {
        if (capabilities.currentExtent.width != UINT32_MAX) {
            return capabilities.currentExtent;
        } else {
            VkExtent2D actualExtent = windowExtent;
            actualExtent.width = std::max(
                capabilities.minImageExtent.width,
                std::min(capabilities.maxImageExtent.width, actualExtent.width));
            actualExtent.height = std::max(
                capabilities.minImageExtent.height,
                std::min(capabilities.maxImageExtent.height, actualExtent.height));

            return actualExtent;
        }
    }

    VkFormat LveSwapChain::findDepthFormat() const {
        return device.findSupportedFormat(
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }
}
