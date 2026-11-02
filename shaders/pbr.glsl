//luz PBR (metallic-roughness, o modelo do glTF, do Blender e da Unreal): cada luz é dividida em
//difusa (a luz que entra no material e sai pra todo lado, com a cor dele) e especular (o reflexo
//na superfície: estreito e forte no liso, largo e fraco no áspero). Precisa do global_ubo.glsl e do
//lighting.glsl (calculateShadow) incluídos antes.

#include "pbr_brdf.glsl"

//sol (com sombra) + point lights + ambiente. As intensidades da engine foram pensadas pra luz simples
//(cor * cosseno); o PI deixa o difuso do PBR com o mesmo brilho de antes
vec3 applyPbrLighting(vec3 albedo, float roughness, float metallic, float ao, vec3 N, vec3 posWorld, vec4 posLightSpace) {
    roughness = clamp(roughness, 0.04, 1.0);  //roughness 0 = brilho infinitamente pequeno (some)
    vec3 V = normalize(ubo.inverseView[3].xyz - posWorld);

    float shadow = calculateShadow(posLightSpace);
    vec3 sunRadiance = vec3(ubo.sunDirection.w * PI) * (1.0 - shadow);
    vec3 color = pbrLight(albedo, roughness, metallic, N, V, normalize(ubo.sunDirection.xyz), sunRadiance);

    for (int i = 0; i < ubo.numLights; i++) {
        PointLight light = lightBuffer.pointLights[i];
        vec3 toLight = light.position.xyz - posWorld;
        float attenuation = 1.0 / dot(toLight, toLight);
        vec3 radiance = light.color.xyz * light.color.w * attenuation * PI;
        color += pbrLight(albedo, roughness, metallic, N, V, normalize(toLight), radiance);
    }

    //ambiente: a luz que vem de todo lado, apagada nas frestas pelo AO (sem mapa do ambiente ainda:
    //o reflexo do céu de verdade vem com uma imagem HDRI)
    vec3 ambient = ubo.lightAmbientColor.xyz * ubo.lightAmbientColor.w * albedo * ao * (1.0 - metallic);
    return color + ambient;
}
