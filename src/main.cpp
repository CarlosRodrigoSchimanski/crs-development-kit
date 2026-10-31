#include "app/first_app.hpp"
#include "engine/platform/crs_platform.hpp"

#include <stdexcept>
#include <cstdlib>
#include <filesystem>
#include <iostream>

//todos os caminhos da engine são relativos ("assets/...", "shaders/..."). Rodando da raiz do projeto
//(desenvolvimento) eles já existem; instalada, a engine é aberta por atalho e a pasta atual pode ser
//qualquer uma: aí passa a trabalhar na pasta do .exe, onde o instalador põe assets/ e shaders/
static void findDataFolder() {
    namespace fs = std::filesystem;
    if (fs::exists("assets")) return;
    const fs::path folder = crsdk::platform::executablePath().parent_path();
    if (!folder.empty() && fs::exists(folder / "assets")) fs::current_path(folder);
}

int main() {
    //ajustes do sistema operacional pro programa inteiro (ver crs_platform.hpp);
    //desfeitos sozinhos no fim do main, dando certo ou dando erro
    crsdk::platform::PlatformScope platform;
    findDataFolder();

    try {
        //criar o app também fica dentro do try: o construtor carrega modelos, cria o device etc.
        //e pode lançar exceção tanto quanto o run()
        crsdk::FirstApp app;
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
