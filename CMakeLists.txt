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

#incluir tinyobjloader (carregamento de modelos .obj)
FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG v1.0.7
)
#a v1.0.7 é antiga e pede uma versão mínima de CMake que o CMake atual já removeu;
#isso avisa pra tratar como se pedisse 3.5 em vez da original
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_MakeAvailable(tinyobjloader)
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

#incluir stb_image (carregamento de arquivos de imagem pra textura)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
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
find_program(GLSL_VALIDATOR glslc REQUIRED)
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
        COMMAND ${GLSL_VALIDATOR} ${SHADER} -o ${SPIRV}
        DEPENDS ${SHADER}
    )
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach()
#----------------------------------------------------------#

#finalização: monta o executável e conecta tudo
#incluir vulkan
find_package(Vulkan REQUIRED)

#nome do executável e arquivo principal
add_executable(G_E_Youtube
    src/main.cpp
    src/lve_window.cpp
    src/lve_device.cpp
    src/first_app.cpp
    src/lve_pipeline.cpp
    src/lve_swap_chain.cpp
    src/lve_model.cpp
    src/lve_renderer.cpp
    src/lve_game_object.cpp
    src/systems/simple_render_system.cpp
    src/systems/point_light_system.cpp
    src/systems/shadow_render_system.cpp
    src/lve_camera.cpp
    src/lve_buffer.cpp
    src/lve_descriptors.cpp
    src/orbit_camera_controller.cpp
    src/lve_imgui.cpp
    src/lve_texture.cpp
    src/lve_shadow_map.cpp
    ${IMGUI_SOURCES}
)

#permite que arquivos em subpastas (ex: src/systems/) incluam headers de src/ pelo nome, sem caminho relativo
target_include_directories(G_E_Youtube PRIVATE src ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends ${stb_SOURCE_DIR} ${cgltf_SOURCE_DIR})

#linkando as bibliotecas
target_link_libraries(G_E_Youtube PRIVATE glfw Vulkan::Vulkan glm::glm tinyobjloader)
#winmm: só existe/é necessária no Windows (timeBeginPeriod, usado pelo limitador de FPS)
if(WIN32)
    target_link_libraries(G_E_Youtube PRIVATE winmm)
endif()

#chamada de verificação de compilação dos shaders
add_custom_target(Shaders DEPENDS ${SPIRV_BINARY_FILES})

#garante que os shaders sejam compilados antes do executável
add_dependencies(G_E_Youtube Shaders)
#----------------------------------------------------------#
