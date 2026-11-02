#version 450
#extension GL_GOOGLE_include_directive : require

//a esfera da prévia de material: posição, normal, coordenada da textura e a tangente (a direção em
//que a textura anda pra direita, em volta da esfera: o normal map precisa dela)
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec3 tangent;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragUv;
layout(location = 3) out vec3 fragTangent;

#include "material_preview_push.glsl"

void main() {
    //a esfera fica parada na origem: a matriz é só a câmera de estúdio
    gl_Position = push.viewProjection * vec4(position, 1.0);
    fragPos = position;
    fragNormal = normal;
    fragUv = uv;
    fragTangent = tangent;
}
