#include "engine/scripting/crs_scripting.hpp"
#include "engine/gameplay/character_controller_system.hpp"
#include "engine/core/crs_input.hpp"
#include "engine/scene/crs_scene.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace crsdk;

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

//o Python é um só pro programa inteiro, como no jogo: desligar e religar a cada teste não é seguro
//(o pybind11 avisa, e aqui chegou a corromper a memória depois de várias voltas).
//Cada teste usa o mesmo e, ao terminar, desliga o que ligou (cena, input, componentes, erros) e
//esquece os módulos que importou, como o Play faz (dois testes podem ter um "comum" diferente cada)
struct PythonTest {
    CrsScripting &scripting = shared();

    ~PythonTest() {
        scripting.stopComponents();
        scripting.setScene(nullptr);
        scripting.setInput(nullptr);
        scripting.setWindow(nullptr);
        scripting.takeErrors();
        scripting.forgetProjectModules();
    }

    static CrsScripting& shared() {
        static CrsScripting instance;
        return instance;
    }
};

TEST_CASE_FIXTURE(PythonTest, "Python: o script roda de verdade") {
    std::string script = tempPath("ge_teste_script.py");
    std::string result = tempPath("ge_teste_resultado.txt");
    //o script escreve um arquivo: se ele existir com o texto certo, o Python rodou
    writeText(script, "with open(r'" + result + "', 'w') as f:\n    f.write(str(6 * 7))\n");

    scripting.runFile(script);

    CHECK(readText(result) == "42");
    std::filesystem::remove(script);
    std::filesystem::remove(result);
}

TEST_CASE_FIXTURE(PythonTest, "Python: erro no script vira exceção com a mensagem do Python") {
    std::string script = tempPath("ge_teste_erro.py");
    writeText(script, "x = 1 / 0\n");

    CHECK_THROWS_WITH_AS(scripting.runFile(script), doctest::Contains("ZeroDivisionError"), std::runtime_error);
    std::filesystem::remove(script);
}

TEST_CASE_FIXTURE(PythonTest, "Python: script que não existe é erro") {
    CHECK_THROWS(scripting.runFile(tempPath("ge_nao_existe.py")));
}

//API crsdk: os asserts ficam no Python (assert falhando = exceção = teste falha com a mensagem)

TEST_CASE_FIXTURE(PythonTest, "API Python: crsdk.scene acha objetos e mexe no transform de verdade") {
    CrsScene scene;
    auto &player = scene.createGameObject();
    player.name = "Player";
    player.transform.translation = {1.f, 0.f, 0.f};
    auto &mesh = scene.createGameObject();
    mesh.name = "PlayerMesh";
    mesh.parentId = player.getId();
    mesh.transform.translation = {0.f, 2.f, 0.f};
    scripting.setScene(&scene);

    scripting.runString(
        "import crsdk\n"
        "player = crsdk.scene.find('Player')\n"
        "assert player.name == 'Player'\n"
        "assert crsdk.scene.find('nao existe') is None\n"
        "assert [o.name for o in crsdk.scene.objects] == ['Player', 'PlayerMesh']\n"
        "mesh = player.children[0]\n"
        "assert mesh.parent == player\n"
        "assert mesh.worldPosition == crsdk.Vec3(1, 2, 0)\n"
        "player.transform.translation.y += 3\n"
        "player.transform.rotation = crsdk.Vec3(0, 1.5, 0)\n");

    CHECK(player.transform.translation.y == doctest::Approx(3.f));
    CHECK(player.transform.rotation.y == doctest::Approx(1.5f));
}

TEST_CASE_FIXTURE(PythonTest, "API Python: Vec3 faz as contas de vetor") {
    CHECK_NOTHROW(scripting.runString(
        "from crsdk import Vec3\n"
        "a = Vec3(1, 2, 3)\n"
        "assert a + Vec3(1, 1, 1) == Vec3(2, 3, 4)\n"
        "assert a - a == Vec3()\n"
        "assert 2 * a == a * 2 == Vec3(2, 4, 6)\n"
        "assert -a == Vec3(-1, -2, -3)\n"
        "assert Vec3(3, 0, 4).length() == 5\n"
        "assert Vec3(0, 0, 9).normalized() == Vec3(0, 0, 1)\n"
        "assert Vec3().normalized() == Vec3()\n"
        "assert Vec3(1, 0, 0).cross(Vec3(0, 1, 0)) == Vec3(0, 0, 1)\n"
        "assert a.dot(Vec3(1, 1, 1)) == 6\n"
        "b = a.copy(); b.x = 10\n"
        "assert a.x == 1\n"));
}

