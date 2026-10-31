//o módulo "crsdk" que os scripts Python importam: a ponte (pybind11) entre o Python e a engine.
//Mudou algo aqui? Atualizar junto a referência assets/scripts/crsdk/*.pyi (o VS Code lê ela).
//Os objetos do C++ são entregues por referência (o Python não copia nem apaga): um GameObject guardado
//num script vale enquanto a cena dele existir.
#include "engine/scripting/crs_python_api.hpp"

#include "engine/core/crs_input.hpp"
#include "engine/core/crs_window.hpp"
#include "engine/scene/crs_scene.hpp"

#include <pybind11/embed.h>
#include <pybind11/stl.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace py = pybind11;

namespace crsdk {
    ScriptContext& scriptContext() {
        static ScriptContext context{};
        return context;
    }

    //a cena ligada ao Python (pai, filhos e posição no mundo dependem dela)
    static CrsScene& currentScene() {
        if (scriptContext().scene == nullptr) {
            throw std::runtime_error("crsdk: nenhuma cena ligada ao Python");
        }
        return *scriptContext().scene;
    }

    //objetos em ordem de id: a mesma ordem toda vez (o unordered_map não garante nenhuma)
    template <typename Predicate>
    static std::vector<CrsGameObject*> objectsWhere(CrsScene &scene, Predicate predicate) {
        std::vector<CrsGameObject*> result{};
        for (auto &kv : scene.getGameObjects()) {
            if (predicate(kv.second)) result.push_back(&kv.second);
        }
        std::sort(result.begin(), result.end(), [](auto *a, auto *b) { return a->getId() < b->getId(); });
        return result;
    }

    //número curto no repr (o float tem só ~7 dígitos de verdade: 3.6, não 3.5999999046325684)
    static std::string vecText(float value) {
        return py::str("{:.6g}").format(value).cast<std::string>();
    }

    //componente opcional como ponteiro: o Python recebe o componente de verdade, ou None se o objeto não tem
    template <typename Component>
    static Component* optionalRef(std::optional<Component> &component) {
        return component ? &*component : nullptr;
    }

    //a rotação do objeto no mundo, sem a escala (só os eixos, cada um com tamanho 1)
    static glm::mat3 worldRotation(const CrsGameObject &obj) {
        glm::mat3 m{currentScene().worldMatrix(obj)};
        for (int i = 0; i < 3; i++) {
            float length = glm::length(m[i]);
            if (length > 0.f) m[i] /= length;
        }
        return m;
    }

    //o que o raycast do Python devolve: o objeto, a distância e o ponto do impacto
    struct ScriptRayHit {
        CrsGameObject *object;
        float distance;
        glm::vec3 point;
    };

