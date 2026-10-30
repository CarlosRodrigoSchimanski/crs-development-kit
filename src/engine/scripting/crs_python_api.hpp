#pragma once

//uso interno do scripting: o que o módulo crsdk do Python está enxergando agora

namespace crsdk {
    class CrsScene;
    class CrsInput;
    class CrsWindow;

    struct ScriptContext {
        CrsScene *scene = nullptr;
        const CrsInput *input = nullptr;
        const CrsWindow *window = nullptr;
    };

    //um só pro programa inteiro (o Python também é um só)
    ScriptContext& scriptContext();
}
