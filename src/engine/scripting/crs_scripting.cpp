#include "engine/scripting/crs_scripting.hpp"
#include "engine/scripting/crs_python_api.hpp"
#include "engine/scene/crs_scene.hpp"

#include <pybind11/embed.h>
#include <pybind11/stl.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>


namespace py = pybind11;

namespace crsdk {

    struct RunningComponent {
        py::object instance;
        std::string label; //arquivo.Classe para mensagem de erro
        float milliseconds = 0.f;  //tempo do update(), em média móvel
        bool measured = false;     //já tem uma medida (a primeira entra inteira na média)
    };
    
    struct CrsScripting::Interpreter {
        //liga o Python ao ser criado e desliga ao ser destruído
        py::scoped_interpreter guard{};
        //as pastas do addImportFolder, já no formato que o Python usa
        std::vector<std::string> importFolders{};
        std::vector<RunningComponent> components{};
        std::vector<std::string> errors{};
    };

    CrsScripting::CrsScripting() : interpreter{std::make_unique<Interpreter>()} {
        //sem pycache nas pastas do projeto
        py::module_::import("sys").attr("dont_write_bytecode") = true;
    }

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

    void CrsScripting::addImportFolder(const std::string &folder) {
        py::object path = py::module_::import("os").attr("path");
        std::string absolute = path.attr("abspath")(folder).cast<std::string>();
        py::list sysPath = py::module_::import("sys").attr("path");
        if (sysPath.contains(absolute)) return;

        sysPath.insert(0, absolute);
        //join com "" põe a barra no fim
        interpreter->importFolders.push_back(path.attr("normcase")(path.attr("join")(absolute, "")).cast<std::string>());
    }

    void CrsScripting::forgetProjectModules() {
        py::dict modules = py::module_::import("sys").attr("modules");
        py::object path = py::module_::import("os").attr("path");

        //primeiro junta os nomes: não dá pra apagar do dicionário enquanto anda nele
        std::vector<std::string> toForget{};
        for (auto item : modules) {
            py::object file = py::getattr(item.second, "__file__", py::none());
            if (file.is_none()) continue;  //módulo embutido (crsdk, sys...): não tem arquivo
            std::string filePath = path.attr("normcase")(path.attr("abspath")(file)).cast<std::string>();
            for (auto &folder : interpreter->importFolders) {
                if (filePath.rfind(folder, 0) == 0) toForget.push_back(item.first.cast<std::string>());
            }
        }
        for (auto &name : toForget) modules.attr("pop")(name);
        //o Python também guarda a lista de arquivos de cada pasta: arquivo novo precisa disso pra ser achado
        py::module_::import("importlib").attr("invalidate_caches")();
    }

    void CrsScripting::startComponents(CrsScene &scene) {
        stopComponents();
        py::object importlib = py::module_::import("importlib");
        py::object builtins = py::module_::import("builtins");
        py::object baseClass = py::module_::import("crsdk").attr("Component");

        //em ordem de id: os start() rodam sempre na mesma ordem
        std::vector<CrsGameObject*> objects{};
        for (auto &kv : scene.getGameObjects()) objects.push_back(&kv.second);
        std::sort(objects.begin(), objects.end(), [](auto *a, auto *b) { return a->getId() < b->getId(); });

        for (CrsGameObject *obj : objects) {
            for (const ScriptComponent &script : obj->scripts) {
                std::string label = obj->name + ": " + script.module + "." + script.className;
                try {
                    py::object componentClass = importlib.attr("import_module")(script.module).attr(script.className.c_str());
                    if (!py::isinstance<py::type>(componentClass) || !builtins.attr("issubclass")(componentClass, baseClass).cast<bool>()) {
                        throw std::runtime_error("não é uma classe de componente (herde de crsdk.Component)");
                    }
                    //o objeto vai por referência: o componente mexe no objeto de verdade da cena
                    py::object instance = componentClass(py::cast(obj, py::return_value_policy::reference));
                    //args: uma cópia dos padrões da classe (no passo 3d o editor muda cada um por objeto)
                    //deepcopy: o py::dict(...) de um dict não copia, e uma lista dentro dos args também precisa ser cópia
                    py::object args = py::module_::import("copy").attr("deepcopy")(componentClass.attr("args"));
                    //os args mudados neste objeto por cima dos padrões
                    //(nome que a classe não tem mais é ignorado: o arg foi apagado ou renomeado no .py)
                    for (const auto &[name, value] : script.args) {
                        if (args.contains(name)) args[py::str(name)] = py::cast(value);
                    }

                    instance.attr("start")(args);
                    interpreter->components.push_back({instance, label});
                } catch (const std::exception &e) {
                    interpreter->errors.push_back(label + "\n" + e.what());
                }
            }
        }
    }

