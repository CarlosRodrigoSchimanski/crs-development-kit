#version 450
#extension GL_GOOGLE_include_directive : require

//um pedaço do terreno: posição já em metros a partir do canto do terreno, e a normal
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

layout(location = 0) out vec3 fragPosWorldSpace;
layout(location = 1) out vec3 fragNormalWorldSpace;
layout(location = 2) out vec4 fragPosLightSpace;

#include "global_ubo.glsl"
#include "terrain_push.glsl"

void main() {
    //o terreno não gira nem escala: a matriz só leva do canto dele pro mundo
    vec4 positionWorldSpace = push.modelMatrix * vec4(position, 1.0);
    gl_Position = ubo.projection * ubo.view * positionWorldSpace;
    fragPosWorldSpace = positionWorldSpace.xyz;
    fragNormalWorldSpace = normal;
    fragPosLightSpace = ubo.lightSpaceMatrix * positionWorldSpace;
}
