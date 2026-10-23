#include "engine/scene/lve_ray.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace lve {

    std::optional<float> intersectRayAabb(const Ray &ray, glm::vec3 boxMin, glm::vec3 boxMax) {
        float tEnter = 0.f;  //só pra frente da origem
        float tExit = std::numeric_limits<float>::max();

        for (int axis = 0; axis < 3; axis++) {
            if (std::abs(ray.direction[axis]) < 1e-8f) {
                //paralelo a essa fatia: só acerta se a origem já estiver dentro dela
                if (ray.origin[axis] < boxMin[axis] || ray.origin[axis] > boxMax[axis]) return std::nullopt;
                continue;
            }
            //onde o raio entra e sai da fatia deste eixo
            float t1 = (boxMin[axis] - ray.origin[axis]) / ray.direction[axis];
            float t2 = (boxMax[axis] - ray.origin[axis]) / ray.direction[axis];
            if (t1 > t2) std::swap(t1, t2);

            tEnter = std::max(tEnter, t1);
            tExit = std::min(tExit, t2);
            if (tEnter > tExit) return std::nullopt;  //nunca dentro das três fatias ao mesmo tempo
        }
        return tEnter;
    }

    std::optional<float> intersectRayObject(const Ray &ray, const glm::mat4 &world, glm::vec3 localMin, glm::vec3 localMax) {
        //leva o raio pro espaço do objeto (w = 0 na direção: ela só gira e escala, não anda)
        glm::mat4 toLocal = glm::inverse(world);
        Ray local{
            glm::vec3{toLocal * glm::vec4{ray.origin, 1.f}},
            glm::vec3{toLocal * glm::vec4{ray.direction, 0.f}}};
        return intersectRayAabb(local, localMin, localMax);
    }

    Ray screenPointToRay(glm::vec2 pixel, glm::vec2 screenSize, const glm::mat4 &projection, const glm::mat4 &view) {
        //pixel -> -1..1 nos dois eixos (o Y do Vulkan aponta pra baixo, igual ao do mouse)
        glm::vec2 ndc = pixel / screenSize * 2.f - 1.f;

        //o caminho da câmera ao contrário: da tela de volta pro mundo
        glm::mat4 screenToWorld = glm::inverse(projection * view);
        glm::vec4 nearPoint = screenToWorld * glm::vec4{ndc, 0.f, 1.f};  //profundidade 0 = perto
        glm::vec4 farPoint = screenToWorld * glm::vec4{ndc, 1.f, 1.f};   //profundidade 1 = longe
        //divide pelo w: desfaz a perspectiva
        glm::vec3 nearWorld = glm::vec3{nearPoint} / nearPoint.w;
        glm::vec3 farWorld = glm::vec3{farPoint} / farPoint.w;

        return Ray{nearWorld, glm::normalize(farWorld - nearWorld)};
    }

}
