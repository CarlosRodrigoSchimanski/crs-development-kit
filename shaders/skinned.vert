#version 450
#extension GL_GOOGLE_include_directive : require

//igual ao simple_shader.vert, mas o vértice é deformado pelos ossos antes de tudo (skinning)

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 uv;
//binding 1: até 4 ossos que puxam o vértice e o peso de cada um (somam 1)
layout(location = 4) in uvec4 joints;
layout(location = 5) in vec4 weights;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragPosWorldSpace;
layout(location = 2) out vec3 fragNormalWorldSpace;
layout(location = 3) out vec2 fragUv;
layout(location = 4) out vec4 fragPosLightSpace;

#include "global_ubo.glsl"

//matrizes de skinning de todos os objetos animados do frame (SkinningSystem)
layout(set = 0, binding = 3) readonly buffer BoneBuffer {
    mat4 bones[];
} boneBuffer;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    uint boneOffset;  //onde a fatia deste objeto começa no boneBuffer
} push;

void main() {
    //mistura das matrizes dos 4 ossos pelos pesos
    mat4 skin = weights.x * boneBuffer.bones[push.boneOffset + joints.x]
              + weights.y * boneBuffer.bones[push.boneOffset + joints.y]
              + weights.z * boneBuffer.bones[push.boneOffset + joints.z]
              + weights.w * boneBuffer.bones[push.boneOffset + joints.w];

    vec4 positionWorldSpace = push.modelMatrix * skin * vec4(position, 1.0);
    gl_Position = ubo.projection * ubo.view * positionWorldSpace;
    fragPosWorldSpace = positionWorldSpace.xyz;
    //normal deformada pelo osso e pelo objeto (inversa transposta: aguenta escala diferente por eixo)
    mat3 skinnedModel = mat3(push.modelMatrix) * mat3(skin);
    fragNormalWorldSpace = normalize(transpose(inverse(skinnedModel)) * normal);
    fragColor = color;
    fragUv = uv;
    fragPosLightSpace = ubo.lightSpaceMatrix * positionWorldSpace;
}
