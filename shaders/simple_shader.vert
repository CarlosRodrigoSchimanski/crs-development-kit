#version 450 //versão do shader

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 uv;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragPosWorldSpace;
layout(location = 2) out vec3 fragNormalWorldSpace;
layout(location = 3) out vec2 fragUv;
layout(location = 4) out vec4 fragPosLightSpace;

struct PointLight {
    vec4 position;
    vec4 color;
};

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projection;
    mat4 view;
    mat4 inverseView;
    mat4 lightSpaceMatrix;
    vec4 sunDirection;
    vec4 lightAmbientColor;
    PointLight pointLight[10];
    int numLights;
} ubo;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    mat4 normalMatrix;
} push;

void main() {//função principal do shader
    vec4 positionWorldSpace = push.modelMatrix * vec4(position, 1.0);
    gl_Position = ubo.projection * ubo.view * positionWorldSpace;
    fragPosWorldSpace = positionWorldSpace.xyz;
    fragNormalWorldSpace = normalize(mat3(push.normalMatrix) * normal);
    fragColor = color;
    fragUv = uv;
    //mesma posição, mas vista pela câmera do sol: é com ela que o .frag consulta o shadow map
    fragPosLightSpace = ubo.lightSpaceMatrix * positionWorldSpace;
}