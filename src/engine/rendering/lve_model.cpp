#include "engine/rendering/lve_model.hpp"

#include <cgltf.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cassert>
#include <stdexcept>
#include <map>
#include <tuple>
#include <utility>


namespace lve {

    LveModel::LveModel(LveDevice& device, const LveModel::Builder &builder) : lveDevice{device} {
        //vértices, índices e textura sobem pra GPU no mesmo envio (1 espera em vez de 3)
        LveUploadBatch upload{device};
        createVertexBuffers(builder.vertices, upload);
        createIndexBuffers(builder.indices, upload);

        edgeIndexCount = static_cast<uint32_t>(builder.edgeIndices.size());
        if (edgeIndexCount > 0) {
            edgeIndexBuffer = createDeviceLocalBuffer(
                builder.edgeIndices.data(), sizeof(uint32_t), edgeIndexCount, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, upload);
        }

        if (!builder.baseColorImage.empty()) {
            texture = std::make_unique<LveTexture>(device, builder.baseColorImage, upload);
        }

        upload.submit();
    }

    LveTexture *LveModel::getTexture() const { return texture.get(); }

    std::unique_ptr<LveModel> LveModel::createModelFromFile(LveDevice &device, const std::string &filepath) {
        Builder builder{};
        builder.loadModel(filepath);
        return std::make_unique<LveModel>(device, builder);
    }

    void LveModel::createVertexBuffers(const std::vector<Vertex>& vertices, LveUploadBatch &upload) {
        vertexCount = static_cast<uint32_t>(vertices.size());
        assert(vertexCount >= 3 && "Vertex count must be at least 3");

        vertexBuffer = createDeviceLocalBuffer(
            vertices.data(), sizeof(Vertex), vertexCount, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, upload);
    }

    void LveModel::createIndexBuffers(const std::vector<uint32_t> &indices, LveUploadBatch &upload) {
        indexCount = static_cast<uint32_t>(indices.size());
        //sem índices o indexBuffer fica vazio, e bind/draw usam os vértices direto
        if (indexCount == 0) {
            return;
        }

        indexBuffer = createDeviceLocalBuffer(
            indices.data(), sizeof(uint32_t), indexCount, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, upload);
    }

    //dado escrito uma vez e lido muitas pela GPU: fica na memória DEVICE_LOCAL (a mais rápida pra GPU,
    //mas a CPU não enxerga); o lote copia pra ela a partir de um staging buffer
    std::unique_ptr<LveBuffer> LveModel::createDeviceLocalBuffer(
        const void *data, uint32_t elementSize, uint32_t count, VkBufferUsageFlags usage, LveUploadBatch &upload) {
        auto buffer = std::make_unique<LveBuffer>(
            lveDevice,
            elementSize,
            count,
            usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        //só grava a cópia: ela acontece de fato no upload.submit()
        upload.copyToBuffer(buffer->getBuffer(), data, static_cast<VkDeviceSize>(elementSize) * count);
        return buffer;
    }

    void LveModel::bind(VkCommandBuffer commandBuffer) {
        VkBuffer buffers[] = {vertexBuffer->getBuffer()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

        if(indexBuffer) {
            vkCmdBindIndexBuffer(commandBuffer, indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
        }
    }

    void LveModel::draw(VkCommandBuffer commandBuffer) {
        if(indexBuffer) {
            vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
        } else {
            vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
        }
    }

    void LveModel::drawEdges(VkCommandBuffer commandBuffer) {
        if (!edgeIndexBuffer) return;
        //mesmos vértices dos triângulos; só troca a lista de índices
        VkBuffer buffers[] = {vertexBuffer->getBuffer()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, edgeIndexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, edgeIndexCount, 1, 0, 0, 0);
    }

    std::vector<VkVertexInputBindingDescription> LveModel::Vertex::getBindingDescriptions() {
        std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
        bindingDescriptions[0].binding = 0;
        bindingDescriptions[0].stride = sizeof(Vertex);
        bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescriptions;
    }

    std::vector<VkVertexInputAttributeDescription> LveModel::Vertex::getAttributeDescriptions() {
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
        attributeDescriptions.push_back({0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex,position)});
        attributeDescriptions.push_back({1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex,color)});
        attributeDescriptions.push_back({2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex,normal)});
        attributeDescriptions.push_back({3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex,uv)});

