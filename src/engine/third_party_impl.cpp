//implementação das bibliotecas "só header": cada uma precisa do seu *_IMPLEMENTATION
//definido em exatamente UM .cpp do projeto. Ficam aqui, separadas do código da engine, porque:
//  - este arquivo fica fora do /W4 (CMakeLists.txt): os avisos dessas libs não são nossos
//  - mexer no lve_model.cpp/lve_texture.cpp não recompila as ~17 mil linhas delas
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
