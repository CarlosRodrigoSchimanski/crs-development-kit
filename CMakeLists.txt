#versão do cmake
cmake_minimum_required(VERSION 3.20)

#nome e linguagem do projeto
#a versão vai pro instalador (CRSDK-0.1.0-setup.exe)
project(CRSDK VERSION 0.1.0 LANGUAGES CXX)

#versão do c++ a ser utilizada
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#Release com LTO (o linker otimiza juntando as funções de todos os arquivos, inclusive entre a
#engine e o app). Só no Release: no Debug deixaria a compilação lenta sem ganho.
#O Release também define NDEBUG, que desliga as validation layers (crs_device.hpp)
include(CheckIPOSupported)
check_ipo_supported(RESULT LTO_SUPPORTED OUTPUT LTO_ERROR)
if(LTO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
else()
    message(WARNING "LTO não suportado: ${LTO_ERROR}")
endif()
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


#incluir nativefiledialog-extended (janelas de abrir/salvar arquivo do sistema;
#o código de cada sistema operacional fica dentro dela)
FetchContent_Declare(
    nfd
    GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
    GIT_TAG v1.2.1
)
FetchContent_MakeAvailable(nfd)
#----------------------------------------------------------#

#Python embutido (scripts dos objetos): o 3.12 instalado no sistema, e o pybind11 pra ligar C++ <-> Python.
#Fixo no 3.12: o pybind11 2.13 é estável com ele (o 3.14 também instalado é novo demais)
find_package(Python 3.12 EXACT REQUIRED COMPONENTS Interpreter Development.Embed)
set(PYBIND11_FINDPYTHON ON)
FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG v2.13.6
)
FetchContent_MakeAvailable(pybind11)

#Python portátil (embeddable) pra ir junto da engine instalada: quem instala não precisa ter Python.
#Mesma versão do instalado aqui (3.12.10). Ele lê o python312._pth ao lado do python312.dll e usa só
#a biblioteca padrão de dentro do python312.zip (ignora qualquer Python instalado no PC)
if(WIN32)
    FetchContent_Declare(
        python_embed
        URL https://www.python.org/ftp/python/3.12.10/python-3.12.10-embed-amd64.zip
        URL_HASH SHA256=4acbed6dd1c744b0376e3b1cf57ce906f9dc9e95e68824584c8099a63025a3c3
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(python_embed)
endif()
#----------------------------------------------------------#

#incluir Jolt Physics (física: corpos com forma de caixa, esfera, cápsula; personagem que desliza na
#parede e sobe rampa; mais tarde o heightfield do terreno). A de Horizon Forbidden West e do Godot 4.
#O CMakeLists dela fica na subpasta Build/
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)  #mesmo runtime do resto (DLL), senão não linka
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)  #o LTO do projeto (lá em cima) já vale pra ela
#computação na GPU da Jolt (ainda não usada): sem ela não puxa DirectX nem um segundo Vulkan
set(JPH_USE_DX12 OFF CACHE BOOL "" FORCE)
set(JPH_USE_VK OFF CACHE BOOL "" FORCE)
set(JPH_USE_MTL OFF CACHE BOOL "" FORCE)
set(JPH_USE_CPU_COMPUTE OFF CACHE BOOL "" FORCE)
#instruções do processador: até SSE4.2 (qualquer PC de 2008 pra cá). AVX/AVX2 dariam um pouco mais de
#velocidade, mas o .exe fecharia sozinho em processador sem elas (alguns Pentium/Celeron até ~2020)
set(USE_AVX OFF CACHE BOOL "" FORCE)
set(USE_AVX2 OFF CACHE BOOL "" FORCE)
set(USE_LZCNT OFF CACHE BOOL "" FORCE)
set(USE_TZCNT OFF CACHE BOOL "" FORCE)
set(USE_F16C OFF CACHE BOOL "" FORCE)
set(USE_FMADD OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    jolt
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG v5.6.0
    SOURCE_SUBDIR Build
)
FetchContent_MakeAvailable(jolt)
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
    #InputText com std::string (campo de texto dos args e do Generate Component)
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
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
    ${CMAKE_SOURCE_DIR}/shaders/selection_mask.vert
    ${CMAKE_SOURCE_DIR}/shaders/selection_mask_skinned.vert
    ${CMAKE_SOURCE_DIR}/shaders/selection_mask.frag
    ${CMAKE_SOURCE_DIR}/shaders/selection_outline.vert
    ${CMAKE_SOURCE_DIR}/shaders/selection_outline.frag
    ${CMAKE_SOURCE_DIR}/shaders/terrain.vert
    ${CMAKE_SOURCE_DIR}/shaders/terrain.frag
)
#loop para compilar cada shader
foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    #define o caminho de saída do arquivo e adiciona .spv
    set(SPIRV ${CMAKE_SOURCE_DIR}/shaders/${SHADER_NAME}.spv)
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${GLSLC} ${SHADER} -o ${SPIRV}
        #os shaders incluem esses arquivos: mudar um deles tem que recompilar todos
        DEPENDS ${SHADER} ${CMAKE_SOURCE_DIR}/shaders/global_ubo.glsl ${CMAKE_SOURCE_DIR}/shaders/lighting.glsl
                ${CMAKE_SOURCE_DIR}/shaders/terrain_push.glsl ${CMAKE_SOURCE_DIR}/shaders/pbr.glsl
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
    src/engine/core/crs_window.cpp
    src/engine/core/crs_device.cpp
    src/engine/core/crs_handle.cpp
    src/engine/core/crs_vk_check.cpp
    src/engine/core/crs_input.cpp
    src/engine/rendering/crs_pipeline.cpp
    src/engine/rendering/crs_swap_chain.cpp
    src/engine/rendering/crs_model.cpp
    src/engine/rendering/crs_model_cache.cpp
    src/engine/rendering/crs_renderer.cpp
    src/engine/rendering/crs_camera.cpp
    src/engine/rendering/crs_buffer.cpp
    src/engine/rendering/crs_descriptors.cpp
    src/engine/rendering/crs_texture.cpp
    src/engine/rendering/crs_material_files.cpp
    src/engine/rendering/crs_shadow_map.cpp
    src/engine/rendering/crs_image_barrier.cpp
    src/engine/rendering/crs_upload_batch.cpp
    src/engine/rendering/systems/simple_render_system.cpp
    src/engine/rendering/systems/point_light_system.cpp
    src/engine/rendering/systems/shadow_render_system.cpp
    src/engine/rendering/systems/gizmo_system.cpp
    src/engine/rendering/systems/selection_outline_system.cpp
    src/engine/rendering/systems/terrain_render_system.cpp
    src/engine/rendering/systems/skinning_system.cpp
    src/engine/scene/crs_game_object.cpp
    src/engine/scene/crs_scene.cpp
    src/engine/scene/scene_serializer.cpp
    src/engine/scene/crs_ray.cpp
    src/engine/terrain/crs_terrain.cpp
    src/engine/physics/crs_physics_system.cpp
    src/engine/gameplay/character_controller_system.cpp
    src/engine/gameplay/camera_rig_system.cpp
    src/engine/gameplay/character_animation_system.cpp
    src/engine/scripting/crs_scripting.cpp
    src/engine/scripting/crs_python_api.cpp
    src/engine/animation/crs_animation.cpp
    src/engine/animation/crs_gltf_animation.cpp
    src/engine/animation/crs_animation_file.cpp
    src/engine/animation/crs_animation_library.cpp
    src/engine/animation/crs_animator_system.cpp
)

