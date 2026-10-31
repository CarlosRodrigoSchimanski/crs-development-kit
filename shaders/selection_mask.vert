#version 450
#extension GL_GOOGLE_include_directive : require

//máscara da seleção: só a forma do objeto selecionado, pra aura do editor saber onde ele está na tela

layout(location = 0) in vec3 position;

#include "global_ubo.glsl"

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    uint boneOffset;  //não usado aqui (o mesmo push do selection_mask_skinned.vert)
} push;

void main() {
    gl_Position = ubo.projection * ubo.view * push.modelMatrix * vec4(position, 1.0);
}
