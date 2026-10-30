#pragma once

#include <imgui.h>

#include <string>

//visual do editor: dashboard escuro com cards (cores tiradas da imagem de referência do usuário)
namespace crsdk::theme {
    //chamar uma vez, logo depois do ImGui::CreateContext (antes do primeiro frame): cores, cantos e fontes
    void apply();

    //Poppins SemiBold: título de card e nome do objeto (nullptr = sem o arquivo: fica a fonte normal)
    ImFont* titleFont();

    //destaque (gráfico, botão principal, item ativo) e o texto apagado (detalhe, dica)
    ImVec4 accent();
    ImVec4 mutedText();

    //um card: bloco com fundo próprio, borda fina e cantos arredondados (como um container no React).
    //A altura acompanha o conteúdo; maxHeight > 0 limita (o resto rola dentro do card)
    void beginCard(const char *id, float maxHeight = 0.f);
    void endCard();
    //linha de título do card com a setinha, como as seções do VS Code: clicar abre/fecha o card.
    //Ao lado do título, um detalhe apagado (ex: o arquivo do componente). true = aberto (mostrar o
    //conteúdo). O ImGui guarda o estado pelo id do card; defaultOpen vale só na primeira vez
    bool cardHeader(const std::string &title, const std::string &detail = "", bool defaultOpen = false);
    //o nome do campo à esquerda, apagado, e o próximo campo ocupando o resto da linha
    //(chamar logo antes do campo, que usa um id escondido: "##posicao"). reserveRight = espaço
    //deixado à direita do campo, pra um botão na mesma linha
    void fieldLabel(const std::string &label, float reserveRight = 0.f);
}
