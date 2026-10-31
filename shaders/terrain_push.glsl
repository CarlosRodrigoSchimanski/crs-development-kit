//push constant do terreno: tem que bater com o TerrainPushConstants do terrain_render_system.cpp
layout(push_constant) uniform Push {
    mat4 modelMatrix;
    vec4 brush;     //xyz = centro do pincel no mundo, w = raio (0 = sem pincel)
    vec4 options;   //x = distância entre as curvas de nível em metros (0 = sem curvas)
                    //y = 1 / tamanho da textura em metros; z = 1 com material, 0 = cor provisória
} push;
