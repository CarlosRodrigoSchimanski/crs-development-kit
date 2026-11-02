#version 450 //shader do passe de sombra: desenha a cena vista do sol, só profundidade
#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec3 position;

#include "global_ubo.glsl"

//instancing: a matriz de cada cópia vem da lista de cópias (a mesma do simple_shader)
layout(location = 4) in mat4 instanceModelMatrix;

void main() {
    //mesma conta do simple_shader, trocando a câmera do jogador pela câmera do sol
    gl_Position = ubo.lightSpaceMatrix * instanceModelMatrix * vec4(position, 1.0);
}
