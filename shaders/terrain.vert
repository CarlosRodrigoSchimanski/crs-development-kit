#version 450
#extension GL_GOOGLE_include_directive : require

//um pedaço do terreno: posição já em metros a partir do canto do terreno, a normal e onde o vértice
//cai na imagem de pintura do pedaço
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 splatUv;

layout(location = 0) out vec3 fragPosWorldSpace;
layout(location = 1) out vec3 fragNormalWorldSpace;
layout(location = 2) out vec4 fragPosLightSpace;
layout(location = 3) out vec2 fragSplatUv;

#include "global_ubo.glsl"
#include "terrain_push.glsl"

void main() {
    //o terreno não gira nem escala: a matriz só leva do canto dele pro mundo
    vec4 positionWorldSpace = push.modelMatrix * vec4(position, 1.0);
    gl_Position = ubo.projection * ubo.view * positionWorldSpace;
    fragPosWorldSpace = positionWorldSpace.xyz;
    fragNormalWorldSpace = normal;
    fragPosLightSpace = ubo.lightSpaceMatrix * positionWorldSpace;
    //entre um vértice e outro o pixel anda pelos pontos de pintura do meio (4 por metro)
    fragSplatUv = splatUv;
}
