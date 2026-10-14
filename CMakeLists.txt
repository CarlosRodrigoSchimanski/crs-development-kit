#versão do cmake
cmake_minimum_required(VERSION 3.20)

#nome e linguagem do projeto
project(G_E_Youtube LANGUAGES CXX)

#versão do c++ a ser utilizada
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
#----------------------------------------------------------#

#incluir glfw
include(FetchContent)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)
#----------------------------------------------------------#

#incluir glm (biblioteca de matemática, só de headers)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)
#----------------------------------------------------------#

#incluir cgltf (carregamento de modelos glTF .gltf/.glb, formato final do jogo)
#é só um header (como o stb): não tem alvo pra linkar, só a pasta de include
FetchContent_Declare(
    cgltf
    GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
    GIT_TAG v1.14
)
FetchContent_MakeAvailable(cgltf)
#----------------------------------------------------------#

#incluir VMA (Vulkan Memory Allocator, da AMD): em vez de um vkAllocateMemory por buffer/imagem,
#pede poucos blocos grandes à GPU e divide eles por dentro (como um malloc faz com a RAM).
#Só header, como o stb: a implementação fica no third_party_impl.cpp
FetchContent_Declare(
    vma
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(vma)
#----------------------------------------------------------#

#incluir stb_image (carregamento de arquivos de imagem pra textura)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    #o stb não publica versões: fixado num commit (2026-08-01) pra o build não mudar sozinho
    GIT_TAG 2c980bb59875b0d32144a71867fbdebb2f77cd20
)
FetchContent_MakeAvailable(stb)

#----------------------------------------------------------#

#incluir Dear ImGui (biblioteca de UI imediata, pra ferramentas de editor)
#o ImGui não tem CMakeLists.txt próprio no repositório principal: ele é só um
#conjunto de .cpp/.h pra compilar direto no seu executável, por isso listamos
#os arquivos fonte na mão em vez de linkar um alvo pronto do FetchContent
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.0
)
FetchContent_MakeAvailable(imgui)
set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
#----------------------------------------------------------#

#compilar os shaders (.vert/.frag) para .spv usando glslc do Vulkan SDK
find_program(GLSLC glslc REQUIRED)
#lista de arquivos de shader
set(SHADER_SOURCES
    ${CMAKE_SOURCE_DIR}/shaders/simple_shader.vert
    ${CMAKE_SOURCE_DIR}/shaders/simple_shader.frag
    ${CMAKE_SOURCE_DIR}/shaders/point_light.vert
    ${CMAKE_SOURCE_DIR}/shaders/point_light.frag
    ${CMAKE_SOURCE_DIR}/shaders/shadow.vert
    ${CMAKE_SOURCE_DIR}/shaders/shadow.frag
)
#loop para compilar cada shader
foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    #define o caminho de saída do arquivo e adiciona .spv
    set(SPIRV ${CMAKE_SOURCE_DIR}/shaders/${SHADER_NAME}.spv)
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${GLSLC} ${SHADER} -o ${SPIRV}
        #os shaders incluem esse arquivo: mudar ele tem que recompilar todos
        DEPENDS ${SHADER} ${CMAKE_SOURCE_DIR}/shaders/global_ubo.glsl
    )
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach()
#----------------------------------------------------------#

#finalização: monta o executável e conecta tudo
#incluir vulkan
find_package(Vulkan REQUIRED)

#arquivos do projeto (separados das bibliotecas: os avisos extras valem só pra eles)
set(PROJECT_SOURCES
    src/main.cpp
    src/engine/core/lve_window.cpp
    src/engine/core/lve_device.cpp
    src/app/first_app.cpp
    src/engine/rendering/lve_pipeline.cpp
    src/engine/rendering/lve_swap_chain.cpp
    src/engine/rendering/lve_model.cpp
    src/engine/rendering/lve_renderer.cpp
    src/engine/scene/lve_game_object.cpp
    src/engine/rendering/systems/simple_render_system.cpp
    src/engine/rendering/systems/point_light_system.cpp
    src/engine/rendering/systems/shadow_render_system.cpp
    src/engine/rendering/lve_camera.cpp
    src/engine/rendering/lve_buffer.cpp
    src/engine/rendering/lve_descriptors.cpp
    src/editor/orbit_camera_controller.cpp
    src/editor/lve_imgui.cpp
    src/engine/rendering/lve_texture.cpp
    src/engine/rendering/lve_shadow_map.cpp
    src/engine/rendering/lve_image_barrier.cpp
)

#nome do executável: código do projeto + ImGui (que é compilado junto, não linkado) +
#a implementação das libs só-header (fora do PROJECT_SOURCES de propósito: sem os avisos extras)
add_executable(G_E_Youtube ${PROJECT_SOURCES} ${IMGUI_SOURCES} src/engine/third_party_impl.cpp)

#avisos extras do compilador só no nosso código: /W4 no MSVC, -Wall -Wextra no GCC/Clang.
#As bibliotecas (ImGui etc.) ficam no nível padrão, o aviso delas não é nosso pra resolver.
if(MSVC)
    set_source_files_properties(${PROJECT_SOURCES} PROPERTIES COMPILE_OPTIONS "/W4")
else()
    set_source_files_properties(${PROJECT_SOURCES} PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra")
endif()

#todo #include do projeto é escrito a partir de src/ (ex: "engine/rendering/lve_model.hpp"),
#assim o include já diz de qual subsistema o arquivo vem. Regra: engine/ nunca inclui editor/, game/ ou app/
target_include_directories(G_E_Youtube PRIVATE src ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends ${stb_SOURCE_DIR} ${cgltf_SOURCE_DIR})

#linkando as bibliotecas
target_link_libraries(G_E_Youtube PRIVATE glfw Vulkan::Vulkan glm::glm GPUOpen::VulkanMemoryAllocator)

#configuração do glm definida UMA vez pro projeto inteiro (antes estava repetida em 7 arquivos):
#profundidade 0..1 como o Vulkan espera (o padrão do glm é -1..1, do OpenGL). Vale pra todo
#arquivo, então a ordem dos #include deixa de importar.
#(GLM_FORCE_RADIANS foi removido: o glm usa radianos sempre desde a 0.9.6, a macro não faz nada)
target_compile_definitions(G_E_Youtube PRIVATE GLM_FORCE_DEPTH_ZERO_TO_ONE)
#winmm: só existe/é necessária no Windows (timeBeginPeriod, usado pelo limitador de FPS)
if(WIN32)
    target_link_libraries(G_E_Youtube PRIVATE winmm)
endif()

#chamada de verificação de compilação dos shaders
add_custom_target(Shaders DEPENDS ${SPIRV_BINARY_FILES})

#garante que os shaders sejam compilados antes do executável
add_dependencies(G_E_Youtube Shaders)
#----------------------------------------------------------#