    static void bindMath(py::module_ &m) {
        py::class_<glm::vec2>(m, "Vec2")
            .def(py::init([](float x, float y) { return glm::vec2{x, y}; }), py::arg("x") = 0.f, py::arg("y") = 0.f)
            .def_property("x", [](const glm::vec2 &v) { return v.x; }, [](glm::vec2 &v, float value) { v.x = value; })
            .def_property("y", [](const glm::vec2 &v) { return v.y; }, [](glm::vec2 &v, float value) { v.y = value; })
            .def("__add__", [](const glm::vec2 &a, const glm::vec2 &b) { return a + b; })
            .def("__sub__", [](const glm::vec2 &a, const glm::vec2 &b) { return a - b; })
            .def("__mul__", [](const glm::vec2 &a, float s) { return a * s; })
            .def("__rmul__", [](const glm::vec2 &a, float s) { return a * s; })
            .def("__truediv__", [](const glm::vec2 &a, float s) { return a / s; })
            .def("__neg__", [](const glm::vec2 &a) { return -a; })
            .def("__eq__", [](const glm::vec2 &a, const glm::vec2 &b) { return a == b; })
            .def("__repr__", [](const glm::vec2 &v) { return "Vec2(" + vecText(v.x) + ", " + vecText(v.y) + ")"; })
            .def("copy", [](const glm::vec2 &v) { return v; })
            .def("length", [](const glm::vec2 &v) { return glm::length(v); });

        py::class_<glm::vec3>(m, "Vec3")
            .def(py::init([](float x, float y, float z) { return glm::vec3{x, y, z}; }),
                py::arg("x") = 0.f, py::arg("y") = 0.f, py::arg("z") = 0.f)
            .def_property("x", [](const glm::vec3 &v) { return v.x; }, [](glm::vec3 &v, float value) { v.x = value; })
            .def_property("y", [](const glm::vec3 &v) { return v.y; }, [](glm::vec3 &v, float value) { v.y = value; })
            .def_property("z", [](const glm::vec3 &v) { return v.z; }, [](glm::vec3 &v, float value) { v.z = value; })
            .def("__add__", [](const glm::vec3 &a, const glm::vec3 &b) { return a + b; })
            .def("__sub__", [](const glm::vec3 &a, const glm::vec3 &b) { return a - b; })
            .def("__mul__", [](const glm::vec3 &a, float s) { return a * s; })
            .def("__rmul__", [](const glm::vec3 &a, float s) { return a * s; })
            .def("__truediv__", [](const glm::vec3 &a, float s) { return a / s; })
            .def("__neg__", [](const glm::vec3 &a) { return -a; })
            .def("__eq__", [](const glm::vec3 &a, const glm::vec3 &b) { return a == b; })
            .def("__repr__", [](const glm::vec3 &v) {
                return "Vec3(" + vecText(v.x) + ", " + vecText(v.y) + ", " + vecText(v.z) + ")";
            })
            .def("copy", [](const glm::vec3 &v) { return v; })
            .def("length", [](const glm::vec3 &v) { return glm::length(v); })
            //vetor zero continua zero (em vez de virar NaN)
            .def("normalized", [](const glm::vec3 &v) {
                float length = glm::length(v);
                return length > 0.f ? v / length : glm::vec3{0.f};
            })
            .def("dot", [](const glm::vec3 &a, const glm::vec3 &b) { return glm::dot(a, b); })
            .def("cross", [](const glm::vec3 &a, const glm::vec3 &b) { return glm::cross(a, b); });
    }

