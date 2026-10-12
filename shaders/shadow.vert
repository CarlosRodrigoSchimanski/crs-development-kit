#version 450 //shader do passe de sombra: desenha a cena vista do sol, só profundidade

layout(location = 0) in vec3 position;

//só o começo do GlobalUbo: como os campos estão na mesma ordem, os offsets batem
//e o resto do bloco (luzes etc.) pode ficar de fora
layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projection;
    mat4 view;
    mat4 inverseView;
    mat4 lightSpaceMatrix;
} ubo;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
} push;

void main() {
    //mesma conta do simple_shader, trocando a câmera do jogador pela câmera do sol
    gl_Position = ubo.lightSpaceMatrix * push.modelMatrix * vec4(position, 1.0);
}
