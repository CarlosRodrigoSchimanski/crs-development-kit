#pragma once

#include "engine/scene/lve_scene.hpp"
#include "engine/core/lve_window.hpp"
#include "engine/core/lve_device.hpp"
#include "engine/core/lve_input.hpp"
#include "engine/rendering/systems/simple_render_system.hpp"
#include "engine/rendering/systems/point_light_system.hpp"
#include "engine/rendering/systems/shadow_render_system.hpp"
#include "engine/rendering/systems/gizmo_system.hpp"
#include "engine/rendering/systems/skinning_system.hpp"
#include "engine/rendering/lve_model_cache.hpp"
#include "engine/rendering/lve_descriptors.hpp"
#include "engine/rendering/lve_shadow_map.hpp"
#include "engine/rendering/lve_renderer.hpp"
#include "engine/rendering/lve_camera.hpp"
#include "engine/rendering/lve_buffer.hpp"
#include "engine/gameplay/character_controller_system.hpp"
#include "engine/gameplay/camera_rig_system.hpp"
#include "engine/animation/lve_animator_system.hpp"
#include "engine/physics/lve_physics_system.hpp"
#include "editor/lve_editor.hpp"
#include "editor/lve_imgui.hpp"

#include <memory>
#include <vector>
#include <optional>
#include <chrono>
#include <string>

namespace lve {
    //aplicação: cria a engine, monta a cena e roda o loop
    class FirstApp {
        public:
            //tamanho inicial da janela
            static constexpr int WIDTH = 1920;
            static constexpr int HEIGHT = 1080;

            FirstApp();

            //não copiável (é dona dos recursos Vulkan)
            FirstApp(const FirstApp&) = delete;
            FirstApp& operator=(const FirstApp&) = delete;

            //loop principal, até fechar a janela
            void run();

        private:
            //monta a cena inicial
            void loadGameObjects();
            //entra no Play: copia a cena do editor
            void startPlay();
            //sai do Play: descarta a cópia
            void stopPlay();
            //P entra no Play, ' sai
            void updatePlayMode();
            //Ctrl+O abre, Ctrl+S grava, Ctrl+Shift+S grava em outro arquivo
            void updateSceneFile();
            //troca a cena do editor pela do arquivo (false = deu erro e a cena ficou como estava)
            bool openSceneFile(const std::string &filepath);
            //grava a cena do editor no sceneFile e anota o resultado
            void saveSceneFile();
            //escolhe um .json na janela do sistema e abre
            void openSceneDialog();
            //escolhe onde gravar na janela do sistema; a cena passa a ser esse arquivo
            void saveSceneAsDialog();
            //escolhe um .glb na janela do sistema e põe na cena, no ponto que a câmera do editor olha
            void importModelDialog();
            //cria um objeto do menu Adicionar no ponto que a câmera do editor olha
            void addObject(AddKind kind);
            //cena que está valendo (Play ou editor)
            LveScene& activeScene();
            //janela de Stats (FPS e modo)
            void drawStatsWindow(float fps);
            //pedaço da tela onde a cena é desenhada
            VkRect2D computeSceneArea() const;
            //câmera ativa da cena do Play
            LveGameObject* findGameCamera();
            //WASD vira intenção de movimento, relativa à câmera de jogo
            CharacterInput readCharacterInput(LveGameObject* gameCamera);
            //posiciona a câmera do render (orbital ou de jogo)
            void updateCamera(LveGameObject* gameCamera, float aspect);
            //sombra apenas para o que a câmera mostra
            glm::mat4 computeLightSpaceMatrix(LveGameObject* gameCamera, glm::vec3 lightDirection, uint32_t shadowMapSize);
            //desenha o frame (sombra, cena, luzes, UI)
            void renderFrame(FrameInfo& frameInfo);
            //dados globais do frame pros shaders (câmera, sombra, luzes)
            void updateGlobalUbo(FrameInfo& frameInfo, LveGameObject* gameCamera);
            //tempo do frame e FPS
            float updateFrameTime();
            //um frame inteiro
            void drawFrame();

            //ordem importa: criados de cima pra baixo, destruídos de baixo pra cima
            LveWindow lveWindow{WIDTH, HEIGHT, "Olá Vulkan"};
            LveDevice lveDevice{lveWindow};
            LveRenderer lveRenderer{lveWindow, lveDevice};
            //pool dos descriptor sets globais
            std::unique_ptr<LveDescriptorPool> globalPool{};
            //formato dos dados globais: 0 = UBO, 1 = shadow map, 2 = luzes
            std::unique_ptr<LveDescriptorSetLayout> globalSetLayout{};
            //buffers do UBO, um por frame em voo
            std::vector<std::unique_ptr<LveBuffer>> uboBuffers;
            //mapa de profundidade do sol
            LveShadowMap shadowMap{lveDevice};

            //sistemas de render
            std::unique_ptr<SimpleRenderSystem> simpleRenderSystem{};
            std::unique_ptr<PointLightSystem> pointLightSystem{};
            std::unique_ptr<ShadowRenderSystem> shadowRenderSystem{};
            std::unique_ptr<GizmoRenderSystem> gizmoRenderSystem{};
            //matrizes dos ossos dos objetos animados (dono do buffer do binding 3)
            std::unique_ptr<SkinningSystem> skinningSystem{};

            //interface do editor
            std::unique_ptr<LveImgui> lveImgui{};
            //descriptor sets globais, um por frame em voo
            std::vector<VkDescriptorSet> globalDescriptorSets;

            //relógio do frame anterior e FPS exibido
            std::chrono::high_resolution_clock::time_point currentTime{};
            float displayFps = 0.f;

            //trava contra reentrada do drawFrame
            bool drawingFrame = false;

            //modelos carregados, um por arquivo
            LveModelCache modelCache{lveDevice};

            //cena do editor
            LveScene lveScene;
            //resultado do último salvar (mostrado na janela de stats)
            std::string sceneFileStatus{};
            //arquivo da cena aberta no editor (Salvar grava nele)
            std::string sceneFile{};
            //cópia da cena durante o Play (vazio = editor)
            std::optional<LveScene> playScene;
            //ferramentas do editor
            LveEditor editor;
            //retangulo da cena
            VkRect2D sceneArea{};
            //teclado
            LveInput lveInput;
            //câmera do render
            LveCamera camera{};
            //câmera do sol
            LveCamera shadowCamera{};
            //física do Play
            PhysicsSystem physicsSystem{};
            //personagens do Play
            CharacterControllerSystem characterController{};
            //pivos de camera
            CameraRigSystem cameraRigSystem{};
            //relógio das animações do Play
            AnimatorSystem animatorSystem{};
    };
}