    static void bindScene(py::module_ &m) {
        py::class_<TransformComponent>(m, "Transform")
            .def_readwrite("translation", &TransformComponent::translation)
            .def_readwrite("rotation", &TransformComponent::rotation)
            .def_readwrite("scale", &TransformComponent::scale);

        py::class_<AnimatorComponent>(m, "Animator")
            .def("play", &AnimatorComponent::play, py::arg("name"), py::arg("fade") = 0.2f, py::arg("loop") = true)
            .def("isBlending", &AnimatorComponent::isBlending)
            .def("blendWeight", &AnimatorComponent::blendWeight)
            .def("clipTime", &AnimatorComponent::clipTime)
            .def("progress", &AnimatorComponent::progress)
            .def("isFinished", &AnimatorComponent::isFinished)
            .def("frame", &AnimatorComponent::frame, py::arg("fps") = 30.f)
            .def_readonly("clip", &AnimatorComponent::clip)
            .def_readonly("animations", &AnimatorComponent::animations)
            .def_readonly("duration", &AnimatorComponent::duration)
            .def_readwrite("time", &AnimatorComponent::time)
            .def_readwrite("speed", &AnimatorComponent::speed)
            .def_readwrite("loop", &AnimatorComponent::loop);

        py::class_<CharacterInput>(m, "CharacterInput")
            .def_readwrite("moveDirection", &CharacterInput::moveDirection)
            .def_readwrite("jump", &CharacterInput::jump)
            .def_readwrite("run", &CharacterInput::run);

        py::class_<CharacterComponent>(m, "Character")
            .def_readwrite("moveSpeed", &CharacterComponent::moveSpeed)
            .def_readwrite("runSpeed", &CharacterComponent::runSpeed)
            .def_readwrite("jumpSpeed", &CharacterComponent::jumpSpeed)
            .def_readwrite("input", &CharacterComponent::input);

        py::enum_<ColliderShape>(m, "Shape")
            .value("Box", ColliderShape::Box)
            .value("Sphere", ColliderShape::Sphere)
            .value("Capsule", ColliderShape::Capsule);

        py::class_<RigidBodyComponent>(m, "RigidBody")
            .def_property("isStatic",
                [](const RigidBodyComponent &body) { return body.type == BodyType::Static; },
                [](RigidBodyComponent &body, bool isStatic) { body.type = isStatic ? BodyType::Static : BodyType::Dynamic; })
            .def_readwrite("velocity", &RigidBodyComponent::velocity)
            .def_readwrite("shape", &RigidBodyComponent::shape)
            .def_readwrite("halfExtents", &RigidBodyComponent::halfExtents)
            .def_readwrite("radius", &RigidBodyComponent::radius)
            .def_readwrite("height", &RigidBodyComponent::height)
            .def_readwrite("center", &RigidBodyComponent::center)
            .def_readwrite("mass", &RigidBodyComponent::mass)
            //quem decide é a física (encostou em algo embaixo): o script só lê
            .def_readonly("onGround", &RigidBodyComponent::onGround);

        py::class_<CameraComponent>(m, "Camera")
            .def_readwrite("fovY", &CameraComponent::fovY)
            .def_readwrite("nearPlane", &CameraComponent::nearPlane)
            .def_readwrite("farPlane", &CameraComponent::farPlane);

        py::class_<PointLightComponent>(m, "PointLight")
            .def_readwrite("intensity", &PointLightComponent::lightIntensity);

        py::class_<CrsGameObject>(m, "GameObject")
            .def_property_readonly("id", &CrsGameObject::getId)
            .def_readwrite("name", &CrsGameObject::name)
            .def_readwrite("transform", &CrsGameObject::transform)
            .def_readwrite("color", &CrsGameObject::color)
            .def_readwrite("parentId", &CrsGameObject::parentId)
            .def_property("parent",
                [](const CrsGameObject &obj) -> CrsGameObject* {
                    return obj.parentId ? currentScene().findGameObjectById(*obj.parentId) : nullptr;
                },
                [](CrsGameObject &obj, CrsGameObject *parent) {
                    if (parent == nullptr) {
                        obj.parentId.reset();
                        return;
                    }
                    //o pai não pode ser o próprio objeto nem um descendente dele (a matriz do mundo nunca acabaria)
                    for (CrsGameObject *p = parent; p != nullptr;
                         p = p->parentId ? currentScene().findGameObjectById(*p->parentId) : nullptr) {
                        if (p == &obj) throw py::value_error("crsdk: o pai não pode ser o próprio objeto nem um filho dele");
                    }
                    obj.parentId = parent->getId();
                },
                py::return_value_policy::reference)
            .def_property_readonly("children", [](const CrsGameObject &obj) {
                return objectsWhere(currentScene(), [&](const CrsGameObject &o) { return o.parentId == obj.getId(); });
            }, py::return_value_policy::reference)
            .def_property_readonly("worldPosition", [](const CrsGameObject &obj) {
                return glm::vec3{currentScene().worldMatrix(obj)[3]};
            })
            //uma direção do objeto (ex: Vec3(0, 0, -1), pra onde a câmera olha) vista no mundo, com tamanho 1
            .def("worldDirection", [](const CrsGameObject &obj, glm::vec3 local) {
                glm::vec3 direction = worldRotation(obj) * local;
                float length = glm::length(direction);
                return length > 0.f ? direction / length : direction;
            }, py::arg("local"))
            .def("applyRotation", [](CrsGameObject &obj, glm::vec3 radians, bool local) {
                //no mundo, um filho precisa da rotação do pai pra saber onde ficam os eixos do mundo
                glm::mat3 parentRotation{1.f};
                if (!local && obj.parentId) {
                    if (CrsGameObject *parent = currentScene().findGameObjectById(*obj.parentId)) parentRotation = worldRotation(*parent);
                }
                obj.transform.applyRotation(radians, local, parentRotation);
            }, py::arg("radians"), py::arg("local") = true)
            .def_property_readonly("animator", [](CrsGameObject &obj) { return optionalRef(obj.animator); },
                py::return_value_policy::reference_internal)
            .def_property_readonly("character", [](CrsGameObject &obj) { return optionalRef(obj.character); },
                py::return_value_policy::reference_internal)
            .def_property_readonly("rigidBody", [](CrsGameObject &obj) { return optionalRef(obj.rigidBody); },
                py::return_value_policy::reference_internal)
            .def_property_readonly("camera", [](CrsGameObject &obj) { return optionalRef(obj.camera); },
                py::return_value_policy::reference_internal)
            .def_property_readonly("pointLight", [](CrsGameObject &obj) { return optionalRef(obj.pointLight); },
                py::return_value_policy::reference_internal)
            .def("__eq__", [](const CrsGameObject &a, const CrsGameObject &b) { return a.getId() == b.getId(); })
            .def("__hash__", [](const CrsGameObject &obj) { return obj.getId(); })
            .def("__repr__", [](const CrsGameObject &obj) {
                return "<GameObject " + std::to_string(obj.getId()) + " '" + obj.name + "'>";
            });

        py::class_<ScriptRayHit>(m, "RayHit")
            .def_property_readonly("object", [](const ScriptRayHit &hit) { return hit.object; }, py::return_value_policy::reference)
            .def_readonly("distance", &ScriptRayHit::distance)
            .def_readonly("point", &ScriptRayHit::point);

        py::class_<CrsScene>(m, "Scene")
            .def_property_readonly("objects", [](CrsScene &scene) {
                return objectsWhere(scene, [](const CrsGameObject &) { return true; });
            }, py::return_value_policy::reference)
            //o primeiro (menor id) com esse nome
            .def("find", [](CrsScene &scene, const std::string &name) -> CrsGameObject* {
                auto found = objectsWhere(scene, [&](const CrsGameObject &o) { return o.name == name; });
                return found.empty() ? nullptr : found.front();
            }, py::arg("name"), py::return_value_policy::reference)
            .def("findById", &CrsScene::findGameObjectById, py::arg("id"), py::return_value_policy::reference)
            .def("createObject", [](CrsScene &scene, const std::string &name) {
                CrsGameObject &obj = scene.createGameObject();
                obj.name = name;
                return &obj;
            }, py::arg("name") = "", py::return_value_policy::reference)
            .def("raycast", [](CrsScene &scene, glm::vec3 origin, glm::vec3 direction, float maxDistance,
                               const CrsGameObject *ignore) -> std::optional<ScriptRayHit> {
                float length = glm::length(direction);
                if (length == 0.f) throw py::value_error("crsdk: a direção do raio não pode ser zero");
                //direção de tamanho 1: a distância sai em unidades do mundo
                Ray ray{origin, direction / length};
                std::optional<CrsGameObject::id_t> ignoreId{};
                if (ignore != nullptr) ignoreId = ignore->getId();

                auto hit = scene.raycast(ray, maxDistance, ignoreId);
                if (!hit) return std::nullopt;
                return ScriptRayHit{scene.findGameObjectById(hit->id), hit->distance, ray.origin + ray.direction * hit->distance};
            }, py::arg("origin"), py::arg("direction"), py::arg("maxDistance"), py::arg("ignore") = py::none())
            .def("terrainHeight", &CrsScene::terrainHeightAt, py::arg("x"), py::arg("z"))
            .def_property("activeCamera",
                [](CrsScene &scene) -> CrsGameObject* {
                    auto id = scene.getActiveCamera();
                    return id ? scene.findGameObjectById(*id) : nullptr;
                },
                [](CrsScene &scene, const CrsGameObject &camera) { scene.setActiveCamera(camera.getId()); },
                py::return_value_policy::reference);
    }

