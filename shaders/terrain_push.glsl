//push constant do terreno: tem que bater com o TerrainPushConstants do terrain_render_system.cpp (128 bytes)
layout(push_constant) uniform Push {
    mat4 modelMatrix;
    vec4 brush;          //xyz = centro do pincel no mundo, w = raio (0 = sem pincel)
    vec4 options;        //x = distância entre as curvas de nível em metros (0 = sem curvas)
                         //y = quantas camadas (0 = sem material: a cor provisória)
    vec4 layerScales[2]; //1 / tamanho da textura de cada camada em metros (camada i = [i / 4][i % 4])
} push;
