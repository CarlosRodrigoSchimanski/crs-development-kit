#pragma once

#include "engine/rendering/lve_model.hpp"

#include <memory>
#include <unordered_map>
#include <optional>


namespace lve {
    struct CameraComponent{
        float fovY = glm::radians(50.f);
        float nearPlane = 0.1f;
        float farPlane = 100.f;
    };

    struct TransformComponent
    {
        glm::vec3 translation{};
        glm::vec3 scale{1.f,1.f,1.f};
        glm::vec3 rotation{};

        glm::mat3 rotationMatrix() const;
        glm::mat4 mat4() const;
        glm::mat3 normalMatrix() const;
    };

    struct PointLightComponent {
        float lightIntensity = 1.f;  
    };
    

    class LveGameObject{

        public:
        using id_t = unsigned int;
        using Map = std::unordered_map<id_t, LveGameObject>;

        static LveGameObject createGameObject();
        static LveGameObject makePointLight(float intensity = 10.f, float radius = 0.1f, glm::vec3 color = glm::vec3{1.f});

        LveGameObject(const LveGameObject &) = default;
        LveGameObject &operator=(const LveGameObject &) = default;
        LveGameObject(LveGameObject &&) = default;
        LveGameObject &operator=(LveGameObject &&) = default;

        id_t getId() const;

        glm::vec3 color{};
        //false = usa a textura branca em vez da do modelo (fica só a cor do vértice)
        bool useTexture{true};
        bool editorOnly{false};
        TransformComponent transform{};
        
        std::shared_ptr<LveModel>model{};
        std::optional<PointLightComponent> pointLight{};
        std::optional<CameraComponent> camera{};

        
        private:
        LveGameObject(id_t objId);

        id_t id;
    };
}