#pragma once

#include "engine/rendering/lve_model.hpp"

#include <memory>
#include <unordered_map>
#include <optional>
#include <string>

#include <glm/gtc/constants.hpp>


namespace lve {
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
        float moveSpeed = 4.f; //unidade por segundo
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
        float yawOffset = glm::pi<float>();      //frente do modelo: de frente na vista Front do Blender (-Y) = -Z na engine
    };

    //toca uma animação do modelo (só modelos com esqueleto)
    struct AnimatorComponent {
        std::string clip{};       //nome da animação; vazio (ou não achou) = a primeira do modelo
        float time = 0.f;         //instante atual da animação (segundos)
        float speed = 1.f;        //1 = velocidade normal
        bool loop = true;         //recomeça ao chegar no fim
        uint32_t boneOffset = 0;  //onde as matrizes dos ossos deste objeto começam no buffer do frame (o SkinningSystem preenche)
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

        std::shared_ptr<LveModel>model{};
        std::optional<PointLightComponent> pointLight{};
        std::optional<CameraComponent> camera{};
        std::optional<RigidBodyComponent> rigidBody{};
        std::optional<CharacterComponent> character{};
        std::optional<CameraRigComponent> cameraRig{};
        std::optional<FaceMovementComponent> faceMovement{};
        std::optional<AnimatorComponent> animator{};


        private:
        LveGameObject(id_t objId);

        id_t id;
    };
}