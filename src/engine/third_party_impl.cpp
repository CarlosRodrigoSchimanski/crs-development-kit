//implementação das bibliotecas "só header": cada uma precisa do seu *_IMPLEMENTATION
//definido em exatamente UM .cpp do projeto. Ficam aqui, separadas do código da engine, porque:
//  - este arquivo fica fora do /W4 (CMakeLists.txt): os avisos dessas libs não são nossos
//  - mexer no crs_model.cpp/crs_texture.cpp não recompila as dezenas de milhares de linhas delas
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
