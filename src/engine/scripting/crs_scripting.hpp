#pragma once

#include <memory>
#include <string>

namespace crsdk {
    //o Python embutido na engine: liga o interpretador e roda scripts .py
    //(só um por programa: o Python não foi feito pra ter dois ao mesmo tempo)
    class CrsScripting {
        public:
            CrsScripting();
            ~CrsScripting();

            CrsScripting(const CrsScripting&) = delete;
            CrsScripting& operator=(const CrsScripting&) = delete;

            //roda um arquivo .py; erro no script vira exceção com a mensagem do Python
            void runFile(const std::string &filepath);

        private:
            //o interpretador fica escondido aqui dentro (quem inclui este header não precisa do Python)
            struct Interpreter;
            std::unique_ptr<Interpreter> interpreter;
    };
}
