#include "engine/scripting/lve_scripting.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace lve;

static std::string tempPath(const std::string &name) {
    return (std::filesystem::temp_directory_path() / name).generic_string();
}

static void writeText(const std::string &path, const std::string &text) {
    std::ofstream out{path};
    out << text;
}

static std::string readText(const std::string &path) {
    std::ifstream in{path};
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

TEST_CASE("Python: o script roda de verdade") {
    LveScripting scripting;
    std::string script = tempPath("ge_teste_script.py");
    std::string result = tempPath("ge_teste_resultado.txt");
    //o script escreve um arquivo: se ele existir com o texto certo, o Python rodou
    writeText(script, "with open(r'" + result + "', 'w') as f:\n    f.write(str(6 * 7))\n");

    scripting.runFile(script);

    CHECK(readText(result) == "42");
    std::filesystem::remove(script);
    std::filesystem::remove(result);
}

TEST_CASE("Python: erro no script vira exceção com a mensagem do Python") {
    LveScripting scripting;
    std::string script = tempPath("ge_teste_erro.py");
    writeText(script, "x = 1 / 0\n");

    CHECK_THROWS_WITH_AS(scripting.runFile(script), doctest::Contains("ZeroDivisionError"), std::runtime_error);
    std::filesystem::remove(script);
}

TEST_CASE("Python: script que não existe é erro") {
    LveScripting scripting;
    CHECK_THROWS(scripting.runFile(tempPath("ge_nao_existe.py")));
}
