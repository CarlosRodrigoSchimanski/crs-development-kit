#include "editor/crs_editor_theme.hpp"

#include <cfloat>
#include <cmath>
#include <filesystem>

namespace crsdk::theme {
    //fontes do editor (Poppins, licença OFL em assets/editor/fonts/OFL.txt)
    constexpr const char *REGULAR_FONT = "assets/editor/fonts/Poppins-Regular.ttf";
    constexpr const char *SEMIBOLD_FONT = "assets/editor/fonts/Poppins-SemiBold.ttf";
    constexpr float FONT_SIZE = 16.f;
    constexpr float TITLE_FONT_SIZE = 17.f;

    static ImFont *title = nullptr;

    //cor do CSS (sRGB, como o navegador e a imagem de referência) -> linear.
    //A tela da engine é sRGB (crs_swap_chain): a GPU converte de linear pra sRGB ao gravar, então
    //sem isso todo tom escuro sai bem mais claro e lavado (o #0C0A1D virava um roxo médio)
    static float toLinear(unsigned int channel) {
        float c = static_cast<float>(channel) / 255.f;
        return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
    }

    //cor escrita como no CSS: 0x49A7E3 = #49A7E3
    static ImVec4 rgb(unsigned int hex, float alpha = 1.f) {
        return ImVec4(toLinear((hex >> 16) & 0xFF), toLinear((hex >> 8) & 0xFF), toLinear(hex & 0xFF), alpha);
    }

    //paleta da referência
    constexpr unsigned int BACKGROUND = 0x0C0A1D;  //fundo da janela
    constexpr unsigned int CARD = 0x110E23;        //fundo do card: um tom acima do fundo
    constexpr unsigned int BORDER = 0x1B192B;      //borda fina do card
    constexpr unsigned int FIELD = 0x16132A;       //campo e botão dentro do card
    constexpr unsigned int FIELD_HOVER = 0x221E3D;
    constexpr unsigned int FIELD_ACTIVE = 0x2B2650;
    constexpr unsigned int BLUE = 0x49A7E3;        //destaque
    constexpr unsigned int ROYAL = 0x365BF2;       //destaque forte (clicado, selecionado)
    constexpr unsigned int TEXT = 0xFAFAFA;
    constexpr unsigned int MUTED = 0x8E8BA0;       //texto apagado

    ImVec4 accent() { return rgb(BLUE); }
    ImVec4 mutedText() { return rgb(MUTED); }
    ImFont* titleFont() { return title; }

    static void applyColors(ImGuiStyle &style) {
        ImVec4 *c = style.Colors;
        c[ImGuiCol_Text] = rgb(TEXT);
        c[ImGuiCol_TextDisabled] = rgb(MUTED);
        c[ImGuiCol_WindowBg] = rgb(BACKGROUND);
        c[ImGuiCol_ChildBg] = rgb(CARD);
        c[ImGuiCol_PopupBg] = rgb(CARD);
        c[ImGuiCol_Border] = rgb(BORDER);
        c[ImGuiCol_BorderShadow] = rgb(0, 0.f);
        c[ImGuiCol_FrameBg] = rgb(FIELD);
        c[ImGuiCol_FrameBgHovered] = rgb(FIELD_HOVER);
        c[ImGuiCol_FrameBgActive] = rgb(FIELD_ACTIVE);
        c[ImGuiCol_TitleBg] = rgb(BACKGROUND);
        c[ImGuiCol_TitleBgActive] = rgb(CARD);
        c[ImGuiCol_TitleBgCollapsed] = rgb(BACKGROUND);
        c[ImGuiCol_MenuBarBg] = rgb(BACKGROUND);
        c[ImGuiCol_ScrollbarBg] = rgb(0, 0.f);
        c[ImGuiCol_ScrollbarGrab] = rgb(FIELD_ACTIVE);
        c[ImGuiCol_ScrollbarGrabHovered] = rgb(0x3A3468);
        c[ImGuiCol_ScrollbarGrabActive] = rgb(ROYAL);
        c[ImGuiCol_CheckMark] = rgb(BLUE);
        c[ImGuiCol_SliderGrab] = rgb(BLUE);
        c[ImGuiCol_SliderGrabActive] = rgb(ROYAL);
        c[ImGuiCol_Button] = rgb(FIELD);
        c[ImGuiCol_ButtonHovered] = rgb(FIELD_HOVER);
        c[ImGuiCol_ButtonActive] = rgb(ROYAL);
        //linha do outliner / item de lista: selecionado em azul apagado, como o item ativo da referência
        c[ImGuiCol_Header] = rgb(ROYAL, 0.35f);
        c[ImGuiCol_HeaderHovered] = rgb(FIELD_HOVER);
        c[ImGuiCol_HeaderActive] = rgb(ROYAL, 0.55f);
        c[ImGuiCol_Separator] = rgb(BORDER);
        c[ImGuiCol_SeparatorHovered] = rgb(BLUE, 0.6f);
        c[ImGuiCol_SeparatorActive] = rgb(BLUE);
        c[ImGuiCol_ResizeGrip] = rgb(0, 0.f);
        c[ImGuiCol_ResizeGripHovered] = rgb(BLUE, 0.6f);
        c[ImGuiCol_ResizeGripActive] = rgb(BLUE);
        c[ImGuiCol_Tab] = rgb(FIELD);
        c[ImGuiCol_TabHovered] = rgb(FIELD_HOVER);
        c[ImGuiCol_TabSelected] = rgb(FIELD_ACTIVE);
        c[ImGuiCol_PlotLines] = rgb(BLUE);
        c[ImGuiCol_PlotHistogram] = rgb(BLUE);
        c[ImGuiCol_TextSelectedBg] = rgb(ROYAL, 0.4f);
        c[ImGuiCol_NavHighlight] = rgb(BLUE);
        c[ImGuiCol_ModalWindowDimBg] = rgb(BACKGROUND, 0.6f);
    }

