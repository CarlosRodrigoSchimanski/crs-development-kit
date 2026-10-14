#include "engine/rendering/lve_model.hpp"
#include "engine/core/lve_utils.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#include <cassert>
#include <unordered_map>
#include <filesystem>
#include <stdexcept>

namespace std {
    template<>
    struct hash<lve::LveModel::Vertex> {
        size_t operator()(lve::LveModel::Vertex const &vertex) const{
            size_t seed = 0;
            lve::hashCombine(seed, vertex.position, vertex.color, vertex.normal, vertex.uv);
            return seed;
        }
    };
    
}


namespace lve {

    LveModel::LveModel(LveDevice& device, const LveModel::Builder &builder) : lveDevice{device} {
        createVertexBuffers(builder.vertices);
        createIndexBuffers(builder.indices);
        if (!builder.baseColorImage.empty()) {
            texture = std::make_unique<LveTexture>(device, builder.baseColorImage);
        }
    }

    LveTexture *LveModel::getTexture() const { return texture.get(); }

    std::unique_ptr<LveModel> LveModel::createModelFromFile(LveDevice &device, const std::string &filepath) {
        Builder builder{};
        builder.loadModel(filepath);
        return std::make_unique<LveModel>(device, builder);
    }

    void LveModel::createVertexBuffers(const std::vector<Vertex>& vertices) {
        vertexCount = static_cast<uint32_t>(vertices.size());
        assert(vertexCount >= 3 && "Vertex count must be at least 3");

        vertexBuffer = createDeviceLocalBuffer(
            vertices.data(), sizeof(Vertex), vertexCount, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    }

    void LveModel::createIndexBuffers(const std::vector<uint32_t> &indices) {
        indexCount = static_cast<uint32_t>(indices.size());
        //sem índices o indexBuffer fica vazio, e bind/draw usam os vértices direto
        if (indexCount == 0) {
            return;
        }

        indexBuffer = createDeviceLocalBuffer(
            indices.data(), sizeof(uint32_t), indexCount, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    }

    //dado escrito uma vez e lido muitas pela GPU: passa por um staging e fica na memória DEVICE_LOCAL
    std::unique_ptr<LveBuffer> LveModel::createDeviceLocalBuffer(
        const void *data, uint32_t elementSize, uint32_t count, VkBufferUsageFlags usage) {
        LveBuffer stagingBuffer{
            lveDevice,
            elementSize,
            count,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        };
        stagingBuffer.map();
        stagingBuffer.writeToBuffer(data);

        auto buffer = std::make_unique<LveBuffer>(
            lveDevice,
            elementSize,
            count,
            usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        lveDevice.copyBuffer(stagingBuffer.getBuffer(), buffer->getBuffer(), static_cast<VkDeviceSize>(elementSize) * count);
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
        //limpa aqui, uma vez só, em vez de cada carregador lembrar de limpar
        vertices.clear();
        indices.clear();
        baseColorImage.clear();

        std::string extension = std::filesystem::path(filepath).extension().string();
        if (extension == ".obj") {
            loadObj(filepath);
        } else if (extension == ".gltf" || extension == ".glb") {
            loadGltf(filepath);
        } else {
            throw std::runtime_error("unsupported model format: " + filepath);
        }
    }

    void LveModel::Builder::loadGltf(const std::string &filepath) {
        cgltf_options options{};
        cgltf_data *data = nullptr;
        if (cgltf_parse_file(&options, filepath.c_str(), &data) != cgltf_result_success) {
            throw std::runtime_error("failed to parse glTF: " + filepath);
        }
        //o parse só lê a estrutura (JSON); os dados dos vértices ficam nos buffers (.bin ou dentro do .glb)
        if (cgltf_load_buffers(&options, data, filepath.c_str()) != cgltf_result_success) {
            cgltf_free(data);
            throw std::runtime_error("failed to load glTF buffers: " + filepath);
        }

        //glTF é Y pra cima, a engine é Y pra baixo: gira 180 graus em X, (x, y, z) -> (x, -y, -z).
        //só negar o Y espelharia o modelo (troca a "mão" do sistema e inverte a ordem dos triângulos)
        const glm::mat4 yUpToYDown = glm::rotate(glm::mat4{1.f}, glm::pi<float>(), glm::vec3{1.f, 0.f, 0.f});

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

        cgltf_free(data);
    }

    void LveModel::Builder::loadObj(const std::string &filepath) {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        if(!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str())) {
            throw std::runtime_error(warn + err);
        }

        std::unordered_map<Vertex, uint32_t> uniqueVertices{};
        for(const auto &shape : shapes) {
            for(const auto &index : shape.mesh.indices) {
                Vertex vertex{};

                if(index.vertex_index >= 0) {
                    vertex.position = {
                        attrib.vertices[3 * index.vertex_index + 0],
                        attrib.vertices[3 * index.vertex_index + 1],
                        attrib.vertices[3 * index.vertex_index + 2]
                    };

                    vertex.color = {
                        attrib.colors[3 * index.vertex_index + 0],
                        attrib.colors[3 * index.vertex_index + 1],
                        attrib.colors[3 * index.vertex_index + 2]
                    };
                    
                }

                if(index.normal_index >= 0) {
                    vertex.normal = {
                        attrib.normals[3 * index.normal_index + 0],
                        attrib.normals[3 * index.normal_index + 1],
                        attrib.normals[3 * index.normal_index + 2]
                    };
                }

                if(index.texcoord_index >= 0) {
                    vertex.uv = {
                        attrib.texcoords[2 * index.texcoord_index + 0],
                        attrib.texcoords[2 * index.texcoord_index + 1]
                    };
                }

                if (uniqueVertices.count(vertex) == 0) {
                    uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                    vertices.push_back(vertex);
                }
                indices.push_back(uniqueVertices[vertex]);
            }
        }
    }

}