#version 450
#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec3 position;

#include "global_ubo.glsl"

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    vec4 color;  //rgb = cor, a = opacidade
} push;

void main() {
    gl_Position = ubo.projection * ubo.view * push.modelMatrix * vec4(position, 1.0);
}
