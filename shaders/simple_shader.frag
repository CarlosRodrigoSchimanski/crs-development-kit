#version 450
#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragPosWorldSpace;
layout(location = 2) in vec3 fragNormalWorldSpace;
layout(location = 3) in vec2 fragUv;
layout(location = 4) in vec4 fragPosLightSpace;
layout(location = 0) out vec4 outColor;

#include "global_ubo.glsl"
#include "lighting.glsl"

//set 1 = material do objeto (troca a cada objeto desenhado)
layout(set = 1, binding = 0) uniform sampler2D texSampler;

void main() {
    //objetos sem textura recebem a branca 1x1, então isso vira só fragColor
    vec3 surfaceColor = texture(texSampler, fragUv).rgb * fragColor;
    outColor = vec4(applyLighting(surfaceColor, fragNormalWorldSpace, fragPosWorldSpace, fragPosLightSpace), 1.0);
}