    void CrsScripting::updateComponents(float dt) {
        scriptContext().deltaTime = dt;
        auto &components = interpreter->components;
        for (auto it = components.begin(); it != components.end();) {
            try {
                auto begin = std::chrono::steady_clock::now();
                it->instance.attr("update")();
                float ms = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - begin).count();
                //média móvel, como o FPS da tela: o número fica legível
                it->milliseconds = it->measured ? it->milliseconds + (ms - it->milliseconds) * 0.1f : ms;
                it->measured = true;
                ++it;
            } catch (const std::exception &e) {
                //o componente com erro sai: um erro por frame lotaria a tela, e o resto do jogo continua
                interpreter->errors.push_back(it->label + "\n" + e.what());
                it = components.erase(it);
            }
        }
    }

    void CrsScripting::stopComponents() {
        interpreter->components.clear();
    }

    std::vector<std::string> CrsScripting::takeErrors() {
        //devolve a lista e deixa uma vazia no lugar
        return std::exchange(interpreter->errors, {});
    }

    std::vector<ComponentTiming> CrsScripting::componentTimings() const {
        std::vector<ComponentTiming> timings{};
        for (const RunningComponent &component : interpreter->components) {
            timings.push_back({component.label, component.milliseconds});
        }
        return timings;
    }

    //valor padrão de um arg, se for de um tipo que o painel sabe mostrar
    static std::optional<ScriptArg> argFromPython(py::handle value) {
        //bool antes de int: no Python, True também é um int
        if (py::isinstance<py::bool_>(value)) return ScriptArg{value.cast<bool>()};
        if (py::isinstance<py::int_>(value)) return ScriptArg{value.cast<int>()};
        if (py::isinstance<py::float_>(value)) return ScriptArg{value.cast<float>()};
        if (py::isinstance<py::str>(value)) return ScriptArg{value.cast<std::string>()};
        return std::nullopt;
    }

    //nome do módulo de um .py dentro da pasta: comum/pulo.py -> "comum.pulo" (vazio = não é módulo)
    static std::string moduleName(const std::filesystem::path &relative) {
        std::string name{};
        for (const auto &part : relative.parent_path()) name += part.string() + ".";
        std::string stem = relative.stem().string();
        //o __init__.py é o próprio pacote (a pasta)
        if (stem == "__init__") return name.empty() ? "" : name.substr(0, name.size() - 1);
        return name + stem;
    }

    std::vector<ComponentInfo> CrsScripting::findComponents(const std::string &folder) {
        std::vector<ComponentInfo> found{};
        if (!std::filesystem::is_directory(folder)) return found;
        //o arquivo pode ter mudado desde a última vez
        forgetProjectModules();

        namespace fs = std::filesystem;
        std::vector<std::string> modules{};
        for (auto it = fs::recursive_directory_iterator{folder}; it != fs::recursive_directory_iterator{}; ++it) {
            std::string filename = it->path().filename().string();
            //pastas escondidas e o __pycache__ ficam de fora
            if (it->is_directory() && (filename.starts_with(".") || filename == "__pycache__")) {
                it.disable_recursion_pending();
                continue;
            }
            if (!it->is_regular_file() || it->path().extension() != ".py") continue;
            std::string module = moduleName(fs::relative(it->path(), folder));
            if (!module.empty()) modules.push_back(module);
        }
        std::sort(modules.begin(), modules.end());

        py::object importlib = py::module_::import("importlib");
        py::object inspect = py::module_::import("inspect");
        py::object builtins = py::module_::import("builtins");
        py::object baseClass = py::module_::import("crsdk").attr("Component");

        for (const std::string &module : modules) {
            try {
                py::object imported = importlib.attr("import_module")(module);
                for (py::handle member : inspect.attr("getmembers")(imported, inspect.attr("isclass"))) {
                    py::object cls = member.cast<py::tuple>()[1];
                    if (cls.is(baseClass) || !builtins.attr("issubclass")(cls, baseClass).cast<bool>()) continue;
                    //só as classes escritas neste arquivo (uma importada de outro aparece no arquivo dela)
                    if (cls.attr("__module__").cast<std::string>() != module) continue;

                    ComponentInfo info{};
                    info.module = module;
                    info.className = cls.attr("__name__").cast<std::string>();
                    //a docstring da própria classe (o getdoc herdaria a do crsdk.Component)
                    py::object doc = cls.attr("__dict__").attr("get")("__doc__");
                    if (!doc.is_none()) {
                        std::string text = inspect.attr("cleandoc")(doc).cast<std::string>();
                        info.description = text.substr(0, text.find('\n'));
                    }
                    py::object args = cls.attr("args");
                    if (py::isinstance<py::dict>(args)) {
                        for (auto item : args.cast<py::dict>()) {
                            auto value = argFromPython(item.second);
                            if (value) info.args.push_back({py::str(item.first).cast<std::string>(), *value});
                        }
                    }
                    found.push_back(info);
                }
            } catch (const std::exception &e) {
                interpreter->errors.push_back(folder + ": " + module + "\n" + e.what());
            }
        }
        return found;
    }

    //"PuloDuplo" -> "pulo_duplo": sublinhado onde uma maiúscula vem logo depois de uma minúscula
    static std::string snakeCase(const std::string &name) {
        std::string result{};
        for (size_t i = 0; i < name.size(); i++) {
            unsigned char c = static_cast<unsigned char>(name[i]);
            if (std::isupper(c) && i > 0 && std::islower(static_cast<unsigned char>(name[i - 1]))) result += '_';
            result += static_cast<char>(std::tolower(c));
        }
        return result;
    }

    std::string CrsScripting::createComponentFile(const std::string &folder, const std::string &className) {
        //nome de classe Python sem acento (vira nome de arquivo também): letra, depois letras, números ou _
        bool valid = !className.empty() && std::isalpha(static_cast<unsigned char>(className[0]));
        for (char c : className) valid = valid && (std::isalnum(static_cast<unsigned char>(c)) || c == '_');
        if (!valid) throw std::runtime_error("nome inválido: use letras, números e _, começando com letra (ex: PuloDuplo)");

        std::string module = snakeCase(className);
        std::filesystem::path file = std::filesystem::path{folder} / (module + ".py");
        if (std::filesystem::exists(file)) throw std::runtime_error("já existe " + file.generic_string());
        //um components/random.py esconderia o random do Python de todos os scripts do projeto
        if (py::module_::import("keyword").attr("iskeyword")(module).cast<bool>() ||
            !py::module_::import("importlib.util").attr("find_spec")(module).is_none()) {
            throw std::runtime_error("\"" + module + "\" já é um nome usado pelo Python: escolha outro");
        }

        std::filesystem::create_directories(folder);
        std::ofstream out{file};
        if (!out) throw std::runtime_error("não deu pra criar " + file.generic_string());
        out << "import crsdk\n"
            << "\n\n"
            << "class " << className << "(crsdk.Component):\n"
            << "    \"\"\"O que este componente faz (aparece no Add Component do editor).\"\"\"\n"
            << "\n"
            << "    # parâmetros editáveis no painel do editor: bool, int, float ou texto\n"
            << "    args = {\n"
            << "        \"velocidade\": 1.0,\n"
            << "    }\n"
            << "\n"
            << "    def start(self, args):\n"
            << "        \"\"\"Chamado uma vez ao dar Play.\"\"\"\n"
            << "        self.velocidade = args[\"velocidade\"]\n"
            << "\n"
            << "    def update(self):\n"
            << "        \"\"\"Chamado todo frame durante o Play (self.object é o objeto).\"\"\"\n"
            << "        pass\n";
        return module;
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
