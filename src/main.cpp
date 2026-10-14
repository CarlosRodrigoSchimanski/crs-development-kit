#include "app/first_app.hpp"
#include "engine/platform/lve_platform.hpp"

#include <stdexcept>
#include <cstdlib>
#include <iostream>

int main() {
    //ajustes do sistema operacional pro programa inteiro (ver lve_platform.hpp);
    //desfeitos sozinhos no fim do main, dando certo ou dando erro
    lve::platform::PlatformScope platform;

    try {
        //criar o app também fica dentro do try: o construtor carrega modelos, cria o device etc.
        //e pode lançar exceção tanto quanto o run()
        lve::FirstApp app;
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
