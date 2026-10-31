#version 450
#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec3 fragPosWorldSpace;
layout(location = 1) in vec3 fragNormalWorldSpace;
layout(location = 2) in vec4 fragPosLightSpace;
layout(location = 0) out vec4 outColor;

#include "global_ubo.glsl"
#include "lighting.glsl"
#include "pbr.glsl"
#include "terrain_push.glsl"

//set 1 = material do terreno (mapas PBR); sem material: texturas neutras de 1 pixel
layout(set = 1, binding = 0) uniform sampler2D albedoMap;
layout(set = 1, binding = 1) uniform sampler2D normalMap;
layout(set = 1, binding = 2) uniform sampler2D roughnessMap;
layout(set = 1, binding = 3) uniform sampler2D aoMap;

//cor provisória (a pintura de camadas vem no 4.2): grama no plano, rocha na encosta
vec3 baseColor(vec3 normal) {
    vec3 grass = vec3(0.13, 0.22, 0.06);
    vec3 rock = vec3(0.22, 0.19, 0.15);
    float steep = smoothstep(0.55, 0.8, 1.0 - normal.y * normal.y);
    return mix(grass, rock, steep);
}

//curvas de nível (só no editor): uma linha a cada 'interval' metros de altura e uma mais forte a
//cada 5. Retorna quanto da cor da linha aparece (0 = nada)
float contourLines(float interval, vec3 normal) {
    float level = fragPosWorldSpace.y / interval;
    //quanto o nível muda de um pixel pro vizinho: deixa a linha com a mesma espessura na tela, perto ou longe
    float width = max(fwidth(level), 1e-5);
    //distância (em níveis) até a linha mais perto
    float distanceToLine = abs(fract(level - 0.5) - 0.5);
    float line = 1.0 - smoothstep(width * 0.5, width * 1.5, distanceToLine);
    float major = abs(mod(floor(level + 0.5), 5.0)) < 0.5 ? 1.0 : 0.0;
    float majorLine = (1.0 - smoothstep(width, width * 2.5, distanceToLine)) * major;
    //longe as linhas ficam apertadas demais: as finas somem, as fortes continuam
    float minorFade = 1.0 - smoothstep(0.15, 0.4, width);
    //chão plano: sem linha (uma curva bem na altura do plano pintaria ele inteiro)
    float slopeFade = smoothstep(0.0005, 0.01, 1.0 - normal.y);
    return max(line * 0.6 * minorFade, majorLine * 0.95) * slopeFade;
}

//o material visto de cima, repetido pelo chão: a coordenada da textura é a posição (x, z) em metros
vec3 materialColor(vec3 normal, out vec3 shadingNormal) {
    vec2 uv = fragPosWorldSpace.xz * push.options.y;
    vec3 albedo = texture(albedoMap, uv).rgb;
    float roughness = texture(roughnessMap, uv).r;
    float ao = texture(aoMap, uv).r;

    //normal map (padrão OpenGL): x = pra direita na foto (+X do mundo), y = pra cima na foto. A foto
    //começa no canto de cima (Vulkan), então "pra cima" nela é o -Z do mundo = cross(normal, tangente)
    vec3 tangent = normalize(vec3(1.0, 0.0, 0.0) - normal * normal.x);
    vec3 bitangent = cross(normal, tangent);
    vec3 detail = texture(normalMap, uv).xyz * 2.0 - 1.0;
    shadingNormal = normalize(tangent * detail.x + bitangent * detail.y + normal * detail.z);

    return applyPbrLighting(albedo, roughness, 0.0, ao, shadingNormal, fragPosWorldSpace, fragPosLightSpace);
}

void main() {
    vec3 normal = normalize(fragNormalWorldSpace);
    vec3 shadingNormal = normal;
    vec3 color = push.options.z > 0.5
        ? materialColor(normal, shadingNormal)
        : applyLighting(baseColor(normal), normal, fragPosWorldSpace, fragPosLightSpace);

    if (push.options.x > 0.0) {
        //azul do editor: aparece tanto em textura clara quanto escura (preto some no xadrez)
        color = mix(color, vec3(0.07, 0.39, 0.77), contourLines(push.options.x, normal));
    }

    //pincel do editor: um aro claro no raio e o miolo levemente azulado
    float radius = push.brush.w;
    if (radius > 0.0) {
        float d = distance(fragPosWorldSpace.xz, push.brush.xz);
        float pixel = max(fwidth(d), 1e-5);
        float ring = 1.0 - smoothstep(pixel * 0.5, pixel * 2.0, abs(d - radius));
        float inside = d < radius ? 0.12 * (1.0 - d / radius) + 0.05 : 0.0;
        color = mix(color, vec3(0.07, 0.39, 0.77), inside);
        color = mix(color, vec3(1.0), ring * 0.9);
    }

    outColor = vec4(color, 1.0);
}
