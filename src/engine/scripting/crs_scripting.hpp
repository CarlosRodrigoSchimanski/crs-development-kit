#pragma once

#include "engine/scene/crs_game_object.hpp"

#include <memory>
#include <string>
#include <vector>

namespace crsdk {
    class CrsScene;
    class CrsInput;
    class CrsWindow;

    //um arg como a classe declara no dicionário args: nome e valor padrão
    struct ComponentArgInfo {
        std::string name;
        ScriptArg defaultValue;
    };

    //quanto um componente gasta no update(), pra achar script pesado
    struct ComponentTiming {
        std::string label;     //"Objeto: arquivo.Classe"
        float milliseconds;    //média dos últimos frames (não treme como o valor de um frame só)
    };

    //uma classe de componente achada na pasta dos componentes (o que o editor lista no Add Component)
    struct ComponentInfo {
        std::string module;     //"girar", ou "comum.pulo" numa subpasta
        std::string className;
        std::string description;  //a primeira linha da docstring da classe
        //na ordem do dicionário; arg de tipo que o painel não sabe mostrar (lista...) fica de fora
        std::vector<ComponentArgInfo> args;
    };

    //o Python embutido na engine: liga o interpretador e roda scripts .py
    //(só um por programa: o Python não foi feito pra ter dois ao mesmo tempo)
    //os scripts enxergam a engine pelo módulo "crsdk" (crs_python_api.cpp; referência em assets/scripts/crsdk/)
    class CrsScripting {
        public:
            CrsScripting();
            ~CrsScripting();

            CrsScripting(const CrsScripting&) = delete;
            CrsScripting& operator=(const CrsScripting&) = delete;

            //roda um arquivo .py; erro no script vira exceção com a mensagem do Python
            void runFile(const std::string &filepath);
            //roda um texto com código Python (mesmas regras do runFile)
            void runString(const std::string &code);

            //pasta onde o "import" dos scripts procura arquivos
            //entra no começo do sys.path, como a pasta do script num Python normal
            void addImportFolder(const std::string &folder);
            //esquece os módulos importados dessas pastas: o próximo import lê o arquivo novo
            //chamar ao dar Play: assim a edição num arquivo importado vale sem fechar a engine
            void forgetProjectModules();
            
            //Play: cria os componentes Python de cada objeto da cena e chama o start(args) de cada um
            void startComponents(CrsScene &scene);
            //chama o update() de todos os componentes (dt = segundos desde o frame anterior: crsdk.deltaTime)
            void updateComponents(float dt);
            //fim do Play: apaga os componentes (antes de a cena do Play sumir: eles apontam pros objetos dela)
            void stopComponents();
            //erros de componente desde a última chamada (o componente com erro para de rodar; o jogo continua)
            std::vector<std::string> takeErrors();
            //o tempo do update() de cada componente rodando, na ordem em que rodam
            std::vector<ComponentTiming> componentTimings() const;

            //as classes de componente dos .py da pasta (e subpastas), em ordem de arquivo e classe.
            //Lê os arquivos de novo (esquece os módulos antes); arquivo com erro vai pro takeErrors
            std::vector<ComponentInfo> findComponents(const std::string &folder);
            //cria o .py de um componente novo, já com o modelo (classe, args, start, update), e devolve
            //o nome do módulo. "PuloDuplo" vira pulo_duplo.py. Erro: nome inválido, arquivo que já
            //existe ou nome igual ao de um módulo do Python (o arquivo esconderia o módulo de verdade)
            std::string createComponentFile(const std::string &folder, const std::string &className);

            //o que os scripts veem como crsdk.scene / crsdk.input / crsdk.window (nullptr = nenhum)
            //quem chama garante que continuam vivos enquanto estiverem ligados
            void setScene(CrsScene *scene);
            void setInput(const CrsInput *input);
            void setWindow(const CrsWindow *window);

        private:
            //o interpretador fica escondido aqui dentro (quem inclui este header não precisa do Python)
            struct Interpreter;
            std::unique_ptr<Interpreter> interpreter;
    };
}
