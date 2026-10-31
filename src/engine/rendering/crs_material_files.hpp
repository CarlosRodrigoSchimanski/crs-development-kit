#pragma once

#include <string>
#include <vector>

namespace crsdk {
    //pasta onde ficam os materiais (uma subpasta por material)
    constexpr const char *MATERIAL_FOLDER = "assets/materials";

    //os mapas de um material PBR (caminhos dos arquivos; vazio = o material não tem esse mapa)
    struct MaterialFiles {
        std::string albedo;        //a cor (Diffuse / Base Color)
        std::string normal;        //o relevo fino (Normal no padrão OpenGL)
        std::string roughness;     //0 = liso, reflete como espelho; 1 = áspero, espalha a luz
        std::string ao;            //ambient occlusion: frestas e cantos mais escuros
        std::string displacement;  //altura (ainda não usado: mistura de camadas e parallax depois)
    };

    //acha os mapas pelo nome do arquivo, no padrão do Poly Haven e de quase todo site de texturas:
    //_diff/_albedo/_basecolor, _nor_gl/_normal (o _nor_dx fica de fora: o verde é invertido),
    //_rough, _ao, _disp/_height
    MaterialFiles findMaterialFiles(const std::string &folder);

    //as subpastas de 'root' que têm pelo menos a cor (os materiais que dá pra escolher), em ordem alfabética
    std::vector<std::string> listMaterials(const std::string &root = MATERIAL_FOLDER);
}
