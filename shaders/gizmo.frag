#version 450

layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    vec4 color;
} push;

void main() {
    //cor do modelo (material do glb) x tint
    outColor = vec4(fragColor, 1.0) * push.color;
}
