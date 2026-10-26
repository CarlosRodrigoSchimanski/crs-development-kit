#include "engine/scene/lve_game_object.hpp"

namespace lve {

    //rotação de Tait-Bryan na ordem Y, X, Z (yaw, pitch, roll); cada coluna é um eixo local do objeto
    glm::mat3 TransformComponent::rotationMatrix() const {
        const float c3 = glm::cos(rotation.z);
        const float s3 = glm::sin(rotation.z);
        const float c2 = glm::cos(rotation.x);
        const float s2 = glm::sin(rotation.x);
        const float c1 = glm::cos(rotation.y);
        const float s1 = glm::sin(rotation.y);
        return glm::mat3{
            {c1 * c3 + s1 * s2 * s3, c2 * s3, c1 * s2 * s3 - c3 * s1},
            {c3 * s1 * s2 - c1 * s3, c2 * c3, c1 * c3 * s2 + s1 * s3},
            {c2 * s1, -s2, c1 * c2}
        };
    }

    //translação * rotação * escala: cada eixo da rotação é esticado pela escala dele
    glm::mat4 TransformComponent::mat4() const {
        const glm::mat3 r = rotationMatrix();
        glm::mat4 m{1.f};
        m[0] = glm::vec4{r[0] * scale.x, 0.f};
        m[1] = glm::vec4{r[1] * scale.y, 0.f};
        m[2] = glm::vec4{r[2] * scale.z, 0.f};
        m[3] = glm::vec4{translation, 1.f};
        return m;
    }

    //igual à rotação do mat4, mas com a escala invertida: mantém as normais perpendiculares
    //à superfície mesmo quando o objeto é esticado de forma desigual
    glm::mat3 TransformComponent::normalMatrix() const {
        const glm::mat3 r = rotationMatrix();
        return glm::mat3{r[0] / scale.x, r[1] / scale.y, r[2] / scale.z};
    }

    void TransformComponent::setFromMatrix(const glm::mat4 &matrix) {
        translation = glm::vec3{matrix[3]};

        //cada coluna é um eixo do objeto
        glm::vec3 axisX{matrix[0]};
        glm::vec3 axisY{matrix[1]};
        glm::vec3 axisZ{matrix[2]};
        scale = {glm::length(axisX), glm::length(axisY), glm::length(axisZ)};

        //tirando a escala, sobra a rotação pura
        axisX /= scale.x;
        axisY /= scale.y;
        axisZ /= scale.z;

        //o inverso do rotationMatrix (ordem Y, X, Z): cada ângulo sai de dois números da matriz
        rotation.x = glm::asin(glm::clamp(-axisZ.y, -1.f, 1.f));
        rotation.y = glm::atan(axisZ.x, axisZ.z);
        rotation.z = glm::atan(axisX.y, axisY.y);
    }

    LveGameObject LveGameObject::makePointLight(float intensity, float radius, glm::vec3 color) {
        LveGameObject gameObject = LveGameObject::createGameObject();
        gameObject.color = color;
        gameObject.transform.scale.x = radius;
        gameObject.pointLight = PointLightComponent{};
        gameObject.pointLight->lightIntensity = intensity;
        return gameObject;
    }

    LveGameObject LveGameObject::createGameObject() {
        static id_t currentId = 0;
        return LveGameObject{currentId++};
    }

    LveGameObject::id_t LveGameObject::getId() const { return id; }

    LveGameObject::LveGameObject(id_t objId) : id{objId} {}
}