TEST_CASE_FIXTURE(PythonTest, "API Python: criar objeto e trocar o pai (sem deixar fazer ciclo)") {
    CrsScene scene;
    scripting.setScene(&scene);

    scripting.runString(
        "import crsdk\n"
        "a = crsdk.scene.createObject('A')\n"
        "b = crsdk.scene.createObject('B')\n"
        "b.parent = a\n"
        "assert b.parentId == a.id\n"
        "try:\n"
        "    a.parent = b\n"
        "    raise AssertionError('deixou fazer ciclo')\n"
        "except ValueError:\n"
        "    pass\n"
        "b.parent = None\n"
        "assert b.parentId is None\n");

    CHECK(scene.getGameObjects().size() == 2);
}

TEST_CASE_FIXTURE(PythonTest, "API Python: raycast acha a caixa de colisão e respeita o ignore") {
    CrsScene scene;
    auto &wall = scene.createGameObject();
    wall.name = "Parede";
    wall.transform.translation = {0.f, 0.f, 5.f};
    wall.rigidBody.emplace();
    wall.rigidBody->halfExtents = {1.f, 1.f, 1.f};
    scripting.setScene(&scene);

    CHECK_NOTHROW(scripting.runString(
        "import crsdk\n"
        "from crsdk import Vec3\n"
        "hit = crsdk.scene.raycast(Vec3(), Vec3(0, 0, 10), 100)\n"
        "assert hit.object.name == 'Parede'\n"
        "assert abs(hit.distance - 4) < 1e-5\n"
        "assert abs(hit.point.z - 4) < 1e-5\n"
        "assert crsdk.scene.raycast(Vec3(), Vec3(0, 0, 1), 3) is None\n"
        "assert crsdk.scene.raycast(Vec3(), Vec3(0, 0, 1), 100, ignore=hit.object) is None\n"));
}

TEST_CASE_FIXTURE(PythonTest, "API Python: animator.play troca a animação no C++") {
    CrsScene scene;
    auto &mesh = scene.createGameObject();
    mesh.name = "Mesh";
    mesh.animator.emplace();
    mesh.animator->clip = "idle";
    scene.createGameObject().name = "SemAnimacao";
    scripting.setScene(&scene);

    scripting.runString(
        "import crsdk\n"
        "assert crsdk.scene.find('SemAnimacao').animator is None\n"
        "anim = crsdk.scene.find('Mesh').animator\n"
        "anim.play('walk', fade=0.5)\n"
        "assert anim.isBlending()\n");

    CHECK(mesh.animator->clip == "walk");
    CHECK(mesh.animator->previousClip == "idle");
    CHECK(mesh.animator->fadeDuration == doctest::Approx(0.5f));
}

TEST_CASE_FIXTURE(PythonTest, "API Python: input e nomes das teclas") {
    CrsInput input;
    scripting.setInput(&input);

    CHECK_NOTHROW(scripting.runString(
        "import crsdk\n"
        "assert crsdk.keys.A == 65 and crsdk.keys.SPACE == 32 and crsdk.keys.NUM0 == 48\n"
        "assert crsdk.mouse.LEFT == 0\n"
        "assert not crsdk.input.isDown(crsdk.keys.W)\n"
        "assert crsdk.input.mouseDelta == crsdk.Vec2(0, 0)\n"));
}

TEST_CASE_FIXTURE(PythonTest, "API Python: sem cena ligada o crsdk.scene dá erro claro") {
    CHECK_THROWS_WITH_AS(scripting.runString("import crsdk\ncrsdk.scene\n"),
        doctest::Contains("crsdk.scene"), std::runtime_error);
}

