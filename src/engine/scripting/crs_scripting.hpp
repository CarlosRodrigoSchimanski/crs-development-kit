#pragma once

#include <memory>
#include <string>

namespace crsdk {
    class CrsScene;
    class CrsInput;
    class CrsWindow;

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
