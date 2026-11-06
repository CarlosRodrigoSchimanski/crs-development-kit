#version 450 //passe de sombra com recorte: onde a textura é transparente, o pixel não grava profundidade

layout(location = 0) in vec2 fragUv;

//set 1 = textura de cor do modelo (o mesmo layout do simple_shader.frag)
layout(set = 1, binding = 0) uniform sampler2D texSampler;

void main() {
    //mesmo corte do simple_shader.frag: a sombra fica com o formato do que aparece na tela
    if (texture(texSampler, fragUv).a < 0.5) discard;
}