TEST_CASE_FIXTURE(PythonTest, "Python: import de outro arquivo da pasta, e a edição vale depois do forget") {
    std::string folder = tempPath("crsdk_teste_imports");
    std::filesystem::create_directories(folder);
    writeText(folder + "/ajudante_teste.py", "def valor():\n    return 1\n");
    scripting.addImportFolder(folder);

    CHECK_NOTHROW(scripting.runString("import ajudante_teste\nassert ajudante_teste.valor() == 1\n"));

    //o arquivo muda: sem o forget o Python continua com o antigo na memória (o problema do Blender)
    writeText(folder + "/ajudante_teste.py", "def valor():\n    return 2\n");
    CHECK_NOTHROW(scripting.runString("import ajudante_teste\nassert ajudante_teste.valor() == 1\n"));

    scripting.forgetProjectModules();
    CHECK_NOTHROW(scripting.runString("import ajudante_teste\nassert ajudante_teste.valor() == 2\n"));

    std::filesystem::remove_all(folder);
}

//componentes: uma pasta temporária faz o papel de assets/components
static std::string componentFolder(const std::string &name) {
    std::string folder = tempPath(name);
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder);
    return folder;
}

TEST_CASE_FIXTURE(PythonTest, "Componente: start(args) uma vez, update() todo frame, mexendo no objeto de verdade") {
    std::string folder = componentFolder("crsdk_teste_comp_basico");
    writeText(folder + "/andar_teste.py",
        "import crsdk\n"
        "class Andar(crsdk.Component):\n"
        "    args = {'velocidade': 2.0}\n"
        "    def start(self, args):\n"
        "        self.velocidade = args['velocidade']\n"
        "        self.object.name = 'iniciado'\n"
        "    def update(self):\n"
        "        self.object.transform.translation.x += self.velocidade * crsdk.deltaTime\n");
    scripting.addImportFolder(folder);

    CrsScene scene;
    auto &obj = scene.createGameObject();
    obj.scripts.push_back({"andar_teste", "Andar"});
    scripting.setScene(&scene);

    scripting.startComponents(scene);
    CHECK(obj.name == "iniciado");
    CHECK(obj.transform.translation.x == doctest::Approx(0.f));

    scripting.updateComponents(0.5f);
    scripting.updateComponents(0.5f);
    CHECK(obj.transform.translation.x == doctest::Approx(2.f));  //2 por segundo, durante 1 segundo
    CHECK(scripting.takeErrors().empty());

    //parou o Play: update não roda mais
    scripting.stopComponents();
    scripting.updateComponents(0.5f);
    CHECK(obj.transform.translation.x == doctest::Approx(2.f));
    std::filesystem::remove_all(folder);
}

TEST_CASE_FIXTURE(PythonTest, "Componente: importa função de outro arquivo da pasta (o que não funcionava no UPBGE)") {
    std::string folder = componentFolder("crsdk_teste_comp_import");
    std::filesystem::create_directories(folder + "/comum");
    writeText(folder + "/comum/__init__.py", "");
    writeText(folder + "/comum/matematica_teste.py", "def dobro(x):\n    return 2 * x\n");
    writeText(folder + "/usa_import_teste.py",
        "import crsdk\n"
        "from comum.matematica_teste import dobro\n"
        "class UsaImport(crsdk.Component):\n"
        "    def start(self, args):\n"
        "        self.object.transform.scale.y = dobro(21)\n");
    scripting.addImportFolder(folder);

    CrsScene scene;
    auto &obj = scene.createGameObject();
    obj.scripts.push_back({"usa_import_teste", "UsaImport"});
    scripting.setScene(&scene);
    scripting.startComponents(scene);

    CHECK(scripting.takeErrors().empty());
    CHECK(obj.transform.scale.y == doctest::Approx(42.f));
    std::filesystem::remove_all(folder);
}

TEST_CASE_FIXTURE(PythonTest, "Componente: erro no update tira só aquele componente, os outros continuam") {
    std::string folder = componentFolder("crsdk_teste_comp_erro");
    writeText(folder + "/erro_teste.py",
        "import crsdk\n"
        "class Quebra(crsdk.Component):\n"
        "    def update(self):\n"
        "        self.object.transform.translation.y += 1\n"
        "        raise ValueError('quebrou de propósito')\n"
        "class Conta(crsdk.Component):\n"
        "    def update(self):\n"
        "        self.object.transform.translation.x += 1\n");
    scripting.addImportFolder(folder);

    CrsScene scene;
    auto &obj = scene.createGameObject();
    obj.name = "Alvo";
    obj.scripts.push_back({"erro_teste", "Quebra"});
    obj.scripts.push_back({"erro_teste", "Conta"});
    scripting.setScene(&scene);
    scripting.startComponents(scene);

    scripting.updateComponents(0.016f);
    auto errors = scripting.takeErrors();
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].find("Alvo: erro_teste.Quebra") != std::string::npos);
    CHECK(errors[0].find("quebrou de propósito") != std::string::npos);

    scripting.updateComponents(0.016f);
    CHECK(scripting.takeErrors().empty());  //o erro aparece uma vez só
    CHECK(obj.transform.translation.y == doctest::Approx(1.f));  //o Quebra rodou só no primeiro frame
    CHECK(obj.transform.translation.x == doctest::Approx(2.f));  //o Conta seguiu rodando
    std::filesystem::remove_all(folder);
}