#camada de plataforma: uma implementação por sistema operacional, escolhida AQUI (o código não
#tem #ifdef de SO). SO sem implementação própria usa a genérica
if(WIN32)
    list(APPEND ENGINE_SOURCES src/engine/platform/crs_platform_windows.cpp)
else()
    list(APPEND ENGINE_SOURCES src/engine/platform/crs_platform_generic.cpp)
endif()

#jogo/editor: o executável (main, app e editor)
set(APP_SOURCES
    src/main.cpp
    src/app/first_app.cpp
    src/editor/orbit_camera_controller.cpp
    src/editor/crs_imgui.cpp
    src/editor/crs_editor.cpp
    src/editor/crs_editor_theme.cpp
    src/editor/file_dialog.cpp
)

#avisos extras do compilador só no nosso código: /W4 no MSVC, -Wall -Wextra no GCC/Clang.
#As bibliotecas (ImGui etc.) ficam no nível padrão, o aviso delas não é nosso pra resolver.
if(MSVC)
    set_source_files_properties(${ENGINE_SOURCES} ${APP_SOURCES} PROPERTIES COMPILE_OPTIONS "/W4")
else()
    set_source_files_properties(${ENGINE_SOURCES} ${APP_SOURCES} PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra")
endif()

#a engine + a implementação das libs só-header (fora do ENGINE_SOURCES de propósito: sem os avisos extras)
add_library(crsdk_engine STATIC ${ENGINE_SOURCES} src/engine/third_party_impl.cpp)
#PUBLIC = vale pra engine E pra quem usa ela (jogo e testes); PRIVATE = só pra engine.
#Todo #include do projeto é escrito a partir de src/ (ex: "engine/rendering/crs_model.hpp")
target_include_directories(crsdk_engine PUBLIC src PRIVATE ${stb_SOURCE_DIR} ${cgltf_SOURCE_DIR})
target_link_libraries(crsdk_engine PUBLIC glfw Vulkan::Vulkan glm::glm GPUOpen::VulkanMemoryAllocator)
target_link_libraries(crsdk_engine PRIVATE nlohmann_json::nlohmann_json)
#o Python fica escondido dentro da engine: quem usa só vê o CrsScripting
target_link_libraries(crsdk_engine PRIVATE pybind11::embed)
#a Jolt também: fica atrás do PhysicsSystem (nenhum header da engine inclui ela)
target_link_libraries(crsdk_engine PRIVATE Jolt)
#configuração do glm definida UMA vez: profundidade 0..1 como o Vulkan espera (o padrão do glm é
#-1..1, do OpenGL). PUBLIC: quem usa a engine tem que enxergar o glm do mesmo jeito
target_compile_definitions(crsdk_engine PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)
#winmm: só existe/é necessária no Windows (timeBeginPeriod, no crs_platform_windows.cpp)
if(WIN32)
    target_link_libraries(crsdk_engine PRIVATE winmm)
endif()

#o jogo: app + editor + ImGui e ImGuizmo (compilados junto, não linkados), usando a engine
add_executable(CRSDK ${APP_SOURCES} ${IMGUI_SOURCES} ${imguizmo_SOURCE_DIR}/src/ImGuizmo.cpp)
target_include_directories(CRSDK PRIVATE ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends ${imguizmo_SOURCE_DIR}/src)
target_link_libraries(CRSDK PRIVATE crsdk_engine)
#o editor grava a vista da câmera dele em .json
target_link_libraries(CRSDK PRIVATE nlohmann_json::nlohmann_json)
#janelas de abrir/salvar arquivo do editor
target_link_libraries(CRSDK PRIVATE nfd)

#chamada de verificação de compilação dos shaders
add_custom_target(Shaders DEPENDS ${SPIRV_BINARY_FILES})

#garante que os shaders sejam compilados antes do executável
add_dependencies(CRSDK Shaders)
#----------------------------------------------------------#

#testes automatizados: executável separado, sem janela nem GPU.
#Rodar todos: ctest --test-dir build -C Debug --output-on-failure
enable_testing()
set(TEST_SOURCES
    tests/test_main.cpp
    tests/test_physics.cpp
    tests/test_character.cpp
    tests/test_scene.cpp
    tests/test_scene_serializer.cpp
    tests/test_ray.cpp
    tests/test_transform.cpp
    tests/test_camera_rig.cpp
    tests/test_animation.cpp
    tests/test_animation_file.cpp
    tests/test_scripting.cpp
    tests/test_terrain.cpp
    tests/test_material_files.cpp
)
add_executable(CRSDK_tests ${TEST_SOURCES})
if(MSVC)
    target_compile_options(CRSDK_tests PRIVATE /W4)
else()
    target_compile_options(CRSDK_tests PRIVATE -Wall -Wextra)
endif()
target_link_libraries(CRSDK_tests PRIVATE crsdk_engine doctest::doctest)
add_test(NAME engine_tests COMMAND CRSDK_tests)
#----------------------------------------------------------#

#instalação: monta a pasta da engine pronta pra distribuir (o .exe acha assets/ e shaders/ ao lado dele).
#Tudo no componente "engine": as bibliotecas baixadas (glfw, glm...) têm regras de instalação próprias
#(headers e .lib pra desenvolvedor) que não vão pra quem só usa a engine.
#  cmake --install build --config Release --component engine --prefix build/install
install(TARGETS CRSDK RUNTIME DESTINATION . COMPONENT engine)
install(FILES ${SPIRV_BINARY_FILES} DESTINATION shaders COMPONENT engine)
#textures/ fica fora: as texturas já vão dentro dos .glb (lá são só os arquivos originais)
install(DIRECTORY assets/ DESTINATION assets COMPONENT engine
    PATTERN "textures" EXCLUDE
    PATTERN "__pycache__" EXCLUDE
    PATTERN "*.editor.json" EXCLUDE)
if(WIN32)
    #Python portátil inteiro na pasta do .exe (o python312.dll tem que ficar junto do CRSDK.exe)
    install(DIRECTORY ${python_embed_SOURCE_DIR}/ DESTINATION . COMPONENT engine)
    #runtime do C++ do Visual Studio (msvcp140.dll etc.) junto do .exe: o PC não precisa do VC++ Redistributable
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT engine)
    include(InstallRequiredSystemLibraries)
endif()

#instalador (Windows, Inno Setup): instala a pasta acima e gera build/installer/CRSDK-<versão>-setup.exe
#  cmake --build build --config Release --target installer
find_program(ISCC iscc PATHS "$ENV{LOCALAPPDATA}/Programs/Inno Setup 6" "C:/Program Files (x86)/Inno Setup 6" "$ENV{ProgramFiles}/Inno Setup 6")
if(WIN32 AND ISCC)
    add_custom_target(installer
        #começa de uma pasta limpa: arquivo apagado do projeto não pode sobrar no instalador
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/install
        COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --config $<CONFIG> --component engine --prefix ${CMAKE_BINARY_DIR}/install
        COMMAND ${ISCC} /Qp
            /DAppVersion=${PROJECT_VERSION}
            /DSourceDir=${CMAKE_BINARY_DIR}/install
            /DOutputDir=${CMAKE_BINARY_DIR}/installer
            ${CMAKE_SOURCE_DIR}/installer/crsdk.iss
        DEPENDS CRSDK
        COMMENT "Gerando o instalador com o Inno Setup"
        VERBATIM
    )
endif()
#----------------------------------------------------------#
