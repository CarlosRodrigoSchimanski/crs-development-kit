#pragma once

#include <glm/glm.hpp>
#include <optional>

namespace lve {
    //um ponto de partida e uma direção: os pontos do raio são origem + direção * tempo
    struct Ray {
        glm::vec3 origin{0.f};
        glm::vec3 direction{0.f, 0.f, 1.f};
    };

    //tempo onde o raio entra na caixa, vazio = não acerta
    std::optional<float> intersectRayAabb(const Ray &ray, glm::vec3 boxMin, glm::vec3 boxMax);
    //raio contra a caixa local de um objeto, o tempo vale no mundo
    std::optional<float> intersectRayObject(const Ray &ray, const glm::mat4 &world, glm::vec3 localMin, glm::vec3 localMax);
    //raio que sai da câmera e passa por um pixel da tela (o clique do mouse)
    Ray screenPointToRay(glm::vec2 pixel, glm::vec2 screenSize, const glm::mat4 &projection, const glm::mat4 &view);

}