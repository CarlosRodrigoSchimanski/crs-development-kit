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

#incluir nlohmann/json (ler e gravar a cena em arquivo .json)
#baixa só o pacote com o header, não o repositório inteiro (que é grande)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)
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

#incluir doctest (testes automatizados): só um header, sintaxe parecida com a do pytest
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.12
)
FetchContent_MakeAvailable(doctest)
#----------------------------------------------------------#

#incluir ImGuizmo (gizmo de mover/girar/escalar do editor, por cima do ImGui).
#Como o ImGui, é compilado direto no executável; o SOURCE_SUBDIR aponta pra uma pasta sem
#CMakeLists.txt de propósito: assim o FetchContent só baixa (o CMakeLists dele exige um alvo imgui
#que não existe aqui). Sem versões recentes publicadas: fixado num commit (2026-10-10)
FetchContent_Declare(
    imguizmo
    GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
    GIT_TAG 105e4fde45e4e74aae8973d98750820ef7aca7e2
    SOURCE_SUBDIR nao_compilar_pelo_cmake
)
FetchContent_MakeAvailable(imguizmo)
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
    ${CMAKE_SOURCE_DIR}/shaders/gizmo.vert
    ${CMAKE_SOURCE_DIR}/shaders/gizmo.frag
    ${CMAKE_SOURCE_DIR}/shaders/skinned.vert
    ${CMAKE_SOURCE_DIR}/shaders/shadow_skinned.vert
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

#engine: biblioteca usada pelo jogo e pelos testes (tudo de src/engine/).
#Regra: engine/ nunca inclui editor/, game/ ou app/
set(ENGINE_SOURCES
    src/engine/core/lve_window.cpp
    src/engine/core/lve_device.cpp
    src/engine/core/lve_handle.cpp
    src/engine/core/lve_vk_check.cpp
    src/engine/core/lve_input.cpp
    src/engine/rendering/lve_pipeline.cpp
    src/engine/rendering/lve_swap_chain.cpp
    src/engine/rendering/lve_model.cpp
    src/engine/rendering/lve_model_cache.cpp
    src/engine/rendering/lve_renderer.cpp
    src/engine/rendering/lve_camera.cpp
    src/engine/rendering/lve_buffer.cpp
    src/engine/rendering/lve_descriptors.cpp
    src/engine/rendering/lve_texture.cpp
    src/engine/rendering/lve_shadow_map.cpp
    src/engine/rendering/lve_image_barrier.cpp
    src/engine/rendering/lve_upload_batch.cpp
    src/engine/rendering/systems/simple_render_system.cpp
    src/engine/rendering/systems/point_light_system.cpp
    src/engine/rendering/systems/shadow_render_system.cpp
    src/engine/rendering/systems/gizmo_system.cpp
    src/engine/rendering/systems/skinning_system.cpp
    src/engine/scene/lve_game_object.cpp
    src/engine/scene/lve_scene.cpp
    src/engine/scene/scene_serializer.cpp
    src/engine/scene/lve_ray.cpp
    src/engine/physics/lve_physics_system.cpp
    src/engine/gameplay/character_controller_system.cpp
    src/engine/gameplay/camera_rig_system.cpp
    src/engine/animation/lve_animation.cpp
    src/engine/animation/lve_animator_system.cpp
)

#camada de plataforma: uma implementação por sistema operacional, escolhida AQUI (o código não
#tem #ifdef de SO). SO sem implementação própria usa a genérica
if(WIN32)
    list(APPEND ENGINE_SOURCES src/engine/platform/lve_platform_windows.cpp)
else()
    list(APPEND ENGINE_SOURCES src/engine/platform/lve_platform_generic.cpp)
endif()

#jogo/editor: o executável (main, app e editor)
set(APP_SOURCES
    src/main.cpp
    src/app/first_app.cpp
    src/editor/orbit_camera_controller.cpp
    src/editor/lve_imgui.cpp
    src/editor/lve_editor.cpp
)

#avisos extras do compilador só no nosso código: /W4 no MSVC, -Wall -Wextra no GCC/Clang.
#As bibliotecas (ImGui etc.) ficam no nível padrão, o aviso delas não é nosso pra resolver.
if(MSVC)
    set_source_files_properties(${ENGINE_SOURCES} ${APP_SOURCES} PROPERTIES COMPILE_OPTIONS "/W4")
else()
    set_source_files_properties(${ENGINE_SOURCES} ${APP_SOURCES} PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra")
endif()

#a engine + a implementação das libs só-header (fora do ENGINE_SOURCES de propósito: sem os avisos extras)
add_library(ge_engine STATIC ${ENGINE_SOURCES} src/engine/third_party_impl.cpp)
#PUBLIC = vale pra engine E pra quem usa ela (jogo e testes); PRIVATE = só pra engine.
#Todo #include do projeto é escrito a partir de src/ (ex: "engine/rendering/lve_model.hpp")
target_include_directories(ge_engine PUBLIC src PRIVATE ${stb_SOURCE_DIR} ${cgltf_SOURCE_DIR})
target_link_libraries(ge_engine PUBLIC glfw Vulkan::Vulkan glm::glm GPUOpen::VulkanMemoryAllocator)
target_link_libraries(ge_engine PRIVATE nlohmann_json::nlohmann_json)
#configuração do glm definida UMA vez: profundidade 0..1 como o Vulkan espera (o padrão do glm é
#-1..1, do OpenGL). PUBLIC: quem usa a engine tem que enxergar o glm do mesmo jeito
target_compile_definitions(ge_engine PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)
#winmm: só existe/é necessária no Windows (timeBeginPeriod, no lve_platform_windows.cpp)
if(WIN32)
    target_link_libraries(ge_engine PRIVATE winmm)
endif()

#o jogo: app + editor + ImGui e ImGuizmo (compilados junto, não linkados), usando a engine
add_executable(G_E_Youtube ${APP_SOURCES} ${IMGUI_SOURCES} ${imguizmo_SOURCE_DIR}/src/ImGuizmo.cpp)
target_include_directories(G_E_Youtube PRIVATE ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends ${imguizmo_SOURCE_DIR}/src)
target_link_libraries(G_E_Youtube PRIVATE ge_engine)

#chamada de verificação de compilação dos shaders
add_custom_target(Shaders DEPENDS ${SPIRV_BINARY_FILES})

#garante que os shaders sejam compilados antes do executável
add_dependencies(G_E_Youtube Shaders)
#----------------------------------------------------------#

#testes automatizados: executável separado, sem janela nem GPU.
#Rodar todos: ctest --test-dir build -C Debug --output-on-failure
enable_testing()
set(TEST_SOURCES
    tests/test_main.cpp
    tests/test_physics.cpp
    tests/test_character.cpp
    tests/test_scene.cpp
    tests/test_ray.cpp
    tests/test_transform.cpp
    tests/test_camera_rig.cpp
    tests/test_animation.cpp
)
add_executable(G_E_Youtube_tests ${TEST_SOURCES})
if(MSVC)
    target_compile_options(G_E_Youtube_tests PRIVATE /W4)
else()
    target_compile_options(G_E_Youtube_tests PRIVATE -Wall -Wextra)
endif()
target_link_libraries(G_E_Youtube_tests PRIVATE ge_engine doctest::doctest)
add_test(NAME engine_tests COMMAND G_E_Youtube_tests)
#----------------------------------------------------------#
