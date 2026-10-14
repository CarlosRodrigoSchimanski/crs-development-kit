//UBO global (set 0, binding 0), incluído pelos shaders que precisam dele.
//Tem que bater campo a campo, na mesma ordem, com o struct GlobalUbo do C++ (lve_frameInfo.hpp).
//O tamanho do array pointLight tem que ser igual ao MAX_LIGHTS do C++ (até o M9 trocar por SSBO).

struct PointLight {
    vec4 position;
    vec4 color;
};

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projection;
    mat4 view;
    mat4 inverseView;
    mat4 lightSpaceMatrix;
    vec4 sunDirection;  //aponta PARA o sol; w = intensidade
    vec4 lightAmbientColor;  //w = intensidade
    PointLight pointLight[10];
    int numLights;
} ubo;
