#pragma once

#include <vulkan/vulkan.h>

#include <utility>

//declaração antecipada dos handles da VMA (são as mesmas linhas que o vk_mem_alloc.h usa): assim
//os headers da engine não precisam puxar as ~20 mil linhas da VMA; só os .cpp que chamam funções
//da VMA incluem <vk_mem_alloc.h>
VK_DEFINE_HANDLE(VmaAllocator)
VK_DEFINE_HANDLE(VmaAllocation)

namespace lve {

    //"handle inteligente" (RAII) pra um objeto do Vulkan que tem um "pai" (o device ou o instance):
    //guarda o handle e chama a função de destruir dele sozinho no destrutor. Usado como MEMBRO de
    //classe, resolve o caso do construtor que falha no meio: o C++ não roda o destrutor de um objeto
    //cujo construtor lançou exceção, mas DESTRÓI os membros que já tinham sido construídos — então o
    //que já foi criado é liberado sem nenhum try/catch.
    //  Parent  = o tipo do pai (VkDevice ou VkInstance), passado pra função de destruir
    //  T       = o tipo do handle (VkImageView, VkSurfaceKHR...)
    //  Destroy = a função que destrói esse tipo (vkDestroyImageView, vkDestroySurfaceKHR...)
    //Não copiável (dois donos destruiriam o mesmo handle), mas movível (passa a posse pra outro).
    template <typename Parent, typename T, auto Destroy>
    class LveChildHandle {
        public:
            LveChildHandle() = default;
            LveChildHandle(Parent parent, T handle) : parent{parent}, handle{handle} {}
            ~LveChildHandle() { reset(); }

            LveChildHandle(const LveChildHandle &) = delete;
            LveChildHandle &operator=(const LveChildHandle &) = delete;

            //mover: o handle passa pra cá e o outro fica vazio (VK_NULL_HANDLE), então só um destrói
            LveChildHandle(LveChildHandle &&other) noexcept
                : parent{other.parent}, handle{std::exchange(other.handle, VK_NULL_HANDLE)} {}
            LveChildHandle &operator=(LveChildHandle &&other) noexcept {
                if (this != &other) {
                    reset();
                    parent = other.parent;
                    handle = std::exchange(other.handle, VK_NULL_HANDLE);
                }
                return *this;
            }

            T get() const { return handle; }

            //destrói agora (se tiver algo) e fica vazio
            void reset() {
                if (handle != VK_NULL_HANDLE) {
                    Destroy(parent, handle, nullptr);
                    handle = VK_NULL_HANDLE;
                }
            }

        private:
            Parent parent = VK_NULL_HANDLE;
            T handle = VK_NULL_HANDLE;
    };

    //o caso mais comum: objeto que pertence ao device
    template <typename T, auto Destroy>
    using LveHandle = LveChildHandle<VkDevice, T, Destroy>;

    //mesma ideia pra um handle "raiz", que não tem pai (o instance, o device, o allocator da VMA):
    //a função de destruir recebe só o próprio handle
    template <typename T, auto Destroy>
    class LveRootHandle {
        public:
            LveRootHandle() = default;
            explicit LveRootHandle(T handle) : handle{handle} {}
            ~LveRootHandle() { reset(); }

            LveRootHandle(const LveRootHandle &) = delete;
            LveRootHandle &operator=(const LveRootHandle &) = delete;

            LveRootHandle(LveRootHandle &&other) noexcept : handle{std::exchange(other.handle, VK_NULL_HANDLE)} {}
            LveRootHandle &operator=(LveRootHandle &&other) noexcept {
                if (this != &other) {
                    reset();
                    handle = std::exchange(other.handle, VK_NULL_HANDLE);
                }
                return *this;
            }

            T get() const { return handle; }

            void reset() {
                if (handle != VK_NULL_HANDLE) {
                    Destroy(handle);
                    handle = VK_NULL_HANDLE;
                }
            }

        private:
            T handle = VK_NULL_HANDLE;
    };

    //funções de destruir com a "cara" que os templates esperam (no .cpp)
    void destroyInstance(VkInstance instance);
    void destroyDevice(VkDevice device);
    void destroyAllocator(VmaAllocator allocator);
    //o debug messenger é de uma extensão: a função de destruir não vem pronta, é carregada na hora
    void destroyDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks *allocator);

    //raízes: sem pai
    using LveInstance = LveRootHandle<VkInstance, destroyInstance>;
    using LveDeviceHandle = LveRootHandle<VkDevice, destroyDevice>;  //"Handle": a classe LveDevice é dona de um destes
    using LveAllocatorHandle = LveRootHandle<VmaAllocator, destroyAllocator>;

    //filhos do instance
    using LveSurface = LveChildHandle<VkInstance, VkSurfaceKHR, vkDestroySurfaceKHR>;
    using LveDebugMessenger = LveChildHandle<VkInstance, VkDebugUtilsMessengerEXT, destroyDebugMessenger>;

    //filhos do device
    using LveCommandPool = LveHandle<VkCommandPool, vkDestroyCommandPool>;
    using LveImageView = LveHandle<VkImageView, vkDestroyImageView>;
    using LveSampler = LveHandle<VkSampler, vkDestroySampler>;
    using LveSemaphore = LveHandle<VkSemaphore, vkDestroySemaphore>;
    using LveFence = LveHandle<VkFence, vkDestroyFence>;
    using LveShaderModule = LveHandle<VkShaderModule, vkDestroyShaderModule>;
    using LvePipelineLayout = LveHandle<VkPipelineLayout, vkDestroyPipelineLayout>;
    //"Handle" no nome pra não confundir com as classes LveSwapChain, LvePipeline, LveDescriptorSetLayout
    //e LveDescriptorPool, que são donas de um destes
    using LveSwapchainHandle = LveHandle<VkSwapchainKHR, vkDestroySwapchainKHR>;
    using LvePipelineHandle = LveHandle<VkPipeline, vkDestroyPipeline>;
    using LveDescriptorSetLayoutHandle = LveHandle<VkDescriptorSetLayout, vkDestroyDescriptorSetLayout>;
    using LveDescriptorPoolHandle = LveHandle<VkDescriptorPool, vkDestroyDescriptorPool>;

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
