#pragma once

#include <glm/glm.hpp>

namespace lve {
    class LveCamera {
        public:
            void setPerspectiveProjection(
                float fovy,
                float aspect,
                float near,
                float far
            );

            void setViewDirection(
                glm::vec3 position,
                glm::vec3 direction,
                glm::vec3 up = glm::vec3{0.f, -1.f, 0.f}
            );

            void setViewTarget(
                glm::vec3 position,
                glm::vec3 target,
                glm::vec3 up = glm::vec3{0.f, -1.f, 0.f}
            );

            void setOrthographicProjection(
                float left,
                float right,
                float top,
                float bottom,
                float near,
                float far
            );

            //as matrizes são guardadas: devolve referência (sem cópia; o const impede mexer na da câmera)
            const glm::mat4 &getProjection() const;
            const glm::mat4 &getView() const;
            const glm::mat4 &getInverseView() const;
            //a posição é calculada na hora: devolve uma cópia
            glm::vec3 getPosition() const;

        private:
            glm::mat4 projectionMatrix{1.f};
            glm::mat4 viewMatrix{1.f};
            glm::mat4 inverseViewMatrix{1.f};
    };
}
