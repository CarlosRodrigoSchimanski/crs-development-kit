#pragma once

//camada de plataforma: o que a engine precisa pedir ao sistema operacional, numa interface única.
//Cada SO tem a sua implementação num .cpp próprio (crs_platform_windows.cpp, crs_platform_generic.cpp...),
//e quem escolhe qual compilar é o CMake: nada de #ifdef de SO espalhado pelo resto do código.
namespace crsdk::platform {

    //ajustes do SO que valem pro programa inteiro. Criar UM no começo do main: liga no construtor
    //e desfaz no destrutor (RAII), então desfaz mesmo se o programa sair por exceção.
    //Windows: pede ao relógio do sistema precisão de 1ms (o padrão é ~15ms), senão o sleep do
    //limitador de FPS não consegue acertar o alvo.
    class PlatformScope {
        public:
            PlatformScope();
            ~PlatformScope();

            PlatformScope(const PlatformScope &) = delete;
            PlatformScope &operator=(const PlatformScope &) = delete;
    };

    //o SO trava o loop principal enquanto a janela é redimensionada? Se sim, o app precisa
    //desenhar de dentro do refresh callback da janela, senão a imagem congela durante o arrasto.
    //Windows: sim (o arrasto da borda roda num loop modal do próprio Windows)
    bool needsRedrawDuringResize();
}
