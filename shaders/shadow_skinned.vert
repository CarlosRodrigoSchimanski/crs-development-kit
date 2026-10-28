#version 450 //passe de sombra dos objetos animados: o vértice é deformado pelos ossos antes
#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec3 position;
//binding 1: até 4 ossos que puxam o vértice e o peso de cada um
layout(location = 4) in uvec4 joints;
layout(location = 5) in vec4 weights;

#include "global_ubo.glsl"

//matrizes de skinning de todos os objetos animados do frame (SkinningSystem)
layout(set = 0, binding = 3) readonly buffer BoneBuffer {
    mat4 bones[];
} boneBuffer;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    uint boneOffset;
} push;

void main() {
    mat4 skin = weights.x * boneBuffer.bones[push.boneOffset + joints.x]
              + weights.y * boneBuffer.bones[push.boneOffset + joints.y]
              + weights.z * boneBuffer.bones[push.boneOffset + joints.z]
              + weights.w * boneBuffer.bones[push.boneOffset + joints.w];
    gl_Position = ubo.lightSpaceMatrix * push.modelMatrix * skin * vec4(position, 1.0);
}
