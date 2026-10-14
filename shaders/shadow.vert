#version 450 //shader do passe de sombra: desenha a cena vista do sol, só profundidade
#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec3 position;

#include "global_ubo.glsl"

layout(push_constant) uniform Push {
    mat4 modelMatrix;
} push;

void main() {
    //mesma conta do simple_shader, trocando a câmera do jogador pela câmera do sol
    gl_Position = ubo.lightSpaceMatrix * push.modelMatrix * vec4(position, 1.0);
}