        return attributeDescriptions;
    }

    void LveModel::Builder::loadModel(const std::string &filepath) {
        vertices.clear();
        indices.clear();
        baseColorImage.clear();

        cgltf_options options{};
        cgltf_data *rawData = nullptr;
        if (cgltf_parse_file(&options, filepath.c_str(), &rawData) != cgltf_result_success) {
            throw std::runtime_error("failed to parse glTF: " + filepath);
        }
        //RAII pra um recurso de biblioteca C: o unique_ptr chama cgltf_free sozinho ao sair da função,
        //por qualquer caminho (fim normal, throw abaixo ou exceção no meio do carregamento)
        std::unique_ptr<cgltf_data, decltype(&cgltf_free)> data{rawData, &cgltf_free};

        //o parse só lê a estrutura (JSON); os dados dos vértices ficam nos buffers (.bin ou dentro do .glb)
        if (cgltf_load_buffers(&options, data.get(), filepath.c_str()) != cgltf_result_success) {
            throw std::runtime_error("failed to load glTF buffers: " + filepath);
        }
        //arquivo é entrada não confiável: confere se tamanhos, índices e referências batem entre si
        //(ex: accessor dizendo ter mais elementos do que cabe no buffer) antes de ler qualquer vértice
        if (cgltf_validate(data.get()) != cgltf_result_success) {
            throw std::runtime_error("invalid glTF: " + filepath);
        }

        //glTF é Y pra cima, a engine é Y pra baixo: gira 180 graus em X, (x, y, z) -> (x, -y, -z).
        //só negar o Y espelharia o modelo (troca a "mão" do sistema e inverte a ordem dos triângulos);
        //negar Y e Z juntos é exatamente essa rotação, sem o erro de float do sin(pi) != 0
        const glm::mat4 yUpToYDown = glm::scale(glm::mat4{1.f}, glm::vec3{1.f, -1.f, -1.f});

        //glTF é uma cena: cada nó tem sua transformação (e pode ser filho de outro).
        //por enquanto achata tudo numa malha só, já com a transformação aplicada nos vértices
        for (cgltf_size n = 0; n < data->nodes_count; n++) {
            const cgltf_node &node = data->nodes[n];
            if (node.mesh == nullptr) continue;

            float nodeMatrix[16];
            cgltf_node_transform_world(&node, nodeMatrix);  //já inclui as transformações dos pais
            glm::mat4 world = yUpToYDown * glm::make_mat4(nodeMatrix);
            glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3{world}));

            for (cgltf_size p = 0; p < node.mesh->primitives_count; p++) {
                const cgltf_primitive &primitive = node.mesh->primitives[p];
                if (primitive.type != cgltf_primitive_type_triangles) continue;

                const cgltf_accessor *positions = nullptr;
                const cgltf_accessor *normals = nullptr;
                const cgltf_accessor *texcoords = nullptr;
                const cgltf_accessor *colors = nullptr;
                for (cgltf_size a = 0; a < primitive.attributes_count; a++) {
                    const cgltf_attribute &attribute = primitive.attributes[a];
                    switch (attribute.type) {
                        case cgltf_attribute_type_position: positions = attribute.data; break;
                        case cgltf_attribute_type_normal: normals = attribute.data; break;
                        case cgltf_attribute_type_texcoord: if (attribute.index == 0) texcoords = attribute.data; break;
                        case cgltf_attribute_type_color: if (attribute.index == 0) colors = attribute.data; break;
                        default: break;
                    }
                }
                if (positions == nullptr) continue;

                //cor base do material (o "Base Color" do Blender) vira a cor do vértice
                glm::vec3 baseColor{1.f};
                if (primitive.material != nullptr && primitive.material->has_pbr_metallic_roughness) {
                    const cgltf_float *factor = primitive.material->pbr_metallic_roughness.base_color_factor;
                    baseColor = {factor[0], factor[1], factor[2]};

                    //textura de cor embutida no arquivo (por enquanto 1 por modelo: a do primeiro material que tiver)
                    const cgltf_texture *colorTexture = primitive.material->pbr_metallic_roughness.base_color_texture.texture;
                    if (baseColorImage.empty() && colorTexture != nullptr && colorTexture->image != nullptr
                        && colorTexture->image->buffer_view != nullptr) {
                        const cgltf_buffer_view *view = colorTexture->image->buffer_view;
                        const unsigned char *bytes = static_cast<const unsigned char *>(view->buffer->data) + view->offset;
                        baseColorImage.assign(bytes, bytes + view->size);
                    }
                }

                //os índices de cada primitiva começam do 0; somamos onde os vértices dela começam
                uint32_t baseVertex = static_cast<uint32_t>(vertices.size());

                for (cgltf_size i = 0; i < positions->count; i++) {
                    Vertex vertex{};
                    float value[4] = {0.f, 0.f, 0.f, 1.f};

                    cgltf_accessor_read_float(positions, i, value, 3);
                    vertex.position = glm::vec3{world * glm::vec4{value[0], value[1], value[2], 1.f}};

                    if (normals != nullptr) {
                        cgltf_accessor_read_float(normals, i, value, 3);
                        vertex.normal = glm::normalize(normalMatrix * glm::vec3{value[0], value[1], value[2]});
                    }

                    //o UV do glTF já tem origem no canto de cima, igual ao Vulkan: não precisa inverter
                    if (texcoords != nullptr) {
                        cgltf_accessor_read_float(texcoords, i, value, 2);
                        vertex.uv = {value[0], value[1]};
                    }

                    vertex.color = baseColor;
                    if (colors != nullptr) {
                        float color[4] = {1.f, 1.f, 1.f, 1.f};
                        cgltf_accessor_read_float(colors, i, color, 4);
                        vertex.color *= glm::vec3{color[0], color[1], color[2]};
                    }

                    vertices.push_back(vertex);
                }

                if (primitive.indices != nullptr) {
                    for (cgltf_size i = 0; i < primitive.indices->count; i++) {
                        indices.push_back(baseVertex + static_cast<uint32_t>(cgltf_accessor_read_index(primitive.indices, i)));
                    }
                } else {
                    //sem índices: os vértices já estão em ordem de triângulo
                    for (cgltf_size i = 0; i < positions->count; i++) {
                        indices.push_back(baseVertex + static_cast<uint32_t>(i));
                    }
                }
            }
        }
        buildEdges();
    }

    //aresta real: borda da malha, ou onde as faces vizinhas dobram mais que este ângulo
    constexpr float EDGE_ANGLE_DEGREES = 30.f;

    void LveModel::Builder::buildEdges() {
        edgeIndices.clear();

        //solda: vértices na mesma posição viram um id só (o glb duplica vértice nas costuras)
        std::map<std::tuple<float, float, float>, uint32_t> positionIds;
        std::vector<uint32_t> weld(vertices.size());
        for (size_t i = 0; i < vertices.size(); i++) {
            const glm::vec3 &p = vertices[i].position;
            auto result = positionIds.try_emplace({p.x, p.y, p.z}, static_cast<uint32_t>(positionIds.size()));
            weld[i] = result.first->second;
        }

        //cada aresta guarda as normais das faces que a usam
        struct EdgeFaces {
            uint32_t a = 0;  //índices originais, pra desenhar
            uint32_t b = 0;
            std::vector<glm::vec3> normals;
        };
        std::map<std::pair<uint32_t, uint32_t>, EdgeFaces> edges;

        for (size_t t = 0; t + 2 < indices.size(); t += 3) {
            uint32_t tri[3] = {indices[t], indices[t + 1], indices[t + 2]};
            glm::vec3 normal = glm::cross(
                vertices[tri[1]].position - vertices[tri[0]].position,
                vertices[tri[2]].position - vertices[tri[0]].position);
            if (glm::length(normal) < 1e-12f) continue;  //triângulo degenerado (sem área)
            normal = glm::normalize(normal);

            for (int e = 0; e < 3; e++) {
                uint32_t a = tri[e];
                uint32_t b = tri[(e + 1) % 3];
                //a mesma aresta tem que dar a mesma chave nos dois triângulos: menor id primeiro
                uint32_t wa = weld[a];
                uint32_t wb = weld[b];
                std::pair<uint32_t, uint32_t> key = wa < wb ? std::pair{wa, wb} : std::pair{wb, wa};

                EdgeFaces &edge = edges[key];
                if (edge.normals.empty()) {
                    edge.a = a;
                    edge.b = b;
                }
                edge.normals.push_back(normal);
            }
        }

        const float cosLimit = glm::cos(glm::radians(EDGE_ANGLE_DEGREES));
        for (auto &[key, edge] : edges) {
            //borda (1 face), junção com 3+ faces, ou dobra maior que o limite
            bool real = edge.normals.size() != 2 || glm::dot(edge.normals[0], edge.normals[1]) < cosLimit;
            if (real) {
                edgeIndices.push_back(edge.a);
                edgeIndices.push_back(edge.b);
            }
        }
    }

}