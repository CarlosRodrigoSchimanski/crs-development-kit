#include "engine/rendering/crs_instance_batches.hpp"

#include <algorithm>
#include <map>
#include <utility>

namespace crsdk {

    CrsInstanceBatches::CrsInstanceBatches(CrsDevice &device) : crsDevice{device} {}

    const std::vector<CrsInstanceBatches::Batch> &CrsInstanceBatches::build(CrsScene &scene, int frameIndex) {
        //grupos pela chave (modelo, usa textura): a textura vem do modelo, então isso basta
        std::map<std::pair<const CrsModel *, bool>, std::vector<CrsGameObject *>> groups;
        for (auto &kv : scene.getGameObjects()) {
            CrsGameObject &obj = kv.second;
            if (obj.model == nullptr || obj.editorOnly || obj.isAnimated()) continue;
            groups[{obj.model.get(), obj.useTexture}].push_back(&obj);
        }

        batches.clear();
        instances.clear();
        for (auto &group : groups) {
            batches.push_back({group.second.front(), static_cast<uint32_t>(instances.size()), static_cast<uint32_t>(group.second.size())});
            for (CrsGameObject *obj : group.second) {
                const glm::mat4 world = scene.worldMatrix(*obj);
                //normais com a escala de todos os pais: inversa transposta da parte 3x3 do mundo
                instances.push_back({world, glm::mat4{glm::transpose(glm::inverse(glm::mat3{world}))}});
            }
        }
        if (instances.empty()) return batches;

        //a lista deste frame cresce quando falta lugar (com folga, pra não refazer a cada objeto novo)
        const size_t frame = static_cast<size_t>(frameIndex);
        if (!buffers[frame] || capacity[frame] < instances.size()) {
            capacity[frame] = std::max<uint32_t>(static_cast<uint32_t>(instances.size()) * 2, 64);
            buffers[frame] = std::make_unique<CrsBuffer>(crsDevice, sizeof(InstanceData), capacity[frame],
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            buffers[frame]->map();
        }
        buffers[frame]->writeToBuffer(instances.data(), instances.size() * sizeof(InstanceData));
        return batches;
    }

    void CrsInstanceBatches::bind(VkCommandBuffer commandBuffer, int frameIndex, uint32_t binding) const {
        const auto &buffer = buffers[static_cast<size_t>(frameIndex)];
        if (!buffer) return;
        VkBuffer vkBuffer = buffer->getBuffer();
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(commandBuffer, binding, 1, &vkBuffer, &offset);
    }

    VkVertexInputBindingDescription CrsInstanceBatches::bindingDescription(uint32_t binding) {
        //INSTANCE: anda uma posição por cópia desenhada, não por vértice
        return {binding, sizeof(InstanceData), VK_VERTEX_INPUT_RATE_INSTANCE};
    }

    std::vector<VkVertexInputAttributeDescription> CrsInstanceBatches::attributeDescriptions(bool withNormalMatrix, uint32_t binding) {
        //uma mat4 de entrada ocupa 4 locations (uma por coluna)
        std::vector<VkVertexInputAttributeDescription> attributes;
        for (uint32_t column = 0; column < 4; column++) {
            attributes.push_back({4 + column, binding, VK_FORMAT_R32G32B32A32_SFLOAT,
                static_cast<uint32_t>(offsetof(InstanceData, modelMatrix) + column * sizeof(glm::vec4))});
        }
        if (withNormalMatrix) {
            for (uint32_t column = 0; column < 4; column++) {
                attributes.push_back({8 + column, binding, VK_FORMAT_R32G32B32A32_SFLOAT,
                    static_cast<uint32_t>(offsetof(InstanceData, normalMatrix) + column * sizeof(glm::vec4))});
            }
        }
        return attributes;
    }
}
