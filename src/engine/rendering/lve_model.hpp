#pragma once

#include "engine/rendering/lve_upload_batch.hpp"
#include "engine/animation/lve_skeleton.hpp"
#include "engine/rendering/lve_texture.hpp"
#include "engine/rendering/lve_buffer.hpp"
#include "engine/core/lve_device.hpp"

#include <optional>
#include <vector>
#include <memory>
#include <string>

#include <glm/glm.hpp>

namespace lve {

    class LveModel {
        public:
            struct Vertex {
                glm::vec3 position;
                glm::vec3 color;
                glm::vec3 normal;
                glm::vec2 uv;

                static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
                static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();

                //compara campo por campo; campo novo entra na comparação sozinho
                bool operator==(const Vertex &other) const = default;
            };

            //ossos que puxam um vértice e com que peso (só modelos com esqueleto: fica num buffer à parte)
            struct SkinVertex {
                glm::uvec4 joints{0};    //índices dos até 4 ossos
                glm::vec4 weights{0.f};  //quanto cada um puxa (somam 1)
            };

            struct Builder {
                std::vector<Vertex> vertices{};
                std::vector<uint32_t> indices{};
                //imagem de cor do material (png/jpg ainda codificado); vazio = modelo sem textura
                std::vector<unsigned char> baseColorImage{};

                //pares de índices: as arestas reais (onde a superfície dobra), pros gizmos
                std::vector<uint32_t> edgeIndices{};
                //esqueleto e animação
                std::optional<Skeleton> skeleton{};
                std::vector<AnimationClip> animations{};
                std::vector<SkinVertex> skinVertices{};  //um por vértice; vazio = modelo sem esqueleto

                //carrega um .gltf/.glb (formato do jogo: exportado direto do Blender)
                void loadModel(const std::string &filepath);
                //calcula as arestas reais a partir dos triângulos (chamado no fim do loadModel)
                void buildEdges();

            };

            LveModel(LveDevice &device, const LveModel::Builder &builder);

            LveModel(const LveModel&) = delete;
            LveModel& operator=(const LveModel&) = delete;

            static std::unique_ptr<LveModel> createModelFromFile(LveDevice &device, const std::string &filepath);

            void bind(VkCommandBuffer commandBuffer);
            void draw(VkCommandBuffer commandBuffer);

            //desenha as arestas reais como linhas (precisa de uma pipeline LINE_LIST)
            void drawEdges(VkCommandBuffer commandBuffer);

            bool hasSkin() const;
            //como o bind, mas também liga os pesos dos ossos (binding 1) pra pipeline de skinning
            void bindSkinned(VkCommandBuffer commandBuffer);

            //nullptr se o modelo não tem textura
            LveTexture *getTexture() const;
            //caixa em volta do modelo, no espaço dele (menor e maior ponto dos vértices)
            glm::vec3 getBoundsMin() const;
            glm::vec3 getBoundsMax() const;

            //esqueleto do modelo
            const Skeleton *getSkeleton() const;
            const std::vector<AnimationClip> &getAnimations() const;

            //arquivo de onde o modelo foi carregado
            const std::string &getFilePath() const;

            //pipeline de skinning: binding 0 = Vertex, binding 1 = SkinVertex
            static std::vector<VkVertexInputBindingDescription> getSkinnedBindingDescriptions();
            static std::vector<VkVertexInputAttributeDescription> getSkinnedAttributeDescriptions();

        private:
            void createVertexBuffers(const std::vector<Vertex> &vertices, LveUploadBatch &upload);
            void createIndexBuffers(const std::vector<uint32_t> &indices, LveUploadBatch &upload);
            std::unique_ptr<LveBuffer> createDeviceLocalBuffer(
                const void *data, uint32_t elementSize, uint32_t count, VkBufferUsageFlags usage, LveUploadBatch &upload);

            LveDevice &lveDevice;

            std::unique_ptr<LveBuffer> vertexBuffer;
            uint32_t vertexCount;

            std::unique_ptr<LveBuffer> indexBuffer;  //vazio = modelo sem índices
            uint32_t indexCount;

            std::unique_ptr<LveBuffer> edgeIndexBuffer;  //vazio = modelo sem arestas
            uint32_t edgeIndexCount = 0;

            std::unique_ptr<LveBuffer> skinVertexBuffer;

            std::unique_ptr<LveTexture> texture;

            glm::vec3 boundsMin{0.f};
            glm::vec3 boundsMax{0.f};

            std::optional<Skeleton> skeleton{};
            std::vector<AnimationClip> animations{};

            std::string filePath{};
    };
}
