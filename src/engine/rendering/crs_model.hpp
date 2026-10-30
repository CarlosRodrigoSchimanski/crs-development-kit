#pragma once

#include "engine/rendering/crs_upload_batch.hpp"
#include "engine/animation/crs_skeleton.hpp"
#include "engine/rendering/crs_texture.hpp"
#include "engine/rendering/crs_buffer.hpp"
#include "engine/core/crs_device.hpp"

#include <optional>
#include <vector>
#include <memory>
#include <string>

#include <glm/glm.hpp>

namespace crsdk {

    class CrsModel {
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

            CrsModel(CrsDevice &device, const CrsModel::Builder &builder);

            CrsModel(const CrsModel&) = delete;
            CrsModel& operator=(const CrsModel&) = delete;

            static std::unique_ptr<CrsModel> createModelFromFile(CrsDevice &device, const std::string &filepath);

            void bind(VkCommandBuffer commandBuffer);
            void draw(VkCommandBuffer commandBuffer);

            //desenha as arestas reais como linhas (precisa de uma pipeline LINE_LIST)
            void drawEdges(VkCommandBuffer commandBuffer);

            bool hasSkin() const;
            //como o bind, mas também liga os pesos dos ossos (binding 1) pra pipeline de skinning
            void bindSkinned(VkCommandBuffer commandBuffer);

            //nullptr se o modelo não tem textura
            CrsTexture *getTexture() const;
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
            void createVertexBuffers(const std::vector<Vertex> &vertices, CrsUploadBatch &upload);
            void createIndexBuffers(const std::vector<uint32_t> &indices, CrsUploadBatch &upload);
            std::unique_ptr<CrsBuffer> createDeviceLocalBuffer(
                const void *data, uint32_t elementSize, uint32_t count, VkBufferUsageFlags usage, CrsUploadBatch &upload);

            CrsDevice &crsDevice;

            std::unique_ptr<CrsBuffer> vertexBuffer;
            uint32_t vertexCount;

            std::unique_ptr<CrsBuffer> indexBuffer;  //vazio = modelo sem índices
            uint32_t indexCount;

            std::unique_ptr<CrsBuffer> edgeIndexBuffer;  //vazio = modelo sem arestas
            uint32_t edgeIndexCount = 0;

            std::unique_ptr<CrsBuffer> skinVertexBuffer;

            std::unique_ptr<CrsTexture> texture;

            glm::vec3 boundsMin{0.f};
            glm::vec3 boundsMax{0.f};

            std::optional<Skeleton> skeleton{};
            std::vector<AnimationClip> animations{};

            std::string filePath{};
    };
}