TEST_CASE_FIXTURE(PythonTest, "Componente: arquivo, classe ou herança errados viram erro (sem derrubar a engine)") {
    std::string folder = componentFolder("crsdk_teste_comp_invalido");
    writeText(folder + "/invalido_teste.py",
        "class NaoHerda:\n"
        "    def update(self):\n"
        "        pass\n");
    scripting.addImportFolder(folder);

    CrsScene scene;
    auto &obj = scene.createGameObject();
    obj.scripts.push_back({"arquivo_que_nao_existe", "X"});
    obj.scripts.push_back({"invalido_teste", "ClasseQueNaoExiste"});
    obj.scripts.push_back({"invalido_teste", "NaoHerda"});
    scripting.setScene(&scene);
    scripting.startComponents(scene);

    auto errors = scripting.takeErrors();
    REQUIRE(errors.size() == 3);
    CHECK(errors[0].find("ModuleNotFoundError") != std::string::npos);
    CHECK(errors[1].find("ClasseQueNaoExiste") != std::string::npos);
    CHECK(errors[2].find("crsdk.Component") != std::string::npos);
    CHECK_NOTHROW(scripting.updateComponents(0.016f));
    std::filesystem::remove_all(folder);
}

TEST_CASE_FIXTURE(PythonTest, "Componente: cada objeto recebe a própria cópia dos args") {
    std::string folder = componentFolder("crsdk_teste_comp_args");
    writeText(folder + "/args_teste.py",
        "import crsdk\n"
        "class MudaArgs(crsdk.Component):\n"
        "    args = {'contador': 0}\n"
        "    def start(self, args):\n"
        "        args['contador'] += 1\n"
        "        self.object.transform.translation.x = args['contador']\n");
    scripting.addImportFolder(folder);

    CrsScene scene;
    auto &a = scene.createGameObject();
    auto &b = scene.createGameObject();
    a.scripts.push_back({"args_teste", "MudaArgs"});
    b.scripts.push_back({"args_teste", "MudaArgs"});
    scripting.setScene(&scene);
    scripting.startComponents(scene);

    //se os dois mexessem no mesmo dicionário, o segundo veria 2
    CHECK(a.transform.translation.x == doctest::Approx(1.f));
    CHECK(b.transform.translation.x == doctest::Approx(1.f));
    std::filesystem::remove_all(folder);
}

TEST_CASE_FIXTURE(PythonTest, "Componente: o arg mudado no objeto chega no start; arg que a classe não tem mais é ignorado") {
    std::string folder = componentFolder("crsdk_teste_comp_override");
    writeText(folder + "/override_teste.py",
        "import crsdk\n"
        "class Config(crsdk.Component):\n"
        "    args = {'velocidade': 1.0, 'pulos': 1, 'ativo': True, 'nome': 'padrao'}\n"
        "    def start(self, args):\n"
        "        assert 'apagado' not in args\n"
        "        assert isinstance(args['pulos'], int) and isinstance(args['ativo'], bool)\n"
        "        t = self.object.transform.translation\n"
        "        t.x = args['velocidade']\n"
        "        t.y = args['pulos']\n"
        "        t.z = 1 if args['ativo'] else 0\n"
        "        self.object.name = args['nome']\n");
    scripting.addImportFolder(folder);

    CrsScene scene;
    auto &mudado = scene.createGameObject();
    ScriptComponent script{"override_teste", "Config"};
    script.args["velocidade"] = 3.5f;
    script.args["pulos"] = 2;
    script.args["ativo"] = false;
    script.args["nome"] = std::string{"mudado"};
    script.args["apagado"] = 7;  //o .py não tem mais esse arg
    mudado.scripts.push_back(script);
    auto &padrao = scene.createGameObject();
    padrao.scripts.push_back({"override_teste", "Config"});
    scripting.setScene(&scene);
    scripting.startComponents(scene);

    CHECK(scripting.takeErrors().empty());
    CHECK(mudado.transform.translation.x == doctest::Approx(3.5f));
    CHECK(mudado.transform.translation.y == doctest::Approx(2.f));
    CHECK(mudado.transform.translation.z == doctest::Approx(0.f));
    CHECK(mudado.name == "mudado");
    //o outro objeto continua com os padrões da classe
    CHECK(padrao.transform.translation.x == doctest::Approx(1.f));
    CHECK(padrao.transform.translation.z == doctest::Approx(1.f));
    CHECK(padrao.name == "padrao");
    std::filesystem::remove_all(folder);
}

