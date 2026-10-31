#version 450
#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec3 fragPosWorldSpace;
layout(location = 1) in vec3 fragNormalWorldSpace;
layout(location = 2) in vec4 fragPosLightSpace;
layout(location = 3) in vec4 fragWeights0;
layout(location = 4) in vec4 fragWeights1;
layout(location = 0) out vec4 outColor;

#include "global_ubo.glsl"
#include "lighting.glsl"
#include "pbr.glsl"
#include "terrain_push.glsl"

//set 1 = os mapas das 8 camadas, um array por tipo de mapa (posição sem camada: texturas neutras de 1 pixel)
const int MAX_LAYERS = 8;
layout(set = 1, binding = 0) uniform sampler2D albedoMaps[MAX_LAYERS];
layout(set = 1, binding = 1) uniform sampler2D normalMaps[MAX_LAYERS];
layout(set = 1, binding = 2) uniform sampler2D roughnessMaps[MAX_LAYERS];
layout(set = 1, binding = 3) uniform sampler2D aoMaps[MAX_LAYERS];
layout(set = 1, binding = 4) uniform sampler2D heightMaps[MAX_LAYERS];

//mistura pela altura: quanto a altura de cada material (displacement) conta contra o quanto foi pintado,
//e a faixa em que duas camadas se misturam (menor = borda mais seca, como areia entrando nas frestas)
const float HEIGHT_INFLUENCE = 0.6;
const float BLEND_DEPTH = 0.15;

//cor provisória (sem material nenhum): grama no plano, rocha na encosta
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

//as camadas vistas de cima, cada uma repetida pelo chão no tamanho dela, misturadas pela pintura e pela altura
vec3 layeredColor(vec3 normal, int layerCount) {
    float painted[MAX_LAYERS] = float[](fragWeights0.x, fragWeights0.y, fragWeights0.z, fragWeights0.w,
                                        fragWeights1.x, fragWeights1.y, fragWeights1.z, fragWeights1.w);
    //o quanto a coordenada muda de um pixel pro vizinho, calculado aqui fora: dentro do "if" de cada
    //camada a GPU não consegue escolher o mip sozinha (cada pixel pode pular camadas diferentes)
    vec2 uvDx = dFdx(fragPosWorldSpace.xz);
    vec2 uvDy = dFdy(fragPosWorldSpace.xz);

    //1) a "altura" de cada camada presente: pintado + o relevo do material (pedra alta aparece primeiro)
    float score[MAX_LAYERS];
    float best = -1.0;
    for (int i = 0; i < MAX_LAYERS; i++) {
        score[i] = -1.0;
        if (i >= layerCount || painted[i] < 0.002) continue;
        float scale = push.layerScales[i / 4][i % 4];
        float h = textureGrad(heightMaps[i], fragPosWorldSpace.xz * scale, uvDx * scale, uvDy * scale).r;
        score[i] = painted[i] + h * HEIGHT_INFLUENCE;
        best = max(best, score[i]);
    }
    //nada pintado (vértice sem peso): o chão base
    if (best < 0.0) { score[0] = 1.0; best = 1.0; }

    //2) só as camadas perto da mais alta entram, cada uma com o quanto passou da linha
    vec3 tangent = normalize(vec3(1.0, 0.0, 0.0) - normal * normal.x);
    vec3 bitangent = cross(normal, tangent);
    vec3 albedo = vec3(0.0);
    vec3 detail = vec3(0.0);
    float roughness = 0.0;
    float ao = 0.0;
    float total = 0.0;
    for (int i = 0; i < MAX_LAYERS; i++) {
        float blend = max(score[i] - (best - BLEND_DEPTH), 0.0);
        if (blend <= 0.0) continue;
        float scale = push.layerScales[i / 4][i % 4];
        vec2 uv = fragPosWorldSpace.xz * scale;
        vec2 dx = uvDx * scale;
        vec2 dy = uvDy * scale;
        albedo += textureGrad(albedoMaps[i], uv, dx, dy).rgb * blend;
        detail += (textureGrad(normalMaps[i], uv, dx, dy).xyz * 2.0 - 1.0) * blend;
        roughness += textureGrad(roughnessMaps[i], uv, dx, dy).r * blend;
        ao += textureGrad(aoMaps[i], uv, dx, dy).r * blend;
        total += blend;
    }
    albedo /= total;
    roughness /= total;
    ao /= total;

    //normal map (padrão OpenGL): x = pra direita na foto (+X do mundo), y = pra cima na foto. A foto
    //começa no canto de cima (Vulkan), então "pra cima" nela é o -Z do mundo = cross(normal, tangente)
    detail = normalize(detail);
    vec3 shadingNormal = normalize(tangent * detail.x + bitangent * detail.y + normal * detail.z);

    return applyPbrLighting(albedo, roughness, 0.0, ao, shadingNormal, fragPosWorldSpace, fragPosLightSpace);
}

void main() {
    vec3 normal = normalize(fragNormalWorldSpace);
    int layerCount = int(push.options.y + 0.5);
    vec3 color = layerCount > 0
        ? layeredColor(normal, layerCount)
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
