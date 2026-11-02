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

//projeção TRIPLANAR: a textura vem de três projetores, um de cima (plano XZ) e dois dos lados (planos
//ZY e XY), misturados pela inclinação. Só de cima, a textura estica na encosta (a mesma faixa da foto
//cobre uma parede inteira); com os três, cada lado do morro usa o projetor que está de frente pra ele
const float TRIPLANAR_SHARPNESS = 4.0;  //maior = troca de projetor mais seca (menos área misturada)
const float TRIPLANAR_SKIP = 0.02;      //projetor com menos peso que isso nem é lido (no plano: só o de cima)

//os pesos dos três projetores (x = o de lado X, y = o de cima, z = o de lado Z), somando 1
vec3 triplanarWeights(vec3 normal) {
    vec3 w = pow(abs(normal), vec3(TRIPLANAR_SHARPNESS));
    w /= (w.x + w.y + w.z);
    w = mix(vec3(0.0), w, step(TRIPLANAR_SKIP, w));
    return w / (w.x + w.y + w.z);
}

//as coordenadas da foto em cada projetor. Do lado de trás (normal negativa) a foto é espelhada pra não
//ficar ao contrário; nos lados o v desce com a altura (a foto em pé, como numa parede)
struct Triplanar {
    vec3 weights;
    vec3 axisSign;
    vec2 uvX, uvY, uvZ;  //em metros (o tamanho da camada multiplica depois)
    vec2 dxX, dxY, dxZ;  //o quanto mudam de um pixel pro vizinho (mip escolhido fora do "if" de cada camada)
    vec2 dyX, dyY, dyZ;
};

Triplanar makeTriplanar(vec3 pos, vec3 normal) {
    Triplanar tp;
    tp.weights = triplanarWeights(normal);
    tp.axisSign = vec3(normal.x < 0.0 ? -1.0 : 1.0, normal.y < 0.0 ? -1.0 : 1.0, normal.z < 0.0 ? -1.0 : 1.0);
    vec3 dx = dFdx(pos);
    vec3 dy = dFdy(pos);
    vec2 flipX = vec2(tp.axisSign.x, -1.0);
    vec2 flipZ = vec2(-tp.axisSign.z, -1.0);
    tp.uvX = pos.zy * flipX;  tp.dxX = dx.zy * flipX;  tp.dyX = dy.zy * flipX;
    tp.uvY = pos.xz;          tp.dxY = dx.xz;          tp.dyY = dy.xz;
    tp.uvZ = pos.xy * flipZ;  tp.dxZ = dx.xy * flipZ;  tp.dyZ = dy.xy * flipZ;
    return tp;
}

//um mapa de cor/dado de uma camada, misturado dos projetores que têm peso
vec4 sampleTriplanar(sampler2D map, Triplanar tp, float scale) {
    vec4 result = vec4(0.0);
    if (tp.weights.x > 0.0) result += textureGrad(map, tp.uvX * scale, tp.dxX * scale, tp.dyX * scale) * tp.weights.x;
    if (tp.weights.y > 0.0) result += textureGrad(map, tp.uvY * scale, tp.dxY * scale, tp.dyY * scale) * tp.weights.y;
    if (tp.weights.z > 0.0) result += textureGrad(map, tp.uvZ * scale, tp.dxZ * scale, tp.dyZ * scale) * tp.weights.z;
    return result;
}

//o normal map de uma camada nos três projetores, já virado pro mundo e misturado ("whiteout": soma o
//relevo da foto à inclinação do terreno em cada projetor). O normal map é OpenGL (verde = pra cima na
//foto) e a foto começa no canto de cima (Vulkan): o verde vem invertido em relação ao v
vec3 sampleTriplanarNormal(sampler2D map, Triplanar tp, float scale, vec3 normal) {
    vec3 result = vec3(0.0);
    if (tp.weights.x > 0.0) {
        vec3 tn = textureGrad(map, tp.uvX * scale, tp.dxX * scale, tp.dyX * scale).xyz * 2.0 - 1.0;
        tn.x *= tp.axisSign.x;  //a foto espelhada no lado de trás: o relevo de lado espelha junto
        tn = vec3(tn.xy + normal.zy, abs(tn.z) * normal.x);
        result += tn.zyx * tp.weights.x;
    }
    if (tp.weights.y > 0.0) {
        vec3 tn = textureGrad(map, tp.uvY * scale, tp.dxY * scale, tp.dyY * scale).xyz * 2.0 - 1.0;
        tn.y = -tn.y;  //de cima: "pra cima na foto" é o -Z do mundo
        tn = vec3(tn.xy + normal.xz, abs(tn.z) * normal.y);
        result += tn.xzy * tp.weights.y;
    }
    if (tp.weights.z > 0.0) {
        vec3 tn = textureGrad(map, tp.uvZ * scale, tp.dxZ * scale, tp.dyZ * scale).xyz * 2.0 - 1.0;
        tn.x *= -tp.axisSign.z;
        tn = vec3(tn.xy + normal.xy, abs(tn.z) * normal.z);
        result += tn.xyz * tp.weights.z;
    }
    return normalize(result);
}