TEST_CASE_FIXTURE(PythonTest, "Catálogo: acha as classes da pasta e subpastas, com descrição e args") {
    std::string folder = componentFolder("crsdk_teste_catalogo");
    std::filesystem::create_directories(folder + "/comum");
    writeText(folder + "/comum/__init__.py", "");
    writeText(folder + "/comum/base_teste.py",
        "import crsdk\n"
        "class Base(crsdk.Component):\n"
        "    pass\n");
    writeText(folder + "/jogador_teste.py",
        "import crsdk\n"
        "from comum.base_teste import Base\n"
        "class NaoComponente:\n"
        "    pass\n"
        "class Pulo(crsdk.Component):\n"
        "    '''Pula com espaço.\n"
        "\n"
        "    Detalhes que não aparecem no editor.'''\n"
        "    args = {'altura': 2.0, 'pulos': 2, 'duplo': True, 'som': 'pulo.wav', 'lista': [1, 2]}\n"
        "class Correr(Base):\n"
        "    pass\n");
    scripting.addImportFolder(folder);

    auto found = scripting.findComponents(folder);
    CHECK(scripting.takeErrors().empty());
    //Base (da subpasta), e do jogador: Correr e Pulo; o Base importado lá não repete, NaoComponente e crsdk.Component não entram
    REQUIRE(found.size() == 3);
    CHECK(found[0].module == "comum.base_teste");
    CHECK(found[0].className == "Base");
    CHECK(found[0].description.empty());  //sem docstring própria: não herda a do crsdk.Component
    CHECK(found[1].className == "Correr");
    CHECK(found[2].className == "Pulo");
    CHECK(found[2].module == "jogador_teste");
    CHECK(found[2].description == "Pula com espaço.");

    //a lista não é um tipo que o painel mostra: fica de fora; os outros na ordem do dicionário
    const auto &args = found[2].args;
    REQUIRE(args.size() == 4);
    CHECK(args[0].name == "altura");
    CHECK(std::get<float>(args[0].defaultValue) == doctest::Approx(2.f));
    CHECK(args[1].name == "pulos");
    CHECK(std::get<int>(args[1].defaultValue) == 2);
    CHECK(args[2].name == "duplo");
    CHECK(std::get<bool>(args[2].defaultValue) == true);
    CHECK(args[3].name == "som");
    CHECK(std::get<std::string>(args[3].defaultValue) == "pulo.wav");
    std::filesystem::remove_all(folder);
}

TEST_CASE_FIXTURE(PythonTest, "Catálogo: arquivo com erro vira mensagem e os outros continuam aparecendo; edição vale na hora") {
    std::string folder = componentFolder("crsdk_teste_catalogo_erro");
    writeText(folder + "/quebrado_teste.py", "import crsdk\nclass X(crsdk.Component)\n");  //falta o ':'
    writeText(folder + "/bom_teste.py", "import crsdk\nclass Bom(crsdk.Component):\n    pass\n");
    scripting.addImportFolder(folder);

    auto found = scripting.findComponents(folder);
    REQUIRE(found.size() == 1);
    CHECK(found[0].className == "Bom");
    auto errors = scripting.takeErrors();
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].find("quebrado_teste") != std::string::npos);
    CHECK(errors[0].find("SyntaxError") != std::string::npos);

    //arrumou o arquivo: a próxima leitura já acha a classe (sem fechar a engine)
    writeText(folder + "/quebrado_teste.py", "import crsdk\nclass Arrumado(crsdk.Component):\n    pass\n");
    found = scripting.findComponents(folder);
    CHECK(scripting.takeErrors().empty());
    CHECK(found.size() == 2);
    std::filesystem::remove_all(folder);
}

