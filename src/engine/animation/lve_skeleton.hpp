#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>
#include <vector>

namespace lve {

    //um osso do esqueleto
    struct Joint {
        std::string name;
        int parent = -1;  //índice do pai no vetor de ossos; -1 = raiz do esqueleto
        //pose de descanso (a do modelo sem animação), relativa ao pai
        glm::vec3 restTranslation{0.f};
        glm::quat restRotation{1.f, 0.f, 0.f, 0.f};
        glm::vec3 restScale{1.f};
        //leva um vértice do modelo pro espaço do osso na pose de descanso (vem pronto no glb)
        glm::mat4 inverseBind{1.f};
    };

    //os ossos de um modelo, sempre com o pai antes dos filhos no vetor
    struct Skeleton {
        std::vector<Joint> joints;
        //tudo que fica acima do osso raiz (o nó Armature: escala 0.01 e rotação do FBX) e o Y pra baixo
        glm::mat4 rootTransform{1.f};
    };

    //o que um canal da animação mexe num osso
    enum class AnimationPath { Translation, Rotation, Scale };

    //a curva de um osso numa propriedade: os tempos e o valor em cada um (os keyframes)
    struct AnimationChannel {
        int joint = 0;
        AnimationPath path = AnimationPath::Translation;
        std::vector<float> times;
        std::vector<glm::vec4> values;  //translação/escala usam x, y, z; rotação usa os 4 (quatérnio)
        bool step = false;               //true = pula de um valor pro outro (sem misturar)
    };

    //uma animação inteira (o "Idle", o "Walk"...)
    struct AnimationClip {
        std::string name;
        float duration = 0.f;
        std::vector<AnimationChannel> channels;
    };
}
