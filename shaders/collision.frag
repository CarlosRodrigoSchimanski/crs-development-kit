#version 450

layout(location = 0) in vec3 fragPosWorldSpace;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    vec4 color;    //rgb = cor, a = opacidade
    vec4 options;  //x = 1: face (com luz); 0: aresta (a cor direto: linha não tem inclinação de face)
} push;

void main() {
    //a normal da face pela inclinação entre um pixel e o vizinho (a malha de colisão não tem normais):
    //cada face sai lisa e com o próprio tom, e dá pra ver o volume mesmo translúcido
    float light = 1.0;
    if (push.options.x > 0.5) {
        vec3 normal = normalize(cross(dFdx(fragPosWorldSpace), dFdy(fragPosWorldSpace)));
        //luz fixa de cima e de lado; vista por dentro (a face de trás) também acende, por isso o abs
        light = 0.55 + 0.45 * abs(dot(normal, normalize(vec3(0.4, 1.0, 0.25))));
    }
    outColor = vec4(push.color.rgb * light, push.color.a);
}
