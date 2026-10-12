#pragma once

#include <vector>
#include <memory>

#include "engine/core/lve_device.hpp"
#include "engine/rendering/lve_buffer.hpp"
#include "engine/rendering/lve_texture.hpp"

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

                bool operator==(const Vertex &other) const;
            };

            struct Builder {
                std::vector<Vertex> vertices{};
                std::vector<uint32_t> indices{};
                //imagem de cor do material (png/jpg ainda codificado); vazio = modelo sem textura
                std::vector<unsigned char> baseColorImage{};

                //escolhe o carregador pela extensão (.obj ou .gltf/.glb)
                void loadModel(const std::string &filepath);

                private:
                void loadObj(const std::string &filepath);
                void loadGltf(const std::string &filepath);
            };

            LveModel(LveDevice &device, const LveModel::Builder &builder);
            ~LveModel();

            LveModel(const LveModel&) = delete;
            LveModel& operator=(const LveModel&) = delete;

            static std::unique_ptr<LveModel> createModelFromFile(LveDevice &device, const std::string &filepath);

            void bind(VkCommandBuffer commandBuffer);
            void draw(VkCommandBuffer commandBuffer);

            //nullptr se o modelo não tem textura
            LveTexture *getTexture() const;

        private:
            void createVertexBuffers(const std::vector<Vertex> &vertices);
            void createIndexBuffers(const std::vector<uint32_t> &indices);

            LveDevice &lveDevice;

            std::unique_ptr<LveBuffer> vertexBuffer;
            uint32_t vertexCount;

            bool hasIndexBuffer = false;
            std::unique_ptr<LveBuffer> indexBuffer;
            uint32_t indexCount;

            std::unique_ptr<LveTexture> texture;
    };
}
