#pragma once

#include "engine/rendering/lve_model.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <memory>
#include <unordered_map>


namespace lve {
    struct TransformComponent
    {
        glm::vec3 translation{};
        glm::vec3 scale{1.f,1.f,1.f};
        glm::vec3 rotation{};

        glm::mat4 mat4();
        glm::mat3 normalMatrix();
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

        LveGameObject(const LveGameObject &) = delete;
        LveGameObject &operator=(const LveGameObject &) = delete;
        LveGameObject(LveGameObject &&) = default;
        LveGameObject &operator=(LveGameObject &&) = default;

        id_t getId();

        glm::vec3 color{};
        //false = shader ignora a textura global e usa só a cor do vértice
        bool useTexture{true};
        TransformComponent transform{};
        
        std::shared_ptr<LveModel>model{};
        std::unique_ptr<PointLightComponent> pointLight = nullptr;

        
        private:
        LveGameObject(id_t objId);

        id_t id;
    };
}