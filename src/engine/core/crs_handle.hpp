#pragma once

#include <vulkan/vulkan.h>

#include <utility>

//declaração antecipada dos handles da VMA (são as mesmas linhas que o vk_mem_alloc.h usa): assim
//os headers da engine não precisam puxar as ~20 mil linhas da VMA; só os .cpp que chamam funções
//da VMA incluem <vk_mem_alloc.h>
VK_DEFINE_HANDLE(VmaAllocator)
VK_DEFINE_HANDLE(VmaAllocation)

namespace crsdk {

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
    class CrsChildHandle {
        public:
            CrsChildHandle() = default;
            CrsChildHandle(Parent parent, T handle) : parent{parent}, handle{handle} {}
            ~CrsChildHandle() { reset(); }

            CrsChildHandle(const CrsChildHandle &) = delete;
            CrsChildHandle &operator=(const CrsChildHandle &) = delete;

            //mover: o handle passa pra cá e o outro fica vazio (VK_NULL_HANDLE), então só um destrói
            CrsChildHandle(CrsChildHandle &&other) noexcept
                : parent{other.parent}, handle{std::exchange(other.handle, VK_NULL_HANDLE)} {}
            CrsChildHandle &operator=(CrsChildHandle &&other) noexcept {
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
    using CrsHandle = CrsChildHandle<VkDevice, T, Destroy>;

    //mesma ideia pra um handle "raiz", que não tem pai (o instance, o device, o allocator da VMA):
    //a função de destruir recebe só o próprio handle
    template <typename T, auto Destroy>
    class CrsRootHandle {
        public:
            CrsRootHandle() = default;
            explicit CrsRootHandle(T handle) : handle{handle} {}
            ~CrsRootHandle() { reset(); }

            CrsRootHandle(const CrsRootHandle &) = delete;
            CrsRootHandle &operator=(const CrsRootHandle &) = delete;

            CrsRootHandle(CrsRootHandle &&other) noexcept : handle{std::exchange(other.handle, VK_NULL_HANDLE)} {}
            CrsRootHandle &operator=(CrsRootHandle &&other) noexcept {
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
    using CrsInstance = CrsRootHandle<VkInstance, destroyInstance>;
    using CrsDeviceHandle = CrsRootHandle<VkDevice, destroyDevice>;  //"Handle": a classe CrsDevice é dona de um destes
    using CrsAllocatorHandle = CrsRootHandle<VmaAllocator, destroyAllocator>;

    //filhos do instance
    using CrsSurface = CrsChildHandle<VkInstance, VkSurfaceKHR, vkDestroySurfaceKHR>;
    using CrsDebugMessenger = CrsChildHandle<VkInstance, VkDebugUtilsMessengerEXT, destroyDebugMessenger>;

    //filhos do device
    using CrsCommandPool = CrsHandle<VkCommandPool, vkDestroyCommandPool>;
    using CrsImageView = CrsHandle<VkImageView, vkDestroyImageView>;
    using CrsSampler = CrsHandle<VkSampler, vkDestroySampler>;
    using CrsSemaphore = CrsHandle<VkSemaphore, vkDestroySemaphore>;
    using CrsFence = CrsHandle<VkFence, vkDestroyFence>;
    using CrsShaderModule = CrsHandle<VkShaderModule, vkDestroyShaderModule>;
    using CrsPipelineLayout = CrsHandle<VkPipelineLayout, vkDestroyPipelineLayout>;
    //"Handle" no nome pra não confundir com as classes CrsSwapChain, CrsPipeline, CrsDescriptorSetLayout
    //e CrsDescriptorPool, que são donas de um destes
    using CrsSwapchainHandle = CrsHandle<VkSwapchainKHR, vkDestroySwapchainKHR>;
    using CrsPipelineHandle = CrsHandle<VkPipeline, vkDestroyPipeline>;
    using CrsDescriptorSetLayoutHandle = CrsHandle<VkDescriptorSetLayout, vkDestroyDescriptorSetLayout>;
    using CrsDescriptorPoolHandle = CrsHandle<VkDescriptorPool, vkDestroyDescriptorPool>;

    //mesma ideia pra uma imagem com memória da VMA: são DOIS handles (imagem + alocação) e a
    //destruição é pela VMA (vmaDestroyImage), por isso não cabe no CrsHandle acima
    class CrsAllocatedImage {
        public:
            CrsAllocatedImage() = default;
            CrsAllocatedImage(VmaAllocator allocator, VkImage image, VmaAllocation allocation)
                : allocator{allocator}, image{image}, allocation{allocation} {}
            ~CrsAllocatedImage() { reset(); }

            CrsAllocatedImage(const CrsAllocatedImage &) = delete;
            CrsAllocatedImage &operator=(const CrsAllocatedImage &) = delete;

            CrsAllocatedImage(CrsAllocatedImage &&other) noexcept
                : allocator{other.allocator},
                  image{std::exchange(other.image, VK_NULL_HANDLE)},
                  allocation{std::exchange(other.allocation, VK_NULL_HANDLE)} {}
            CrsAllocatedImage &operator=(CrsAllocatedImage &&other) noexcept {
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
