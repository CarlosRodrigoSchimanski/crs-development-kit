#include "engine/rendering/systems/collision_render_system.hpp"

#include "engine/rendering/crs_swap_chain.hpp"

#include <cassert>

namespace crsdk {
    //tem que bater com o push do collision.vert/.frag
    struct CollisionPushConstants {
        glm::mat4 modelMatrix{1.f};
        glm::vec4 color{1.f};
        glm::vec4 options{0.f};  //x = 1: face (com luz), 0: aresta
    };

    //verde-água, como o contorno das caixas de física; o selecionado mais forte
    //o preenchimento bem leve: as faces da frente e de trás somam (e as peças umas atrás das outras)
    constexpr glm::vec4 FILL_COLOR{0.25f, 0.9f, 0.55f, 0.07f};
    constexpr glm::vec4 FILL_SELECTED{0.35f, 1.f, 0.6f, 0.14f};
    constexpr glm::vec4 EDGE_COLOR{0.4f, 0.95f, 0.6f, 0.55f};
    constexpr glm::vec4 EDGE_SELECTED{0.55f, 1.f, 0.7f, 1.f};
    //malha que ninguém desenhou por esses frames sai da GPU (já saiu de todo frame em voo)
    constexpr uint64_t FORGET_AFTER_FRAMES = CrsSwapChain::MAX_FRAMES_IN_FLIGHT + 1;

    CollisionRenderSystem::CollisionRenderSystem(CrsDevice &device, VkFormat colorFormat, VkFormat depthFormat,
                                                 VkDescriptorSetLayout globalSetLayout)
        : crsDevice{device} {
        //set 0 = global (câmera)
        pipelineLayout = CrsPipeline::createPipelineLayout(crsDevice, {globalSetLayout},
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(CollisionPushConstants));
        createPipelines(colorFormat, depthFormat);
    }

    void CollisionRenderSystem::createPipelines(VkFormat colorFormat, VkFormat depthFormat) {
        assert(pipelineLayout.get() != VK_NULL_HANDLE && "Cannot create pipeline before pipeline layout");
        PipelineConfigInfo config{};
        CrsPipeline::defaultPipelineConfigInfo(config);
        //só a posição (vec3) por ponto
        config.bindingDescriptions = {{0, sizeof(glm::vec3), VK_VERTEX_INPUT_RATE_VERTEX}};
        config.attributeDescriptions = {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}};
        CrsPipeline::enableAlphaBlending(config);
        //translúcido: testa a profundidade (atrás de um morro não aparece), mas não escreve
        config.depthStencilInfo.depthWriteEnable = VK_FALSE;
        //a colisão costuma ficar exatamente em cima da parede desenhada: puxada um pouquinho pra
        //frente, ela ganha sempre (sem piscar uma por cima da outra)
        config.rasterizationInfo.depthBiasEnable = VK_TRUE;
        config.rasterizationInfo.depthBiasConstantFactor = -4.f;
        config.rasterizationInfo.depthBiasSlopeFactor = -1.5f;
        config.colorAttachmentFormats = {colorFormat};
        config.depthAttachmentFormat = depthFormat;
        config.pipelineLayout = pipelineLayout.get();
        fillPipeline = std::make_unique<CrsPipeline>(crsDevice, "shaders/collision.vert.spv", "shaders/collision.frag.spv", config);

        //as arestas: o mesmo, de dois em dois índices como linha. O depth bias não vale pra linha: a
        //aresta em cima da aresta da parede passa no "menor ou igual"
        config.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        config.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        edgePipeline = std::make_unique<CrsPipeline>(crsDevice, "shaders/collision.vert.spv", "shaders/collision.frag.spv", config);
    }

    CollisionRenderSystem::GpuMesh &CollisionRenderSystem::gpuMesh(const std::shared_ptr<const CollisionMesh> &mesh) {
        GpuMesh &gpu = meshes[mesh.get()];
        if (!gpu.mesh) {
            //pouco dado e escrito uma vez: na memória que a CPU enxerga (sem cópia por staging)
            auto upload = [&](const void *data, VkDeviceSize elementSize, size_t count, VkBufferUsageFlags usage) {
                auto buffer = std::make_unique<CrsBuffer>(crsDevice, elementSize, static_cast<uint32_t>(count), usage,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                buffer->map();
                buffer->writeToBuffer(data);
                return buffer;
            };
            gpu.mesh = mesh;
            gpu.vertices = upload(mesh->vertices.data(), sizeof(glm::vec3), mesh->vertices.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
            gpu.indices = upload(mesh->indices.data(), sizeof(uint32_t), mesh->indices.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
            if (!mesh->edges.empty()) {
                gpu.edges = upload(mesh->edges.data(), sizeof(uint32_t), mesh->edges.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
            }
        }
        gpu.lastUsedFrame = frameCounter;
        return gpu;
    }

    void CollisionRenderSystem::render(FrameInfo &frameInfo, std::optional<CrsGameObject::id_t> selected) {
        frameCounter++;
        VkCommandBuffer cmd = frameInfo.commandBuffer;

        //os objetos com colisão do modelo que vale (sem o card Física: senão é ele que colide)
        struct Item {
            CrsGameObject *obj;
            GpuMesh *gpu;
        };
        std::vector<Item> items;
        for (auto &kv : frameInfo.crsScene.getGameObjects()) {
            CrsGameObject &obj = kv.second;
            if (obj.rigidBody || !obj.collisionMesh || obj.collisionMesh->empty()) continue;
            items.push_back({&obj, &gpuMesh(obj.collisionMesh)});
        }

        if (!items.empty()) {
            auto draw = [&](CrsPipeline &pipeline, bool edges) {
                pipeline.bind(cmd);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0, 1,
                    &frameInfo.globalDescriptorSet, 0, nullptr);
                for (const Item &item : items) {
                    const bool isSelected = selected && *selected == item.obj->getId();
                    CrsBuffer *indexBuffer = edges ? item.gpu->edges.get() : item.gpu->indices.get();
                    if (!indexBuffer) continue;
                    CollisionPushConstants push{};
                    push.modelMatrix = frameInfo.crsScene.worldMatrix(*item.obj);
                    push.color = edges ? (isSelected ? EDGE_SELECTED : EDGE_COLOR) : (isSelected ? FILL_SELECTED : FILL_COLOR);
                    push.options.x = edges ? 0.f : 1.f;
                    vkCmdPushConstants(cmd, pipelineLayout.get(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, sizeof(push), &push);
                    VkBuffer vertexBuffer = item.gpu->vertices->getBuffer();
                    VkDeviceSize offset = 0;
                    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &offset);
                    vkCmdBindIndexBuffer(cmd, indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
                    const auto count = edges ? item.gpu->mesh->edges.size() : item.gpu->mesh->indices.size();
                    vkCmdDrawIndexed(cmd, static_cast<uint32_t>(count), 1, 0, 0, 0);
                }
            };
            draw(*fillPipeline, false);
            draw(*edgePipeline, true);  //por cima do preenchimento
        }

        //malha de modelo que saiu da cena: os buffers saem quando nenhum frame em voo usa mais
        std::erase_if(meshes, [&](auto &kv) { return frameCounter - kv.second.lastUsedFrame > FORGET_AFTER_FRAMES; });
    }
}
