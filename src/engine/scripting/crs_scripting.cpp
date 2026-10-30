#include "engine/scripting/crs_scripting.hpp"

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

    CrsScripting::~CrsScripting() = default;

    void CrsScripting::runFile(const std::string &filepath) {
        if (!std::filesystem::exists(filepath)) {
            throw std::runtime_error("script não encontrado: " + filepath);
        }
        try {
            //cada script roda com as próprias variáveis (um não enxerga as do outro)
            py::dict scope{};
            scope["__builtins__"] = py::module_::import("builtins");
            scope["__file__"] = filepath;
            py::eval_file(filepath, scope);
        } catch (const py::error_already_set &e) {
            throw std::runtime_error("erro no script " + filepath + ":\n" + e.what());
        }
    }
}
