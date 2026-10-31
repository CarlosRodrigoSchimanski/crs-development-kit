#pragma once

#include "engine/scene/crs_scene.hpp"

#include <memory>

namespace crsdk {
    //física da cena, feita pela Jolt Physics (escondida aqui dentro: nenhum header da engine inclui ela).
    //A cena continua sendo quem manda: a cada update o sistema confere os RigidBodyComponent e
    //  - cria/refaz/remove os corpos da Jolt (forma, tamanho ou escala mudou = corpo refeito);
    //  - objeto que alguém moveu (editor, script) é levado pro lugar novo;
    //  - roda a simulação e devolve posição, rotação, velocidade e onGround pros componentes.
    //Objeto com CharacterComponent vira um personagem da Jolt (CharacterVirtual): desliza na parede,
    //sobe degrau e rampa, para em ladeira íngreme e empurra os corpos dinâmicos.
    //Os corpos usam o transform do objeto como posição no mundo: são objetos sem pai.
    class PhysicsSystem {
        public:
            PhysicsSystem();
            ~PhysicsSystem();

            PhysicsSystem(const PhysicsSystem &) = delete;
            PhysicsSystem &operator=(const PhysicsSystem &) = delete;

            void update(CrsScene &scene, float dt);

            //esquece todos os corpos (outra cena: no começo de cada Play)
            void clear();

            glm::vec3 gravity{0.f, -9.8f, 0.f};

        private:
            struct World;
            std::unique_ptr<World> world;
    };
}
