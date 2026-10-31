#version 450

//aura do selecionado, como no Blender: uma linha laranja colada na borda do objeto e um brilho
//fraco que some com a distância. Cada pixel FORA da máscara procura o pixel do objeto mais perto

layout(set = 0, binding = 0) uniform sampler2D selectionMask;

layout(push_constant) uniform Push {
    vec4 color;     //cor da aura (já em linear); a = força
    float radius;   //até onde o brilho vai, em pixels
} push;

layout(location = 0) out vec4 outColor;

void main() {
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    //dentro do objeto: nada (a aura fica só em volta)
    if (texelFetch(selectionMask, pixel, 0).r > 0.5) discard;

    ivec2 size = textureSize(selectionMask, 0);
    int reach = int(ceil(push.radius));
    float nearest = push.radius + 1.0;
    for (int y = -reach; y <= reach; y++) {
        for (int x = -reach; x <= reach; x++) {
            ivec2 neighbor = clamp(pixel + ivec2(x, y), ivec2(0), size - 1);
            if (texelFetch(selectionMask, neighbor, 0).r > 0.5) {
                nearest = min(nearest, length(vec2(x, y)));
            }
        }
    }
    if (nearest > push.radius) discard;

    //linha firme de ~2 pixels + brilho que cai devagar até o raio
    float line = 1.0 - smoothstep(1.5, 2.5, nearest);
    float glow = 1.0 - nearest / push.radius;
    float strength = max(line, glow * glow * 0.45);
    outColor = vec4(push.color.rgb, push.color.a * strength);
}
