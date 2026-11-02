#version 450
#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUv;
layout(location = 3) in vec3 fragTangent;
layout(location = 0) out vec4 outColor;

#include "pbr_brdf.glsl"
#include "material_preview_push.glsl"

//os mapas do material (o que falta vem como textura neutra de 1 pixel)
layout(set = 0, binding = 0) uniform sampler2D albedoMap;
layout(set = 0, binding = 1) uniform sampler2D normalMap;
layout(set = 0, binding = 2) uniform sampler2D roughnessMap;
layout(set = 0, binding = 3) uniform sampler2D aoMap;

void main() {
    vec3 albedo = texture(albedoMap, fragUv).rgb;
    float roughness = clamp(texture(roughnessMap, fragUv).r, 0.04, 1.0);
    float ao = texture(aoMap, fragUv).r;

    //normal map (padrão OpenGL): x = a tangente (em volta da esfera, pra direita), y = pra cima na foto.
    //A foto começa no canto de cima (Vulkan) e o v desce da tampa de cima: "pra cima" nela aponta pro
    //polo de cima, que é cross(normal, tangente) (igual ao terreno)
    vec3 N = normalize(fragNormal);
    vec3 T = normalize(fragTangent - N * dot(N, fragTangent));
    vec3 B = cross(N, T);
    vec3 detail = texture(normalMap, fragUv).xyz * 2.0 - 1.0;
    N = normalize(T * detail.x + B * detail.y + N * detail.z);

    //estúdio: uma luz principal de cima à esquerda, uma de preenchimento mais fraca do outro lado, uma
    //de contorno por trás (destaca a borda da esfera no fundo escuro do card) e um pouco de ambiente
    vec3 V = normalize(push.cameraPosition.xyz - fragPos);
    //(a principal bem mais forte que as outras: é o contraste entre o lado aceso e o lado escuro
    //que faz a bola parecer redonda, e não um disco chapado)
    vec3 color = pbrLight(albedo, roughness, 0.0, N, V, normalize(vec3(-0.8, 0.75, 0.55)), vec3(4.2));
    color += pbrLight(albedo, roughness, 0.0, N, V, normalize(vec3(0.9, 0.1, 0.4)), vec3(0.45));
    color += pbrLight(albedo, roughness, 0.0, N, V, normalize(vec3(0.3, 0.4, -1.0)), vec3(1.6));
    color += albedo * ao * 0.06;

    outColor = vec4(color, 1.0);
}
