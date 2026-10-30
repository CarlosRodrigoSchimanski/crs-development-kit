#include "engine/scripting/crs_scripting.hpp"
#include "engine/scripting/crs_python_api.hpp"

#include <pybind11/embed.h>

#include <filesystem>
#include <stdexcept>

namespace py = pybind11;

namespace crsdk {
    struct CrsScripting::Interpreter {
        //liga o Python ao ser criado e desliga ao ser destruído
        py::scoped_interpreter guard{};
    };

    CrsScripting::CrsScripting() : interpreter{std::make_unique<Interpreter>()} {}

    CrsScripting::~CrsScripting() {
        //o Python desliga junto: o módulo crsdk não pode ficar apontando pra nada
        scriptContext() = {};
    }

    //cada script roda com as próprias variáveis (um não enxerga as do outro)
    static py::dict newScope(const std::string &filename) {
        py::dict scope{};
        scope["__builtins__"] = py::module_::import("builtins");
        scope["__file__"] = filename;
        return scope;
    }

    void CrsScripting::runFile(const std::string &filepath) {
        if (!std::filesystem::exists(filepath)) {
            throw std::runtime_error("script não encontrado: " + filepath);
        }
        try {
            py::eval_file(filepath, newScope(filepath));
        } catch (const py::error_already_set &e) {
            throw std::runtime_error("erro no script " + filepath + ":\n" + e.what());
        }
    }

    void CrsScripting::runString(const std::string &code) {
        try {
            py::exec(code, newScope("<string>"));
        } catch (const py::error_already_set &e) {
            throw std::runtime_error(std::string{"erro no script:\n"} + e.what());
        }
    }

    void CrsScripting::setScene(CrsScene *scene) {
        scriptContext().scene = scene;
    }

    void CrsScripting::setInput(const CrsInput *input) {
        scriptContext().input = input;
    }

    void CrsScripting::setWindow(const CrsWindow *window) {
        scriptContext().window = window;
    }
}
