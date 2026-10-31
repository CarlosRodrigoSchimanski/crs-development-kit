#include "engine/scene/crs_game_object.hpp"

#include <algorithm>
#include <cmath>

namespace crsdk {

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

    void TransformComponent::applyRotation(const glm::vec3 &radians, bool local, const glm::mat3 &parentRotation) {
        TransformComponent delta{};
        delta.rotation = radians;
        const glm::mat3 current = rotationMatrix();
        const glm::mat3 turn = delta.rotationMatrix();
        //local: o giro vem depois da rotação atual (eixos do objeto). Mundo: o giro vem antes, visto de
        //dentro do pai (a inversa de uma rotação pura é a transposta)
        setRotationMatrix(local ? current * turn : glm::transpose(parentRotation) * turn * parentRotation * current);
    }

    void TransformComponent::setRotationMatrix(const glm::mat3 &rotationMatrix) {
        //de volta pros três ângulos, mantendo posição e escala
        glm::mat4 m{1.f};
        m[0] = glm::vec4{rotationMatrix[0] * scale.x, 0.f};
        m[1] = glm::vec4{rotationMatrix[1] * scale.y, 0.f};
        m[2] = glm::vec4{rotationMatrix[2] * scale.z, 0.f};
        m[3] = glm::vec4{translation, 1.f};
        setFromMatrix(m);
    }

    //meia medida da caixa que envolve a forma
    static glm::vec3 shapeHalfSize(const RigidBodyComponent &body) {
        switch (body.shape) {
            case ColliderShape::Sphere: return glm::vec3{body.radius};
            case ColliderShape::Capsule: return {body.radius, glm::max(body.height * 0.5f, body.radius), body.radius};
            case ColliderShape::Box: break;
        }
        return body.halfExtents;
    }

    glm::vec3 RigidBodyComponent::boundsMin() const { return center - shapeHalfSize(*this); }
    glm::vec3 RigidBodyComponent::boundsMax() const { return center + shapeHalfSize(*this); }

    void AnimatorComponent::play(const std::string &name, float fadeSeconds, bool loopClip) {
        if (name == clip) {
            loop = loopClip;
            return;
        }
        //a atual vira a anterior e vai sumindo; a nova começa do início
        previousClip = clip;
        previousTime = time;
        previousLoop = loop;
        clip = name;
        time = 0.f;
        duration = 0.f;  //ainda não se sabe: o SkinningSystem preenche quando achar a nova
        loop = loopClip;
        fadeDuration = fadeSeconds;
        fadeElapsed = 0.f;
    }

    bool AnimatorComponent::isBlending() const {
        return fadeDuration > 0.f;
    }

    float AnimatorComponent::blendWeight() const {
        if (!isBlending()) return 1.f;
        return std::min(fadeElapsed / fadeDuration, 1.f);
    }

    float AnimatorComponent::clipTime() const {
        if (duration <= 0.f) return time;
        return loop ? std::fmod(time, duration) : std::min(time, duration);
    }

    float AnimatorComponent::progress() const {
        if (duration <= 0.f) return 0.f;
        return clipTime() / duration;
    }

    bool AnimatorComponent::isFinished() const {
        return !loop && duration > 0.f && time >= duration;
    }

    int AnimatorComponent::frame(float fps) const {
        return static_cast<int>(std::floor(clipTime() * fps));
    }

    CrsGameObject::id_t CrsGameObject::getId() const { return id; }

    bool CrsGameObject::isAnimated() const {
        return model != nullptr && model->hasSkin() && animator.has_value();
    }

    CrsGameObject::CrsGameObject(id_t objId) : id{objId} {}
}