TEST_CASE_FIXTURE(PythonTest, "Generate Component: cria o .py com o modelo, que já é um componente que funciona") {
    std::string folder = componentFolder("crsdk_teste_generate");
    scripting.addImportFolder(folder);

    std::string module = scripting.createComponentFile(folder, "PuloDuploTeste");
    CHECK(module == "pulo_duplo_teste");
    CHECK(std::filesystem::exists(folder + "/pulo_duplo_teste.py"));

    auto found = scripting.findComponents(folder);
    CHECK(scripting.takeErrors().empty());
    REQUIRE(found.size() == 1);
    CHECK(found[0].className == "PuloDuploTeste");
    REQUIRE(found[0].args.size() == 1);
    CHECK(found[0].args[0].name == "velocidade");

    //e roda no Play sem erro (start e update do modelo)
    CrsScene scene;
    scene.createGameObject().scripts.push_back({module, "PuloDuploTeste"});
    scripting.setScene(&scene);
    scripting.startComponents(scene);
    scripting.updateComponents(0.016f);
    CHECK(scripting.takeErrors().empty());

    //o mesmo nome de novo: não sobrescreve o que a pessoa já escreveu
    CHECK_THROWS_WITH_AS(scripting.createComponentFile(folder, "PuloDuploTeste"), doctest::Contains("existe"), std::runtime_error);
    std::filesystem::remove_all(folder);
}

TEST_CASE_FIXTURE(PythonTest, "Generate Component: nome inválido ou que esconderia um módulo do Python é recusado") {
    std::string folder = componentFolder("crsdk_teste_generate_nomes");

    CHECK_THROWS_WITH_AS(scripting.createComponentFile(folder, ""), doctest::Contains("inválido"), std::runtime_error);
    CHECK_THROWS_WITH_AS(scripting.createComponentFile(folder, "2Pulos"), doctest::Contains("inválido"), std::runtime_error);
    CHECK_THROWS_WITH_AS(scripting.createComponentFile(folder, "Pulo Duplo"), doctest::Contains("inválido"), std::runtime_error);
    CHECK_THROWS_WITH_AS(scripting.createComponentFile(folder, "Random"), doctest::Contains("Python"), std::runtime_error);
    CHECK_THROWS_WITH_AS(scripting.createComponentFile(folder, "Class"), doctest::Contains("Python"), std::runtime_error);
    CHECK(std::filesystem::is_empty(folder));  //nenhum arquivo foi criado
    std::filesystem::remove_all(folder);
}

TEST_CASE_FIXTURE(PythonTest, "Componente: o tempo do update de cada componente fica medido") {
    std::string folder = componentFolder("crsdk_teste_tempo");
    writeText(folder + "/lento_teste.py",
        "import time\n"
        "import crsdk\n"
        "class Lento(crsdk.Component):\n"
        "    def update(self):\n"
        "        time.sleep(0.002)\n");
    scripting.addImportFolder(folder);

    CrsScene scene;
    auto &obj = scene.createGameObject();
    obj.name = "Pesado";
    obj.scripts.push_back({"lento_teste", "Lento"});
    scripting.setScene(&scene);
    scripting.startComponents(scene);
    scripting.updateComponents(0.016f);

    auto timings = scripting.componentTimings();
    REQUIRE(timings.size() == 1);
    CHECK(timings[0].label == "Pesado: lento_teste.Lento");
    CHECK(timings[0].milliseconds >= 1.5f);  //dormiu 2 ms

    scripting.stopComponents();
    CHECK(scripting.componentTimings().empty());
    std::filesystem::remove_all(folder);
}