    static void applySizes(ImGuiStyle &style) {
        style.WindowRounding = 0.f;  //o painel lateral é preso na borda da tela: canto reto
        style.ChildRounding = 10.f;  //os cards
        style.FrameRounding = 6.f;
        style.PopupRounding = 8.f;
        style.GrabRounding = 6.f;
        style.ScrollbarRounding = 8.f;
        style.TabRounding = 6.f;

        style.WindowBorderSize = 0.f;
        style.ChildBorderSize = 1.f;
        style.PopupBorderSize = 1.f;
        style.FrameBorderSize = 0.f;

        style.WindowPadding = ImVec2(12.f, 12.f);
        style.FramePadding = ImVec2(8.f, 5.f);
        style.ItemSpacing = ImVec2(8.f, 8.f);
        style.ItemInnerSpacing = ImVec2(6.f, 6.f);
        style.IndentSpacing = 16.f;
        style.ScrollbarSize = 10.f;
        style.SeparatorTextBorderSize = 1.f;
    }

    static void loadFonts() {
        ImGuiIO &io = ImGui::GetIO();
        //sem os arquivos (rodando de outra pasta, por exemplo): fica a fonte padrão do ImGui
        //(o AddFontFromFileTTF aborta no Debug se o arquivo não existir)
        if (!std::filesystem::exists(REGULAR_FONT) || !std::filesystem::exists(SEMIBOLD_FONT)) return;
        //a primeira fonte adicionada é a padrão de todo texto; a faixa padrão já tem os acentos (Latin-1)
        io.Fonts->AddFontFromFileTTF(REGULAR_FONT, FONT_SIZE);
        title = io.Fonts->AddFontFromFileTTF(SEMIBOLD_FONT, TITLE_FONT_SIZE);
    }

    void apply() {
        ImGuiStyle &style = ImGui::GetStyle();
        applyColors(style);
        applySizes(style);
        loadFonts();
    }

    void beginCard(const char *id, float maxHeight) {
        if (maxHeight > 0.f) ImGui::SetNextWindowSizeConstraints(ImVec2(0.f, 0.f), ImVec2(FLT_MAX, maxHeight));
        ImGui::BeginChild(id, ImVec2(0.f, 0.f), ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeY);
    }

    void endCard() {
        ImGui::EndChild();
    }

    void fieldLabel(const std::string &label, float reserveRight) {
        constexpr float LABEL_WIDTH = 84.f;
        float x = ImGui::GetCursorPosX();
        //o texto na mesma altura do texto de dentro do campo
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(mutedText(), "%s", label.c_str());
        ImGui::SameLine(x + LABEL_WIDTH);
        ImGui::SetNextItemWidth(reserveRight > 0.f ? -(reserveRight + ImGui::GetStyle().ItemSpacing.x) : -FLT_MIN);
    }

    bool cardHeader(const std::string &text, const std::string &detail, bool defaultOpen) {
        //a linha inteira é clicável (SpanAvailWidth); AllowOverlap deixa um botão na mesma linha
        //(o Remover do componente) receber o clique em vez do título
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap |
                                   ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if (defaultOpen) flags |= ImGuiTreeNodeFlags_DefaultOpen;

        float startX = ImGui::GetCursorPosX();
        if (title) ImGui::PushFont(title);
        //cada card é uma janela filha própria: o mesmo id em cards diferentes não se mistura
        bool open = ImGui::TreeNodeEx("##cardHeader", flags, "%s", text.c_str());
        float textEnd = startX + ImGui::GetTreeNodeToLabelSpacing() + ImGui::CalcTextSize(text.c_str()).x;
        if (title) ImGui::PopFont();

        if (!detail.empty()) {
            ImGui::SameLine(textEnd + ImGui::GetStyle().ItemSpacing.x);
            ImGui::TextColored(mutedText(), "%s", detail.c_str());
        }
        return open;
    }
}
