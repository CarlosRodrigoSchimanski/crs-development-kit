#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <timeapi.h>
#endif

#include "app/first_app.hpp"
#include <stdexcept>
#include <cstdlib>
#include <iostream>

int main() {
#ifdef _WIN32
    //pede ao Windows granularidade de 1ms pro temporizador do sistema (padrão costuma
    //ser ~15ms); sem isso, std::this_thread::sleep_for não consegue precisão suficiente
    //pro limitador de FPS acertar o alvo
    timeBeginPeriod(1);
#endif

    int result = EXIT_SUCCESS;
    try {
        //criar o app também fica dentro do try: o construtor carrega modelos, cria o device etc.
        //e pode lançar exceção tanto quanto o run()
        lve::FirstApp app;
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        result = EXIT_FAILURE;
    }

    //um único caminho de saída: o timeEndPeriod roda dando certo ou dando erro
#ifdef _WIN32
    timeEndPeriod(1);
#endif
    return result;
}