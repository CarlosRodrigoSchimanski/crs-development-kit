#pragma once

#include "engine/scene/lve_game_object.hpp"
#include "engine/scene/lve_ray.hpp"

#include <optional>

namespace lve {
    //o que um raio acertou: o objeto e a distância até o ponto de impacto
    struct RayCastHit {
        LveGameObject::id_t id;
        float distance;
    };
    
    class LveScene{
        public:
            //cria um objeto ja dentro da sena e devolve ele para configurar
            LveGameObject& createGameObject();

            LveGameObject::Map& getGameObjects();
            const LveGameObject::Map& getGameObjects() const;

            void setActiveCamera(LveGameObject::id_t cameraId);
            std::optional<LveGameObject::id_t> getActiveCamera() const;
            
            LveGameObject* findGameObjectById(LveGameObject::id_t id);
            //matriz do objeto no mundo: a dos pais (do mais distante ao mais perto) x a dele
            glm::mat4 worldMatrix(const LveGameObject &obj);
            //objeto com modelo mais perto que o raio acerta (a caixa de cada modelo); vazio = nenhum
            std::optional<LveGameObject::id_t> pickObject(const Ray &ray);
            //raio contra as caixas de colisão (rigidBody): o acerto mais perto até maxDistance; ignoreId fica de fora
            std::optional<RayCastHit> raycast(const Ray &ray, float maxDistance, std::optional<LveGameObject::id_t> ignoreId = std::nullopt);
        
        private:
            LveGameObject::Map gameObjects;
            std::optional<LveGameObject::id_t> activeCameraId{};
            //proximo id livre desta cena
            LveGameObject::id_t nextId = 0;
    };
}