TEST_CASE_FIXTURE(PythonTest, "API Python: o componente preenche a intenção do personagem e o controlador do C++ executa") {
    std::string folder = componentFolder("crsdk_teste_comp_personagem");
    writeText(folder + "/ia_teste.py",
        "import crsdk\n"
        "from crsdk import Vec3\n"
        "class AndaPraFrente(crsdk.Component):\n"
        "    def update(self):\n"
        "        entrada = self.object.character.input\n"
        "        entrada.moveDirection = Vec3(0, 0, 1)\n"
        "        entrada.run = True\n");
    scripting.addImportFolder(folder);

    CrsScene scene;
    auto &npc = scene.createGameObject();
    npc.rigidBody.emplace();
    npc.character.emplace();
    npc.scripts.push_back({"ia_teste", "AndaPraFrente"});
    scripting.setScene(&scene);
    scripting.startComponents(scene);

    //a ordem do jogo: componentes Python, depois o controlador
    scripting.updateComponents(0.016f);
    CharacterControllerSystem{}.update(scene);

    CHECK(scripting.takeErrors().empty());
    CHECK(npc.rigidBody->velocity.z == doctest::Approx(npc.character->runSpeed));
    std::filesystem::remove_all(folder);
}

TEST_CASE_FIXTURE(PythonTest, "API Python: rigidBody, camera, pointLight e character, ou None quando o objeto não tem") {
    CrsScene scene;
    auto &box = scene.createGameObject();
    box.name = "Caixa";
    box.rigidBody.emplace();
    box.rigidBody->onGround = true;
    auto &cam = scene.createGameObject();
    cam.name = "Cam";
    cam.camera.emplace();
    auto &lamp = scene.createGameObject();
    lamp.name = "Luz";
    lamp.pointLight.emplace();
    scripting.setScene(&scene);

    scripting.runString(
        "import crsdk\n"
        "from crsdk import Vec3\n"
        "caixa = crsdk.scene.find('Caixa')\n"
        "assert caixa.camera is None and caixa.pointLight is None and caixa.character is None\n"
        "corpo = caixa.rigidBody\n"
        "assert corpo.onGround\n"
        "corpo.isStatic = True\n"
        "corpo.halfExtents = Vec3(2, 0.5, 2)\n"
        "corpo.velocity.y = 3\n"
        "try:\n"
        "    corpo.onGround = False\n"
        "    raise AssertionError('onGround deveria ser só leitura')\n"
        "except AttributeError:\n"
        "    pass\n"
        "crsdk.scene.find('Cam').camera.fovY = 1.0\n"
        "crsdk.scene.find('Luz').pointLight.intensity = 4.5\n");

    CHECK(box.rigidBody->type == BodyType::Static);
    CHECK(box.rigidBody->halfExtents.x == doctest::Approx(2.f));
    CHECK(box.rigidBody->velocity.y == doctest::Approx(3.f));
    CHECK(cam.camera->fovY == doctest::Approx(1.f));
    CHECK(lamp.pointLight->lightIntensity == doctest::Approx(4.5f));
}

TEST_CASE_FIXTURE(PythonTest, "API Python: worldDirection e applyRotation (local e no mundo) num filho") {
    CrsScene scene;
    auto &pivot = scene.createGameObject();
    pivot.name = "Pivo";
    pivot.transform.rotation.y = glm::radians(90.f);
    pivot.transform.scale = {3.f, 3.f, 3.f};  //a escala do pai não entra na direção
    auto &cam = scene.createGameObject();
    cam.name = "Cam";
    cam.parentId = pivot.getId();
    scripting.setScene(&scene);

    CHECK_NOTHROW(scripting.runString(
        "import crsdk, math\n"
        "from crsdk import Vec3\n"
        "def perto(a, b):\n"
        "    return (a - b).length() < 1e-4\n"
        "cam = crsdk.scene.find('Cam')\n"
        "# o pai virado 90 graus em Y: a frente da câmera (-Z) aponta pro -X do mundo\n"
        "assert perto(cam.worldDirection(Vec3(0, 0, -1)), Vec3(-1, 0, 0))\n"
        "# girar 90 graus em volta do Y do mundo leva a frente de -X pra +Z\n"
        "cam.applyRotation(Vec3(0, math.radians(90), 0), local=False)\n"
        "assert perto(cam.worldDirection(Vec3(0, 0, -1)), Vec3(0, 0, 1))\n"
        "# local: inclina em volta do próprio X; a direita dele (X) não muda\n"
        "direita = cam.worldDirection(Vec3(1, 0, 0))\n"
        "cam.applyRotation(Vec3(math.radians(30), 0, 0))\n"
        "assert perto(cam.worldDirection(Vec3(1, 0, 0)), direita)\n"));
}