    static void bindInput(py::module_ &m) {
        py::class_<CrsInput>(m, "Input")
            .def("isDown", &CrsInput::isDown, py::arg("key"))
            .def("wasPressed", &CrsInput::wasPressed, py::arg("key"))
            .def("wasReleased", &CrsInput::wasReleased, py::arg("key"))
            .def("isMouseDown", &CrsInput::isMouseDown, py::arg("button"))
            .def("wasMousePressed", &CrsInput::wasMousePressed, py::arg("button"))
            .def_property_readonly("mousePosition", &CrsInput::getMousePosition)
            .def_property_readonly("mouseDelta", &CrsInput::getMouseDelta);

        py::class_<CrsWindow>(m, "Window")
            .def_property_readonly("width", [](const CrsWindow &window) { return window.getExtent().width; })
            .def_property_readonly("height", [](const CrsWindow &window) { return window.getExtent().height; });

        //nomes das teclas (os números do GLFW por baixo)
        py::module_ keys = m.def_submodule("keys", "teclas do teclado, pra usar no crsdk.input");
        for (int i = 0; i < 26; i++) {
            keys.attr(std::string(1, char('A' + i)).c_str()) = GLFW_KEY_A + i;
        }
        for (int i = 0; i < 10; i++) {
            keys.attr(("NUM" + std::to_string(i)).c_str()) = GLFW_KEY_0 + i;
            keys.attr(("KP" + std::to_string(i)).c_str()) = GLFW_KEY_KP_0 + i;
        }
        for (int i = 0; i < 12; i++) {
            keys.attr(("F" + std::to_string(i + 1)).c_str()) = GLFW_KEY_F1 + i;
        }
        const std::pair<const char*, int> namedKeys[] = {
            {"SPACE", GLFW_KEY_SPACE}, {"ENTER", GLFW_KEY_ENTER}, {"ESCAPE", GLFW_KEY_ESCAPE},
            {"TAB", GLFW_KEY_TAB}, {"BACKSPACE", GLFW_KEY_BACKSPACE}, {"DELETE", GLFW_KEY_DELETE},
            {"INSERT", GLFW_KEY_INSERT}, {"HOME", GLFW_KEY_HOME}, {"END", GLFW_KEY_END},
            {"PAGE_UP", GLFW_KEY_PAGE_UP}, {"PAGE_DOWN", GLFW_KEY_PAGE_DOWN},
            {"UP", GLFW_KEY_UP}, {"DOWN", GLFW_KEY_DOWN}, {"LEFT", GLFW_KEY_LEFT}, {"RIGHT", GLFW_KEY_RIGHT},
            {"LEFT_SHIFT", GLFW_KEY_LEFT_SHIFT}, {"RIGHT_SHIFT", GLFW_KEY_RIGHT_SHIFT},
            {"LEFT_CONTROL", GLFW_KEY_LEFT_CONTROL}, {"RIGHT_CONTROL", GLFW_KEY_RIGHT_CONTROL},
            {"LEFT_ALT", GLFW_KEY_LEFT_ALT}, {"RIGHT_ALT", GLFW_KEY_RIGHT_ALT},
        };
        for (auto &[name, key] : namedKeys) keys.attr(name) = key;

        py::module_ mouse = m.def_submodule("mouse", "botões do mouse, pra usar no crsdk.input");
        mouse.attr("LEFT") = GLFW_MOUSE_BUTTON_LEFT;
        mouse.attr("RIGHT") = GLFW_MOUSE_BUTTON_RIGHT;
        mouse.attr("MIDDLE") = GLFW_MOUSE_BUTTON_MIDDLE;
    }

