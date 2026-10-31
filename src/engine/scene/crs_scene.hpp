#pragma once

#include "engine/scene/crs_game_object.hpp"
#include "engine/scene/crs_ray.hpp"

#include <optional>

namespace crsdk {
    //o que um raio acertou: o objeto e a distância até o ponto de impacto
    struct RayCastHit {
        CrsGameObject::id_t id;
        float distance;
    };
    
    class CrsScene{
        public:
            //cria um objeto ja dentro da sena e devolve ele para configurar
            CrsGameObject& createGameObject();

            //cria com um id escolido
            CrsGameObject& createGameObject(CrsGameObject::id_t id);

            CrsGameObject::Map& getGameObjects();
            const CrsGameObject::Map& getGameObjects() const;

            void setActiveCamera(CrsGameObject::id_t cameraId);
            std::optional<CrsGameObject::id_t> getActiveCamera() const;
            
            CrsGameObject* findGameObjectById(CrsGameObject::id_t id);
            //apaga o objeto e todos os filhos dele (e os filhos dos filhos); câmera ativa apagada = nenhuma
            void destroyGameObject(CrsGameObject::id_t id);
            //matriz do objeto no mundo: a dos pais (do mais distante ao mais perto) x a dele
            glm::mat4 worldMatrix(const CrsGameObject &obj);
            //objeto com modelo mais perto que o raio acerta (a caixa de cada modelo); vazio = nenhum
            std::optional<CrsGameObject::id_t> pickObject(const Ray &ray);
            //raio contra as caixas de colisão (rigidBody) e os terrenos: o acerto mais perto até maxDistance; ignoreId fica de fora
            std::optional<RayCastHit> raycast(const Ray &ray, float maxDistance, std::optional<CrsGameObject::id_t> ignoreId = std::nullopt);
            //altura do chão no ponto (x, z) do mundo: o primeiro terreno que cobre o ponto; vazio = nenhum
            std::optional<float> terrainHeightAt(float x, float z);
        
        private:
            CrsGameObject::Map gameObjects;
            std::optional<CrsGameObject::id_t> activeCameraId{};
            //proximo id livre desta cena
            CrsGameObject::id_t nextId = 0;
    };
}