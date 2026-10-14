#include "engine/rendering/lve_renderer.hpp"
#include "engine/rendering/lve_image_barrier.hpp"
#include "engine/core/lve_vk_check.hpp"

#include <stdexcept>
#include <cassert>

namespace lve {

    LveRenderer::LveRenderer(LveWindow &window, LveDevice &device) : lveWindow{window}, lveDevice{device} {
        recreateSwapChain();
        createCommandBuffers();
    }

    LveRenderer::~LveRenderer() {
        //devolve os command buffers pro command pool (que é do device e continua existindo)
        vkFreeCommandBuffers(
            lveDevice.device(),
            lveDevice.getCommandPool(),
            static_cast<uint32_t>(commandBuffers.size()),
            commandBuffers.data());
    }

    VkFormat LveRenderer::getSwapChainImageFormat() const { return lveSwapChain->getSwapChainImageFormat(); }
    VkFormat LveRenderer::getSwapChainDepthFormat() const { return lveSwapChain->getSwapChainDepthFormat(); }

    float LveRenderer::getAspectRatio() const {return lveSwapChain->extentAspectRatio();}

    uint32_t LveRenderer::getImageCount() const { return static_cast<uint32_t>(lveSwapChain->imageCount()); }

    VkCommandBuffer LveRenderer::getCurrentCommandBuffer() const {
        assert(isFrameStarted && "Cannot get command buffer when frame not in progress");
        return commandBuffers[currentFrameIndex];
    }


    void LveRenderer::recreateSwapChain() {
        lveWindow.waitWhileMinimized();
        auto extent = lveWindow.getExtent();
        vkCheck(vkDeviceWaitIdle(lveDevice.device()), "espera da GPU antes de recriar a swap chain");

        if (lveSwapChain == nullptr) {
            lveSwapChain = std::make_unique<LveSwapChain>(lveDevice, extent);
        } else {
            //o antigo continua vivo até o fim deste bloco: o novo usa ele durante a criação
            std::unique_ptr<LveSwapChain> oldSwapChain = std::move(lveSwapChain);
            lveSwapChain = std::make_unique<LveSwapChain>(lveDevice, extent, oldSwapChain.get());

            if (!oldSwapChain->compareSwapFormats(*lveSwapChain)) {
                throw std::runtime_error("swap chain image (or depth) format has changed!");
            }
        }
    }

    void LveRenderer::createCommandBuffers() {
        commandBuffers.resize(LveSwapChain::MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = lveDevice.getCommandPool();
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

        if (vkAllocateCommandBuffers(lveDevice.device(), &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    VkCommandBuffer LveRenderer::beginFrame() {
        assert(!isFrameStarted && "Can't call beginFrame while alredy in progress");

        //a janela já avisou que mudou de tamanho: recria antes, senão este frame sairia no tamanho antigo
        if (lveWindow.wasWindowResized()) {
            lveWindow.resetWindowResizedFlag();
            recreateSwapChain();
        }

        auto result = lveSwapChain->acquireNextImage(&currentImageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            //swap chain desatualizada (ex: mudou de tamanho): recria e tenta de novo na nova, em vez
            //de pular o frame. Pular fazia a imagem congelar durante o arrasto da borda no Windows:
            //cada redesenho pedido pelo SO caía nesta recriação e nenhum chegava a desenhar
            recreateSwapChain();
            result = lveSwapChain->acquireNextImage(&currentImageIndex);
            if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                return nullptr;  //mudou de novo nesse meio tempo: fica pro próximo frame
            }
        }

        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        isFrameStarted = true;

        auto commandBuffer = getCurrentCommandBuffer();
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        return commandBuffer;
    }

    void LveRenderer::endFrame() {
        assert(isFrameStarted && "Can't call endFrame while frame is not in progress");

        auto commandBuffer = getCurrentCommandBuffer();
        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }

        auto result = lveSwapChain->submitCommandBuffers(commandBuffer, currentImageIndex);
        if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || lveWindow.wasWindowResized()) {
            lveWindow.resetWindowResizedFlag();
            recreateSwapChain();

        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to submit command buffer!");
        }

        isFrameStarted = false;
        currentFrameIndex = (currentFrameIndex + 1) % LveSwapChain::MAX_FRAMES_IN_FLIGHT;
    }

    //formato com stencil (ex: D24S8) precisa do aspecto de stencil na barreira junto com o de profundidade
    static VkImageAspectFlags depthAspect(VkFormat format) {
        bool hasStencil = format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
        return VK_IMAGE_ASPECT_DEPTH_BIT | (hasStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
    }

    void LveRenderer::beginSwapChainRendering(VkCommandBuffer commandBuffer) {
        assert(isFrameStarted && "can't call beginSwapChainRendering if frame is not in progress");
        assert(commandBuffer== getCurrentCommandBuffer() && "can't begin rendering on command buffer from a different frame");

        VkExtent2D extent = lveSwapChain->getSwapChainExtent();

        //cor: UNDEFINED -> pronta pra receber cor. O src é COLOR_ATTACHMENT_OUTPUT porque é nesse
        //estágio que o submit espera o semáforo "imagem disponível": usando o mesmo estágio, a
        //barreira se encadeia com o semáforo e a troca de layout só acontece depois que a tela
        //liberou a imagem
        imageBarrier(commandBuffer, lveSwapChain->getImage(currentImageIndex), VK_IMAGE_ASPECT_COLOR_BIT,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_NONE,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        //profundidade: o frame anterior que usou esta mesma imagem pode ainda estar escrevendo nela
        imageBarrier(commandBuffer, lveSwapChain->getDepthImage(currentImageIndex),
            depthAspect(lveSwapChain->getSwapChainDepthFormat()),
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = lveSwapChain->getImageView(currentImageIndex);
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue.color = {{0.01f, 0.01f, 0.01f, 1.0f}};

        //a profundidade da tela só serve durante o desenho: não precisa ser guardada (DONT_CARE)
        VkRenderingAttachmentInfo depthAttachment{};
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = lveSwapChain->getDepthImageView(currentImageIndex);
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.clearValue.depthStencil = {1.0f, 0};

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea = {{0, 0}, extent};
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;
        renderingInfo.pDepthAttachment = &depthAttachment;

        vkCmdBeginRendering(commandBuffer, &renderingInfo);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(extent.width);
        viewport.height = static_cast<float>(extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        VkRect2D scissor{{0, 0}, extent};
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    }

    void LveRenderer::endSwapChainRendering(VkCommandBuffer commandBuffer) {
        assert(isFrameStarted && "can't call endSwapChainRendering if frame is not in progress");
        assert(commandBuffer== getCurrentCommandBuffer() && "can't end rendering on command buffer from a different frame");

        vkCmdEndRendering(commandBuffer);

        //cor -> pronta pra tela. Depois dela ninguém mais neste command buffer usa a imagem
        //(dst = NONE): quem espera agora é o semáforo "render terminou", que a apresentação usa
        imageBarrier(commandBuffer, lveSwapChain->getImage(currentImageIndex), VK_IMAGE_ASPECT_COLOR_BIT,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }

    int LveRenderer::getFrameIndex() const {
        assert(isFrameStarted && "Cannot get frame index when frame not in progress");
        return currentFrameIndex;
    }
}