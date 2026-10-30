#pragma once

#include "engine/rendering/crs_model.hpp"

#include <memory>
#include <unordered_map>
#include <optional>
#include <string>
#include <vector>

#include <glm/gtc/constants.hpp>


namespace crsdk {
    struct CameraComponent{
        float fovY = glm::radians(50.f);
        float nearPlane = 0.1f;
        float farPlane = 100.f;
    };

    enum class BodyType {Static, Dynamic};
    struct RigidBodyComponent{
        BodyType type = BodyType::Dynamic;
        glm::vec3 velocity{0.f};
        glm::vec3 halfExtents{1.f};
        //centro da caixa em relação à origem, antes da escala (personagem com origem nos pés: acima dela)
        glm::vec3 center{0.f};
        bool onGround = false;
    };

    //personagem controlavel: anda pela intenção de movimento
    struct CharacterComponent
    {
        float moveSpeed = 1.6f; //andando (unidade por segundo)
        float runSpeed = 5.f;   //correndo (Shift)
        float jumpSpeed = 5.f; //velocidade inicial do pulo
    };

    struct CameraRigComponent {
        float sensitivity = 0.003f;
        float minPitch = glm::radians(-70.f);
        float maxPitch = glm::radians(40.f);
        float distance = 4.f;     //quanto a câmera fica atrás do pivô (o máximo: uma parede no meio aproxima)
        float wallMargin = 0.2f;  //folga entre a câmera e o que o raio acertar
    };

    //vira este objeto (em Y) pra direção em que o pai se move: o visual do personagem
    struct FaceMovementComponent {
        float turnSpeed = glm::radians(1200.f);  //velocidade de giro (radianos por segundo)
        float yawOffset = 0.f;  //frente do modelo: de frente na vista Front do Blender (-Y) = +Z na engine
    };

    //toca uma animação do modelo (só modelos com esqueleto)
    struct AnimatorComponent {
        std::string clip{};       //animação tocando: o nome de um arquivo da lista ou uma do modelo; vazio = a primeira
        std::vector<std::string> animations{};  //arquivos de animação (assets/animations/...): o nome é o do arquivo
        float time = 0.f;         //instante atual da animação (segundos)
        float speed = 1.f;        //1 = velocidade normal
        bool loop = true;         //recomeça ao chegar no fim
        uint32_t boneOffset = 0;  //onde as matrizes dos ossos deste objeto começam no buffer do frame (o SkinningSystem preenche)
        float duration = 0.f;     //duração da animação atual em segundos (o SkinningSystem preenche; 0 = ainda não achou)

        //transição: a animação anterior continua tocando e some aos poucos (estado do momento: não é salvo)
        std::string previousClip{};
        float previousTime = 0.f;
        bool previousLoop = true;
        float fadeDuration = 0.f;  //0 = sem transição
        float fadeElapsed = 0.f;

        //troca de animação misturando com a atual por fadeSeconds; a mesma que já toca não recomeça
        //(é a função que os scripts Python vão chamar)
        void play(const std::string &name, float fadeSeconds = 0.2f, bool loopClip = true);
        //está no meio de uma transição
        bool isBlending() const;
        //quanto vale a animação nova na mistura: 0 = só a anterior, 1 = só a nova
        float blendWeight() const;
        //o instante dentro da animação atual, em segundos (no loop, volta pro começo a cada volta)
        float clipTime() const;
        //quanto da animação já tocou, de 0 a 1 (bom pra janela de golpe: não depende da velocidade)
        float progress() const;
        //a animação sem loop chegou ao fim
        bool isFinished() const;
        //o quadro atual, contando fps quadros por segundo (o Mixamo usa 30)
        int frame(float fps = 30.f) const;
    };

    //qual animação o visual do personagem toca em cada situação (vazio = a primeira da lista)
    //provisório: quando tiver scripts, essa escolha vira Python chamando animator.play()
    struct CharacterAnimationComponent {
        std::string idle{};
        std::string walk{};
        std::string run{};
        std::string fall{};      //no ar (pulando ou caindo): em loop até pousar
        float fadeTime = 0.2f;   //duração da transição (segundos)
    };

    struct TransformComponent {
        glm::vec3 translation{};
        glm::vec3 scale{1.f,1.f,1.f};
        glm::vec3 rotation{};

        glm::mat3 rotationMatrix() const;
        glm::mat4 mat4() const;
        glm::mat3 normalMatrix() const;

        //preenche posição, rotação e escala a partir de uma matriz (o caminho inverso do mat4)
        void setFromMatrix(const glm::mat4 &matrix);
    };

    struct PointLightComponent {
        float lightIntensity = 1.f;
    };


    class CrsGameObject{

        public:
            using id_t = unsigned int;
            using Map = std::unordered_map<id_t, CrsGameObject>;

            CrsGameObject(const CrsGameObject &) = default;
            CrsGameObject &operator=(const CrsGameObject &) = default;
            CrsGameObject(CrsGameObject &&) = default;
            CrsGameObject &operator=(CrsGameObject &&) = default;

            id_t getId() const;
            //tem pesos de osso e está tocando animação: desenhado pelas pipelines de skinning
            bool isAnimated() const;

            //nome mostrado no editor (como no outliner do Blender)
            std::string name{};

            glm::vec3 color{};
            //false = usa a textura branca em vez da do modelo (fica só a cor do vértice)
            bool useTexture{true};
            bool editorOnly{false};
            TransformComponent transform{};
            //pai na cena (o transform passa a ser relativo a ele); vazio = raiz
            std::optional<id_t> parentId{};

            std::shared_ptr<CrsModel>model{};
            std::optional<PointLightComponent> pointLight{};
            std::optional<CameraComponent> camera{};
            std::optional<RigidBodyComponent> rigidBody{};
            std::optional<CharacterComponent> character{};
            std::optional<CameraRigComponent> cameraRig{};
            std::optional<FaceMovementComponent> faceMovement{};
            std::optional<AnimatorComponent> animator{};
        std::optional<CharacterAnimationComponent> characterAnimation{};


        private:
            friend class CrsScene;
            CrsGameObject(id_t objId);
            id_t id;
    };
}