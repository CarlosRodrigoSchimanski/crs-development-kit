#version 450
#extension GL_GOOGLE_include_directive : require

//a colisão do modelo (malhas UCX_) no editor: só a posição de cada ponto
layout(location = 0) in vec3 position;

layout(location = 0) out vec3 fragPosWorldSpace;

#include "global_ubo.glsl"

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    vec4 color;    //rgb = cor, a = opacidade
    vec4 options;  //x = 1: face, 0: aresta (só o .frag usa)
} push;

void main() {
    vec4 world = push.modelMatrix * vec4(position, 1.0);
    gl_Position = ubo.projection * ubo.view * world;
    fragPosWorldSpace = world.xyz;
}
