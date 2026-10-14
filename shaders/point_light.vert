#version 450
#extension GL_GOOGLE_include_directive : require

const vec2 OFFSETS[6] = vec2[](
    vec2(-1.0, -1.0),
    vec2(-1.0,  1.0),
    vec2( 1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2(-1.0,  1.0),
    vec2( 1.0,  1.0)
);

layout(location = 0) out vec2 fragOffset;

#include "global_ubo.glsl"

layout(push_constant) uniform Push {
    vec4 position;
    vec4 color;
    float radius;
} push;


void main() {
    fragOffset = OFFSETS[gl_VertexIndex];
    vec4 lightInCameraSpace = ubo.view * push.position;
    vec4 positionInCameraSpace = lightInCameraSpace + push.radius * vec4(fragOffset, 0.0, 0.0);
    gl_Position = ubo.projection * positionInCameraSpace;
}