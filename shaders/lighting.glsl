//iluminação da cena (sol com sombra + ambiente + point lights), igual pra todo shader que desenha
//superfície: o simple_shader e o terreno. Precisa do global_ubo.glsl incluído antes.

//profundidade da cena vista do sol, gravada pelo passe de sombra
layout(set = 0, binding = 1) uniform sampler2D shadowMap;

//1.0 = totalmente na sombra, 0.0 = iluminado, valores no meio = borda (penumbra)
float calculateShadow(vec4 posLightSpace) {
    //de coordenadas da câmera do sol pra NDC (xy em -1..1, z em 0..1)
    vec3 projCoords = posLightSpace.xyz / posLightSpace.w;
    //além do far do sol: não tem como estar na sombra
    if (projCoords.z > 1.0) return 0.0;
    //NDC -1..1 -> coordenada de textura 0..1
    vec2 shadowUv = projCoords.xy * 0.5 + 0.5;
    //fora do que a câmera do sol enxerga (terreno grande): sem informação, fica iluminado
    if (shadowUv.x < 0.0 || shadowUv.y < 0.0 || shadowUv.x > 1.0 || shadowUv.y > 1.0) return 0.0;
    //PCF: em vez de 1 comparação, faz 9 (o texel e os 8 vizinhos) e tira a média;
    //na borda da sombra parte dos vizinhos dá "sombra" e parte "luz" -> degradê em vez de degrau
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    float shadow = 0.0;
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            //profundidade do que está mais perto do sol nessa direção
            float closestDepth = texture(shadowMap, shadowUv + vec2(x, y) * texelSize).r;
            //se este fragmento está mais longe do sol que isso, tem algo na frente
            shadow += projCoords.z > closestDepth ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

//cor final de um ponto da superfície com a cor dele (surfaceColor)
vec3 applyLighting(vec3 surfaceColor, vec3 normalWorld, vec3 posWorld, vec4 posLightSpace) {
    vec3 diffuseLight = ubo.lightAmbientColor.xyz * ubo.lightAmbientColor.w;
    vec3 specularLight = vec3(0.0);
    vec3 surfaceNormal = normalize(normalWorld);

    vec3 cameraPosWorld = ubo.inverseView[3].xyz;
    vec3 viewDir = normalize(cameraPosWorld - posWorld);

    //sol: luz direcional (sem atenuação, mesma direção em todo lugar), w = intensidade
    vec3 directionToSun = normalize(ubo.sunDirection.xyz);
    float sunCosAngIncidence = max(dot(surfaceNormal, directionToSun), 0.0);
    float shadow = calculateShadow(posLightSpace);
    //a sombra só tira a luz do sol; ambiente e point lights continuam valendo
    diffuseLight += vec3(1.0) * ubo.sunDirection.w * sunCosAngIncidence * (1.0 - shadow);

    for (int i = 0; i < ubo.numLights; i++) {
        PointLight light = lightBuffer.pointLights[i];
        vec3 directionToLight = light.position.xyz - posWorld;
        float attenuation = 1.0 / dot(directionToLight, directionToLight);
        directionToLight = normalize(directionToLight);
        float cosAngIncidence = max(dot(surfaceNormal, directionToLight), 0.0);
        vec3 intensity = light.color.xyz * light.color.w * attenuation;
        diffuseLight += intensity * cosAngIncidence;

        vec3 halfAngle = normalize(directionToLight + viewDir);
        float blinnTerm = clamp(dot(surfaceNormal, halfAngle), 0.0, 1.0);
        blinnTerm = pow(blinnTerm, 128.0);
        specularLight += intensity * blinnTerm;
    }

    return diffuseLight * surfaceColor + specularLight * surfaceColor;
}