    PYBIND11_EMBEDDED_MODULE(crsdk, m) {
        m.doc() = "a API da CRS Development Kit para scripts Python";
        bindMath(m);
        bindScene(m);
        bindInput(m);
        
        //a classe base dos componentes, escrita em Python mesmo: o script herda dela como de qualquer classe
        py::exec(R"(
class Component:
    """Base dos componentes: herde dela, preencha args e escreva start(args) e update()."""
    args = {}

    def __init__(self, obj):
        self.object = obj

    def start(self, args):
        pass

    def update(self):
        pass
)", m.attr("__dict__"));


        //crsdk.scene / crsdk.input / crsdk.window: lidos na hora (a cena muda ao entrar e sair do Play)
        m.def("__getattr__", [](const std::string &name) -> py::object {
            auto &context = scriptContext();
            auto take = [&](auto *pointer) -> py::object {
                if (pointer == nullptr) throw std::runtime_error("crsdk." + name + ": não está ligado ao Python agora");
                return py::cast(pointer, py::return_value_policy::reference);
            };
            if (name == "scene") return take(context.scene);
            if (name == "input") return take(context.input);
            if (name == "window") return take(context.window);
            if (name == "deltaTime") return py::float_(context.deltaTime);
            throw py::attribute_error("module 'crsdk' has no attribute '" + name + "'");
        });
    }
}
