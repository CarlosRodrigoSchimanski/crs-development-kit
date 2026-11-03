#pragma once

#include "engine/core/crs_device.hpp"
#include "engine/rendering/crs_buffer.hpp"
#include "engine/rendering/crs_frameInfo.hpp"
#include "engine/rendering/crs_pipeline.hpp"
#include "engine/scene/crs_collision_mesh.hpp"

#include <memory>
#include <optional>
#include <unordered_map>

namespace crsdk {
    //desenha no editor a colisão que veio nos modelos (malhas UCX_): uma superfície translúcida com um
    //pouco de luz pela inclinação das faces, e as arestas por cima. Fica um pouco à frente da parede
    //desenhada (sem piscar quando as duas coincidem). No Play não é chamado: a colisão é invisível
    class CollisionRenderSystem {
        public:
            CollisionRenderSystem(CrsDevice &device, VkFormat colorFormat, VkFormat depthFormat, VkDescriptorSetLayout globalSetLayout);

            CollisionRenderSystem(const CollisionRenderSystem &) = delete;
            CollisionRenderSystem &operator=(const CollisionRenderSystem &) = delete;

            //selected: o objeto selecionado no editor (a colisão dele fica mais forte)
            void render(FrameInfo &frameInfo, std::optional<CrsGameObject::id_t> selected);

        private:
            void createPipelines(VkFormat colorFormat, VkFormat depthFormat);

            //a malha na GPU: uma vez por malha, dividida por todos os objetos do mesmo modelo
            struct GpuMesh {
                std::shared_ptr<const CollisionMesh> mesh;  //segura viva enquanto os buffers existirem
                std::unique_ptr<CrsBuffer> vertices;
                std::unique_ptr<CrsBuffer> indices;
                std::unique_ptr<CrsBuffer> edges;  //vazio = sem arestas
                uint64_t lastUsedFrame = 0;
            };
            GpuMesh &gpuMesh(const std::shared_ptr<const CollisionMesh> &mesh);

            CrsDevice &crsDevice;
            CrsPipelineLayout pipelineLayout;
            std::unique_ptr<CrsPipeline> fillPipeline;
            std::unique_ptr<CrsPipeline> edgePipeline;
            std::unordered_map<const CollisionMesh *, GpuMesh> meshes;
            uint64_t frameCounter = 0;
    };
}
