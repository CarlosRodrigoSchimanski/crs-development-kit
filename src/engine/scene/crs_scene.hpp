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
    
    //caixa alinhada aos eixos do mundo
    struct WorldBounds {
        glm::vec3 min;
        glm::vec3 max;
        glm::vec3 center() const { return (min + max) * 0.5f; }
        glm::vec3 size() const { return max - min; }
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
            //copia o objeto e todos os filhos dele (Ctrl+D): mesmo lugar, mesmo pai, ids novos, nomes no
            //estilo do Blender ("cube" -> "cube.001"). Terreno ganha alturas próprias (senão esculpir um
            //mexeria nos dois). Devolve o id da cópia do objeto (vazio = não existe)
            std::optional<CrsGameObject::id_t> duplicateGameObject(CrsGameObject::id_t id);
            //um nome que nenhum objeto da cena usa: "nome.001", "nome.002"... (sem o número que já tinha)
            std::string uniqueName(const std::string &name) const;
            //troca o pai do objeto (vazio = vira raiz), sem ele sair do lugar: a posição, a rotação e a
            //escala dele são recalculadas pra continuar no mesmo ponto do mundo. Recusa (false): objeto
            //ou pai que não existe, o pai é ele mesmo ou um descendente dele (daria um círculo), ou o
            //objeto é um terreno (terreno não tem pai). Mesmo pai de antes: nada muda (false)
            bool setParent(CrsGameObject::id_t childId, std::optional<CrsGameObject::id_t> parentId);
            //matriz do objeto no mundo: a dos pais (do mais distante ao mais perto) x a dele
            glm::mat4 worldMatrix(const CrsGameObject &obj);
            //objeto com modelo mais perto que o raio acerta (a caixa de cada modelo); vazio = nenhum
            std::optional<CrsGameObject::id_t> pickObject(const Ray &ray);
            //raio contra as caixas de colisão (rigidBody) e os terrenos: o acerto mais perto até maxDistance; ignoreId fica de fora
            std::optional<RayCastHit> raycast(const Ray &ray, float maxDistance, std::optional<CrsGameObject::id_t> ignoreId = std::nullopt);
            //a caixa no mundo que envolve o objeto e os filhos dele (o modelo, a forma de física e o
            //terreno). Objeto sem nada disso (vazio, luz, câmera) vira um ponto, na posição dele.
            //É o que o "enquadrar" do editor (Del do teclado numérico) usa; vazio = objeto não existe
            std::optional<WorldBounds> worldBounds(CrsGameObject::id_t id);
            //altura do chão no ponto (x, z) do mundo: o primeiro terreno que cobre o ponto; vazio = nenhum
            std::optional<float> terrainHeightAt(float x, float z);
        
        private:
            CrsGameObject::Map gameObjects;
            std::optional<CrsGameObject::id_t> activeCameraId{};
            //proximo id livre desta cena
            CrsGameObject::id_t nextId = 0;
    };
}