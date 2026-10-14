//dados globais do frame (set 0), incluídos pelos shaders que precisam deles.
//Têm que bater campo a campo, na mesma ordem, com os structs GlobalUbo e PointLight do C++
//(lve_frameInfo.hpp).

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
    int numLights;  //quantas posições do lightBuffer valem neste frame
} ubo;

//point lights num storage buffer (binding 2): diferente do uniform buffer, o array pode ficar sem
//tamanho no shader, então a quantidade de luzes não tem um teto escrito aqui
layout(set = 0, binding = 2) readonly buffer PointLightBuffer {
    PointLight pointLights[];
} lightBuffer;
