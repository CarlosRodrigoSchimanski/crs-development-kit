#pragma once

#include "engine/scene/crs_scene.hpp"
#include "engine/core/crs_window.hpp"
#include "engine/core/crs_device.hpp"
#include "engine/core/crs_input.hpp"
#include "engine/rendering/systems/simple_render_system.hpp"
#include "engine/rendering/systems/point_light_system.hpp"
#include "engine/rendering/systems/shadow_render_system.hpp"
#include "engine/rendering/systems/gizmo_system.hpp"
#include "engine/rendering/systems/skinning_system.hpp"
#include "engine/rendering/crs_model_cache.hpp"
#include "engine/rendering/crs_descriptors.hpp"
#include "engine/rendering/crs_shadow_map.hpp"
#include "engine/rendering/crs_renderer.hpp"
#include "engine/rendering/crs_camera.hpp"
#include "engine/rendering/crs_buffer.hpp"
#include "engine/gameplay/character_controller_system.hpp"
#include "engine/gameplay/camera_rig_system.hpp"
#include "engine/gameplay/character_animation_system.hpp"
#include "engine/animation/crs_animator_system.hpp"
#include "engine/animation/crs_animation_library.hpp"
#include "engine/scripting/crs_scripting.hpp"
#include "engine/physics/crs_physics_system.hpp"
#include "editor/crs_editor.hpp"
#include "editor/crs_imgui.hpp"

#include <memory>
#include <vector>
#include <optional>
#include <chrono>
#include <string>

namespace crsdk {
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
            //lê de novo as classes de componente da pasta (o que o Add Component lista)
            void refreshComponents();
            //Generate Component: cria o .py com o modelo e prende no objeto selecionado
            void generateComponent(const std::string &className);
            //erros dos componentes Python: no console e na janela de stats
            void showScriptErrors();
            //cena que está valendo (Play ou editor)
            CrsScene& activeScene();
            //janela de Stats (FPS e modo)
            void drawStatsWindow(float fps);
            //pedaço da tela onde a cena é desenhada
            VkRect2D computeSceneArea() const;
            //câmera ativa da cena do Play
            CrsGameObject* findGameCamera();
            //WASD vira intenção de movimento, relativa à câmera de jogo
            CharacterInput readCharacterInput(CrsGameObject* gameCamera);
            //posiciona a câmera do render (orbital ou de jogo)
            void updateCamera(CrsGameObject* gameCamera, float aspect);
            //sombra apenas para o que a câmera mostra
            glm::mat4 computeLightSpaceMatrix(CrsGameObject* gameCamera, glm::vec3 lightDirection, uint32_t shadowMapSize);
            //desenha o frame (sombra, cena, luzes, UI)
            void renderFrame(FrameInfo& frameInfo);
            //dados globais do frame pros shaders (câmera, sombra, luzes)
            void updateGlobalUbo(FrameInfo& frameInfo, CrsGameObject* gameCamera);
            //tempo do frame e FPS
            float updateFrameTime();
            //um frame inteiro
            void drawFrame();

            //ordem importa: criados de cima pra baixo, destruídos de baixo pra cima
            CrsWindow crsWindow{WIDTH, HEIGHT, "CRS Development Kit"};
            CrsDevice crsDevice{crsWindow};
            CrsRenderer crsRenderer{crsWindow, crsDevice};
            //animações de arquivo (o SkinningSystem usa: criada antes dele)
            CrsAnimationLibrary animationLibrary{};
            //Python embutido (scripts dos objetos)
            CrsScripting scripting{};
            //pool dos descriptor sets globais
            std::unique_ptr<CrsDescriptorPool> globalPool{};
            //formato dos dados globais: 0 = UBO, 1 = shadow map, 2 = luzes
            std::unique_ptr<CrsDescriptorSetLayout> globalSetLayout{};
            //buffers do UBO, um por frame em voo
            std::vector<std::unique_ptr<CrsBuffer>> uboBuffers;
            //mapa de profundidade do sol
            CrsShadowMap shadowMap{crsDevice};

            //sistemas de render
            std::unique_ptr<SimpleRenderSystem> simpleRenderSystem{};
            std::unique_ptr<PointLightSystem> pointLightSystem{};
            std::unique_ptr<ShadowRenderSystem> shadowRenderSystem{};
            std::unique_ptr<GizmoRenderSystem> gizmoRenderSystem{};
            //matrizes dos ossos dos objetos animados (dono do buffer do binding 3)
            std::unique_ptr<SkinningSystem> skinningSystem{};

            //interface do editor
            std::unique_ptr<CrsImgui> crsImgui{};
            //descriptor sets globais, um por frame em voo
            std::vector<VkDescriptorSet> globalDescriptorSets;

            //relógio do frame anterior e FPS exibido
            std::chrono::high_resolution_clock::time_point currentTime{};
            float displayFps = 0.f;

            //trava contra reentrada do drawFrame
            bool drawingFrame = false;

            //modelos carregados, um por arquivo
            CrsModelCache modelCache{crsDevice};

            //cena do editor
            CrsScene crsScene;
            //resultado do último salvar (mostrado na janela de stats)
            std::string sceneFileStatus{};
            //as classes de componente de assets/components (lidas ao abrir, ao parar o Play e no Add Component)
            std::vector<ComponentInfo> componentCatalog{};
            //arquivo da cena aberta no editor (Salvar grava nele)
            std::string sceneFile{};
            //cópia da cena durante o Play (vazio = editor)
            std::optional<CrsScene> playScene;
            //ferramentas do editor
            CrsEditor editor;
            //retangulo da cena
            VkRect2D sceneArea{};
            //teclado
            CrsInput crsInput;
            //câmera do render
            CrsCamera camera{};
            //câmera do sol
            CrsCamera shadowCamera{};
            //física do Play
            PhysicsSystem physicsSystem{};
            //personagens do Play
            CharacterControllerSystem characterController{};
            //pivos de camera
            CameraRigSystem cameraRigSystem{};
            //relógio das animações do Play
            AnimatorSystem animatorSystem{};
            //qual animação o personagem toca (parado, andando, correndo, no ar)
            CharacterAnimationSystem characterAnimationSystem{};
    };
}
