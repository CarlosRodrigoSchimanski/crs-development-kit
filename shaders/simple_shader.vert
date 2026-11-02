#version 450
#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 uv;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragPosWorldSpace;
layout(location = 2) out vec3 fragNormalWorldSpace;
layout(location = 3) out vec2 fragUv;
layout(location = 4) out vec4 fragPosLightSpace;

#include "global_ubo.glsl"

//instancing: cada cópia do modelo traz a própria matriz (uma mat4 ocupa 4 locations)
layout(location = 4) in mat4 instanceModelMatrix;   //4..7
layout(location = 8) in mat4 instanceNormalMatrix;  //8..11

void main() {
    vec4 positionWorldSpace = instanceModelMatrix * vec4(position, 1.0);
    gl_Position = ubo.projection * ubo.view * positionWorldSpace;
    fragPosWorldSpace = positionWorldSpace.xyz;
    fragNormalWorldSpace = normalize(mat3(instanceNormalMatrix) * normal);
    fragColor = color;
    fragUv = uv;
    //mesma posição, mas vista pela câmera do sol: é com ela que o .frag consulta o shadow map
    fragPosLightSpace = ubo.lightSpaceMatrix * positionWorldSpace;
}