//as camadas, cada uma repetida pelo chão no tamanho dela (pelos três projetores), misturadas pela
//pintura e pela altura
vec3 layeredColor(vec3 normal, int layerCount) {
    float painted[MAX_LAYERS] = float[](fragWeights0.x, fragWeights0.y, fragWeights0.z, fragWeights0.w,
                                        fragWeights1.x, fragWeights1.y, fragWeights1.z, fragWeights1.w);
    //coordenadas e derivadas calculadas aqui fora: dentro do "if" de cada camada a GPU não consegue
    //escolher o mip sozinha (cada pixel pode pular camadas diferentes)
    Triplanar tp = makeTriplanar(fragPosWorldSpace, normal);

    //1) a "altura" de cada camada presente: pintado + o relevo do material (pedra alta aparece primeiro)
    float score[MAX_LAYERS];
    float best = -1.0;
    for (int i = 0; i < MAX_LAYERS; i++) {
        score[i] = -1.0;
        if (i >= layerCount || painted[i] < 0.002) continue;
        float scale = push.layerScales[i / 4][i % 4];
        float h = sampleTriplanar(heightMaps[i], tp, scale).r;
        score[i] = painted[i] + h * HEIGHT_INFLUENCE;
        best = max(best, score[i]);
    }
    //nada pintado (vértice sem peso): o chão base
    if (best < 0.0) { score[0] = 1.0; best = 1.0; }

    //2) só as camadas perto da mais alta entram, cada uma com o quanto passou da linha
    vec3 albedo = vec3(0.0);
    vec3 shadingNormal = vec3(0.0);
    float roughness = 0.0;
    float ao = 0.0;
    float total = 0.0;
    for (int i = 0; i < MAX_LAYERS; i++) {
        float blend = max(score[i] - (best - BLEND_DEPTH), 0.0);
        if (blend <= 0.0) continue;
        float scale = push.layerScales[i / 4][i % 4];
        albedo += sampleTriplanar(albedoMaps[i], tp, scale).rgb * blend;
        shadingNormal += sampleTriplanarNormal(normalMaps[i], tp, scale, normal) * blend;
        roughness += sampleTriplanar(roughnessMaps[i], tp, scale).r * blend;
        ao += sampleTriplanar(aoMaps[i], tp, scale).r * blend;
        total += blend;
    }
    albedo /= total;
    roughness /= total;
    ao /= total;
    shadingNormal = normalize(shadingNormal);

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

    //aura de alerta (só no editor): o trecho tem mais de 4 materiais. Tom laranja-avermelhado no
    //trecho inteiro e uma borda firme nos limites dele (32 m), pra saber exatamente qual corrigir
    if (push.options.z > 0.5) {
        vec2 local = fragPosWorldSpace.xz - push.modelMatrix[3].xz;  //metros a partir do canto do terreno
        vec2 inChunk = fract(local / 32.0);
        float edgeMeters = min(min(inChunk.x, 1.0 - inChunk.x), min(inChunk.y, 1.0 - inChunk.y)) * 32.0;
        float pixel = max(fwidth(edgeMeters), 1e-5);
        float border = 1.0 - smoothstep(pixel * 1.5, pixel * 3.0, edgeMeters);
        vec3 alert = vec3(1.0, 0.18, 0.03);
        color = mix(color, alert, 0.28);
        color = mix(color, alert, border * 0.9);
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
