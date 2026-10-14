#pragma once

#include <vulkan/vulkan.h>

#include <utility>

//declaração antecipada dos handles da VMA (são as mesmas linhas que o vk_mem_alloc.h usa): assim
//os headers da engine não precisam puxar as ~20 mil linhas da VMA; só os .cpp que chamam funções
//da VMA incluem <vk_mem_alloc.h>
VK_DEFINE_HANDLE(VmaAllocator)
VK_DEFINE_HANDLE(VmaAllocation)

namespace lve {

    //"handle inteligente" (RAII) pra um objeto do Vulkan que pertence ao device: guarda o handle e
    //chama a função de destruir dele sozinho no destrutor. Usado como MEMBRO de classe, resolve o
    //caso do construtor que falha no meio: o C++ não roda o destrutor de um objeto cujo construtor
    //lançou exceção, mas DESTRÓI os membros que já tinham sido construídos — então o que já foi
    //criado é liberado sem nenhum try/catch.
    //  T       = o tipo do handle (VkImageView, VkSampler...)
    //  Destroy = a função do Vulkan que destrói esse tipo (vkDestroyImageView, vkDestroySampler...)
    //Não copiável (dois donos destruiriam o mesmo handle), mas movível (passa a posse pra outro).
    template <typename T, auto Destroy>
    class LveHandle {
        public:
            LveHandle() = default;
            LveHandle(VkDevice device, T handle) : device{device}, handle{handle} {}
            ~LveHandle() { reset(); }

            LveHandle(const LveHandle &) = delete;
            LveHandle &operator=(const LveHandle &) = delete;

            //mover: o handle passa pra cá e o outro fica vazio (VK_NULL_HANDLE), então só um destrói
            LveHandle(LveHandle &&other) noexcept
                : device{other.device}, handle{std::exchange(other.handle, VK_NULL_HANDLE)} {}
            LveHandle &operator=(LveHandle &&other) noexcept {
                if (this != &other) {
                    reset();
                    device = other.device;
                    handle = std::exchange(other.handle, VK_NULL_HANDLE);
                }
                return *this;
            }

            T get() const { return handle; }

            //destrói agora (se tiver algo) e fica vazio
            void reset() {
                if (handle != VK_NULL_HANDLE) {
                    Destroy(device, handle, nullptr);
                    handle = VK_NULL_HANDLE;
                }
            }

        private:
            VkDevice device = VK_NULL_HANDLE;
            T handle = VK_NULL_HANDLE;
    };

    using LveImageView = LveHandle<VkImageView, vkDestroyImageView>;
    using LveSampler = LveHandle<VkSampler, vkDestroySampler>;
    using LveSemaphore = LveHandle<VkSemaphore, vkDestroySemaphore>;
    using LveFence = LveHandle<VkFence, vkDestroyFence>;
    //"Handle" no nome pra não confundir com a classe LveSwapChain, que é dona de um destes
    using LveSwapchainHandle = LveHandle<VkSwapchainKHR, vkDestroySwapchainKHR>;

    //mesma ideia pra uma imagem com memória da VMA: são DOIS handles (imagem + alocação) e a
    //destruição é pela VMA (vmaDestroyImage), por isso não cabe no LveHandle acima
    class LveAllocatedImage {
        public:
            LveAllocatedImage() = default;
            LveAllocatedImage(VmaAllocator allocator, VkImage image, VmaAllocation allocation)
                : allocator{allocator}, image{image}, allocation{allocation} {}
            ~LveAllocatedImage() { reset(); }

            LveAllocatedImage(const LveAllocatedImage &) = delete;
            LveAllocatedImage &operator=(const LveAllocatedImage &) = delete;

            LveAllocatedImage(LveAllocatedImage &&other) noexcept
                : allocator{other.allocator},
                  image{std::exchange(other.image, VK_NULL_HANDLE)},
                  allocation{std::exchange(other.allocation, VK_NULL_HANDLE)} {}
            LveAllocatedImage &operator=(LveAllocatedImage &&other) noexcept {
                if (this != &other) {
                    reset();
                    allocator = other.allocator;
                    image = std::exchange(other.image, VK_NULL_HANDLE);
                    allocation = std::exchange(other.allocation, VK_NULL_HANDLE);
                }
                return *this;
            }

            VkImage get() const { return image; }

            //no .cpp: precisa do vk_mem_alloc.h (vmaDestroyImage)
            void reset();

        private:
            VmaAllocator allocator = VK_NULL_HANDLE;
            VkImage image = VK_NULL_HANDLE;
            VmaAllocation allocation = VK_NULL_HANDLE;
    };
}
