#version 450 //passe de sombra dos modelos com recorte (grama, folhas): como o shadow.vert, mas leva o UV
#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec3 position;
layout(location = 3) in vec2 uv;

#include "global_ubo.glsl"

//instancing: a matriz de cada cópia vem da lista de cópias (a mesma do simple_shader)
layout(location = 4) in mat4 instanceModelMatrix;

layout(location = 0) out vec2 fragUv;

void main() {
    gl_Position = ubo.lightSpaceMatrix * instanceModelMatrix * vec4(position, 1.0);
    fragUv = uv;
}
