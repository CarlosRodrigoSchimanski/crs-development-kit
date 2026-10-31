//luz PBR (metallic-roughness, o modelo do glTF, do Blender e da Unreal): cada luz é dividida em
//difusa (a luz que entra no material e sai pra todo lado, com a cor dele) e especular (o reflexo
//na superfície: estreito e forte no liso, largo e fraco no áspero). Precisa do global_ubo.glsl e do
//lighting.glsl (calculateShadow) incluídos antes.

const float PI = 3.14159265359;

//quanto da superfície (micro facetas) está virada pra refletir a luz pro olho: GGX / Trowbridge-Reitz
float distributionGGX(float NdotH, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

//sombra das próprias micro facetas (luz e olho rasantes perdem um pouco do brilho): Smith + Schlick-GGX
float geometrySmith(float NdotV, float NdotL, float roughness) {
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float gv = NdotV / (NdotV * (1.0 - k) + k);
    float gl = NdotL / (NdotL * (1.0 - k) + k);
    return gv * gl;
}

//quanto reflete: pouco olhando de frente, quase tudo rasante (o chão molhado ao longe) — Fresnel de Schlick
vec3 fresnelSchlick(float cosTheta, vec3 f0) {
    return f0 + (1.0 - f0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

//uma luz chegando de 'lightDir' com 'radiance' de força: o que volta pro olho
vec3 pbrLight(vec3 albedo, float roughness, float metallic, vec3 N, vec3 V, vec3 lightDir, vec3 radiance) {
    vec3 H = normalize(V + lightDir);
    float NdotL = max(dot(N, lightDir), 0.0);
    float NdotV = max(dot(N, V), 0.0001);
    float NdotH = max(dot(N, H), 0.0);

    //não metal reflete ~4% de branco; metal reflete com a cor dele e não tem difusa
    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), f0);
    vec3 specular = distributionGGX(NdotH, roughness) * geometrySmith(NdotV, NdotL, roughness) * F
                  / (4.0 * NdotV * max(NdotL, 0.0001));
    vec3 diffuse = (vec3(1.0) - F) * (1.0 - metallic) * albedo / PI;
    return (diffuse + specular) * radiance * NdotL;
}

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
