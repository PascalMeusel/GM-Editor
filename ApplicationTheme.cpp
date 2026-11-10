#include "ApplicationInternal.hpp"
#include "Views/Editor/Widgets/Map/MapTypographyFont.hpp"
#include <cstdio>

using namespace gm::application_detail;

namespace gm::application_detail
{
void LoadEditorFont(ImGuiIO &io)
{
    ImFontConfig config;
    config.OversampleH = 3;
    config.OversampleV = 2;
    config.PixelSnapH = true;

    std::vector<std::filesystem::path> regularFontCandidates;
    std::vector<std::filesystem::path> boldFontCandidates;

    // Keep the runtime appearance deterministic: packaged fonts beside the
    // executable always take precedence over machine-specific system fonts.
    const std::filesystem::path executableRoot = ApplicationPaths::executableDirectory();
    if (!executableRoot.empty())
    {
        regularFontCandidates.push_back(executableRoot / "Resources/Fonts/Inter/Inter-Regular.ttf");
        boldFontCandidates.push_back(executableRoot / "Resources/Fonts/Inter/Inter-SemiBold.ttf");
    }

#if defined(_WIN32)
    regularFontCandidates.emplace_back("C:/Windows/Fonts/segoeui.ttf");
    regularFontCandidates.emplace_back("C:/Windows/Fonts/arial.ttf");
    boldFontCandidates.emplace_back("C:/Windows/Fonts/segoeuib.ttf");
    boldFontCandidates.emplace_back("C:/Windows/Fonts/arialbd.ttf");
#else
    regularFontCandidates.emplace_back("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    regularFontCandidates.emplace_back("/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf");
    regularFontCandidates.emplace_back("/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf");
    regularFontCandidates.emplace_back("/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf");
    boldFontCandidates.emplace_back("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf");
    boldFontCandidates.emplace_back("/usr/share/fonts/truetype/liberation2/LiberationSans-Bold.ttf");
    boldFontCandidates.emplace_back("/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf");
    boldFontCandidates.emplace_back("/usr/share/fonts/truetype/ubuntu/Ubuntu-B.ttf");
#endif

    auto loadFirstAvailableFont = [&io, &config](const std::vector<std::filesystem::path> &candidates) -> ImFont * {
        for (const std::filesystem::path &candidate : candidates)
        {
            if (std::filesystem::exists(candidate))
            {
                // ImGui's Windows file wrapper expects UTF-8, not the local
                // code page produced by path::string(). Keep packaged fonts
                // available under accented and non-Latin install paths.
                if (ImFont *font = io.Fonts->AddFontFromFileTTF(candidate.u8string().c_str(),
                                                               18.0f,
                                                               &config,
                                                               kEditorGlyphRanges))
                    return font;
            }
        }
        return nullptr;
    };

    ImFont *regularFont = loadFirstAvailableFont(regularFontCandidates);
    if (regularFont == nullptr)
        io.Fonts->AddFontDefault();
    else
        io.FontDefault = regularFont;

    // Views intentionally use font atlas index 1 for emphasized labels.
    // Preserve that contract even if every external bold fallback is missing.
    if (loadFirstAvailableFont(boldFontCandidates) == nullptr)
        io.Fonts->AddFontDefault();

    const auto atlasFontPath=executableRoot / "Resources/Fonts/SourceSerif4/SourceSerif4-Regular.ttf";
    if(!executableRoot.empty() && std::filesystem::exists(atlasFontPath))
    {
        ImFontConfig atlasConfig=config;
        atlasConfig.PixelSnapH=false;
        std::snprintf(atlasConfig.Name,sizeof(atlasConfig.Name),"%s",kMapAtlasFontName);
        io.Fonts->AddFontFromFileTTF(atlasFontPath.u8string().c_str(),28.f,&atlasConfig,kEditorGlyphRanges);
    }
}

void ApplyEditorPalette(bool highContrast)
{
    ImGuiStyle &style = ImGui::GetStyle();
    ImVec4 *colors = style.Colors;
    colors[ImGuiCol_Text] = highContrast ? ImVec4(0.99f, 0.995f, 1.00f, 1.00f) : ImVec4(0.91f, 0.93f, 0.94f, 1.00f);
    colors[ImGuiCol_TextDisabled] = highContrast ? ImVec4(0.80f, 0.86f, 0.91f, 1.00f) : ImVec4(0.62f, 0.67f, 0.72f, 1.00f);
    colors[ImGuiCol_WindowBg] = highContrast ? ImVec4(0.035f, 0.040f, 0.048f, 1.00f) : ImVec4(0.105f, 0.112f, 0.120f, 1.00f);
    colors[ImGuiCol_ChildBg] = highContrast ? ImVec4(0.052f, 0.060f, 0.072f, 1.00f) : ImVec4(0.135f, 0.145f, 0.154f, 1.00f);
    colors[ImGuiCol_PopupBg] = highContrast ? ImVec4(0.048f, 0.056f, 0.068f, 0.99f) : ImVec4(0.125f, 0.134f, 0.144f, 0.98f);
    colors[ImGuiCol_Border] = highContrast ? ImVec4(0.58f, 0.68f, 0.76f, 1.00f) : ImVec4(0.36f, 0.40f, 0.45f, 1.00f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.02f, 0.025f, 0.030f, highContrast ? 0.55f : 0.35f);
    colors[ImGuiCol_FrameBg] = highContrast ? ImVec4(0.095f, 0.120f, 0.145f, 1.00f) : ImVec4(0.175f, 0.190f, 0.205f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = highContrast ? ImVec4(0.17f, 0.31f, 0.38f, 1.00f) : ImVec4(0.235f, 0.275f, 0.302f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = highContrast ? ImVec4(0.22f, 0.45f, 0.54f, 1.00f) : ImVec4(0.265f, 0.335f, 0.365f, 1.00f);
    colors[ImGuiCol_TitleBg] = highContrast ? ImVec4(0.030f, 0.038f, 0.050f, 1.00f) : ImVec4(0.115f, 0.124f, 0.136f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = highContrast ? ImVec4(0.070f, 0.105f, 0.135f, 1.00f) : ImVec4(0.155f, 0.185f, 0.205f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = colors[ImGuiCol_TitleBg];
    colors[ImGuiCol_MenuBarBg] = highContrast ? ImVec4(0.052f, 0.065f, 0.080f, 1.00f) : ImVec4(0.145f, 0.158f, 0.170f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = highContrast ? ImVec4(0.030f, 0.036f, 0.044f, 1.00f) : ImVec4(0.105f, 0.112f, 0.120f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = highContrast ? ImVec4(0.46f, 0.56f, 0.64f, 1.00f) : ImVec4(0.38f, 0.43f, 0.48f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = highContrast ? ImVec4(0.60f, 0.72f, 0.80f, 1.00f) : ImVec4(0.49f, 0.56f, 0.62f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = highContrast ? ImVec4(0.76f, 0.92f, 0.98f, 1.00f) : ImVec4(0.60f, 0.70f, 0.76f, 1.00f);
    colors[ImGuiCol_CheckMark] = highContrast ? ImVec4(0.78f, 1.00f, 0.94f, 1.00f) : ImVec4(0.48f, 0.82f, 0.76f, 1.00f);
    colors[ImGuiCol_SliderGrab] = highContrast ? ImVec4(0.67f, 0.96f, 0.93f, 1.00f) : ImVec4(0.43f, 0.72f, 0.72f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = highContrast ? ImVec4(0.88f, 1.00f, 0.95f, 1.00f) : ImVec4(0.63f, 0.88f, 0.80f, 1.00f);
    colors[ImGuiCol_Button] = highContrast ? ImVec4(0.13f, 0.26f, 0.32f, 1.00f) : ImVec4(0.205f, 0.275f, 0.310f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = highContrast ? ImVec4(0.22f, 0.48f, 0.56f, 1.00f) : ImVec4(0.275f, 0.395f, 0.435f, 1.00f);
    colors[ImGuiCol_ButtonActive] = highContrast ? ImVec4(0.30f, 0.65f, 0.72f, 1.00f) : ImVec4(0.335f, 0.520f, 0.540f, 1.00f);
    colors[ImGuiCol_Header] = highContrast ? ImVec4(0.14f, 0.32f, 0.39f, 1.00f) : ImVec4(0.210f, 0.300f, 0.335f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = highContrast ? ImVec4(0.25f, 0.55f, 0.62f, 1.00f) : ImVec4(0.295f, 0.430f, 0.465f, 1.00f);
    colors[ImGuiCol_HeaderActive] = highContrast ? ImVec4(0.34f, 0.72f, 0.78f, 1.00f) : ImVec4(0.365f, 0.560f, 0.580f, 1.00f);
    colors[ImGuiCol_Separator] = highContrast ? ImVec4(0.56f, 0.66f, 0.73f, 1.00f) : ImVec4(0.42f, 0.47f, 0.52f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = highContrast ? ImVec4(0.68f, 0.93f, 0.96f, 1.00f) : ImVec4(0.55f, 0.72f, 0.75f, 1.00f);
    colors[ImGuiCol_SeparatorActive] = highContrast ? ImVec4(0.84f, 1.00f, 0.94f, 1.00f) : ImVec4(0.70f, 0.90f, 0.82f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = highContrast ? ImVec4(0.40f, 0.68f, 0.74f, 0.78f) : ImVec4(0.38f, 0.56f, 0.59f, 0.72f);
    colors[ImGuiCol_ResizeGripHovered] = highContrast ? ImVec4(0.58f, 0.88f, 0.93f, 0.92f) : ImVec4(0.49f, 0.72f, 0.75f, 0.90f);
    colors[ImGuiCol_ResizeGripActive] = highContrast ? ImVec4(0.84f, 1.00f, 0.94f, 1.00f) : ImVec4(0.70f, 0.90f, 0.82f, 1.00f);
    colors[ImGuiCol_Tab] = highContrast ? ImVec4(0.070f, 0.090f, 0.110f, 1.00f) : ImVec4(0.155f, 0.170f, 0.185f, 1.00f);
    colors[ImGuiCol_TabHovered] = colors[ImGuiCol_HeaderHovered];
    colors[ImGuiCol_TabActive] = highContrast ? ImVec4(0.16f, 0.34f, 0.40f, 1.00f) : ImVec4(0.235f, 0.325f, 0.355f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = highContrast ? ImVec4(0.050f, 0.062f, 0.078f, 1.00f) : ImVec4(0.125f, 0.138f, 0.150f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = highContrast ? ImVec4(0.08f, 0.14f, 0.17f, 1.00f) : ImVec4(0.175f, 0.215f, 0.235f, 1.00f);
    colors[ImGuiCol_TabSelectedOverline] = highContrast ? ImVec4(0.90f, 1.00f, 0.72f, 1.00f) : ImVec4(0.87f, 0.68f, 0.36f, 1.00f);
    colors[ImGuiCol_DockingPreview] = highContrast ? ImVec4(0.78f, 1.00f, 0.96f, 0.70f) : ImVec4(0.48f, 0.82f, 0.78f, 0.62f);
    colors[ImGuiCol_DockingEmptyBg] = highContrast ? ImVec4(0.030f, 0.035f, 0.045f, 1.00f) : ImVec4(0.090f, 0.097f, 0.105f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = highContrast ? ImVec4(0.10f, 0.16f, 0.20f, 1.00f) : ImVec4(0.190f, 0.220f, 0.240f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = highContrast ? ImVec4(0.62f, 0.72f, 0.80f, 1.00f) : ImVec4(0.44f, 0.49f, 0.54f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = highContrast ? ImVec4(0.44f, 0.54f, 0.62f, 1.00f) : ImVec4(0.32f, 0.36f, 0.40f, 1.00f);
    colors[ImGuiCol_TableRowBg] = highContrast ? ImVec4(0.052f, 0.060f, 0.072f, 1.00f) : ImVec4(0.135f, 0.145f, 0.154f, 1.00f);
    colors[ImGuiCol_TableRowBgAlt] = highContrast ? ImVec4(0.070f, 0.084f, 0.102f, 1.00f) : ImVec4(0.162f, 0.174f, 0.185f, 1.00f);
    colors[ImGuiCol_TextSelectedBg] = highContrast ? ImVec4(0.32f, 0.58f, 0.68f, 0.55f) : ImVec4(0.28f, 0.48f, 0.54f, 0.45f);
    colors[ImGuiCol_DragDropTarget] = highContrast ? ImVec4(1.00f, 0.92f, 0.46f, 0.95f) : ImVec4(0.95f, 0.72f, 0.32f, 0.90f);
    colors[ImGuiCol_NavHighlight] = highContrast ? ImVec4(0.94f, 1.00f, 0.70f, 1.00f) : ImVec4(0.86f, 0.70f, 0.38f, 0.98f);
    colors[ImGuiCol_NavWindowingHighlight] = highContrast ? ImVec4(1.00f, 0.96f, 0.62f, 0.95f) : ImVec4(0.82f, 0.90f, 0.86f, 0.78f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.030f, 0.035f, 0.040f, highContrast ? 0.55f : 0.42f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.030f, 0.035f, 0.040f, highContrast ? 0.70f : 0.58f);
    colors[ImGuiCol_PlotLines] = highContrast ? ImVec4(0.78f, 0.92f, 0.96f, 1.00f) : ImVec4(0.55f, 0.76f, 0.78f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = highContrast ? ImVec4(1.00f, 0.84f, 0.48f, 1.00f) : ImVec4(0.98f, 0.70f, 0.38f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = highContrast ? ImVec4(0.98f, 0.78f, 0.42f, 1.00f) : ImVec4(0.82f, 0.62f, 0.34f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = highContrast ? ImVec4(1.00f, 0.92f, 0.56f, 1.00f) : ImVec4(0.95f, 0.76f, 0.42f, 1.00f);

    if (!highContrast)
    {
        // GM Editor: warm charcoal canvas, graphite surfaces, parchment text,
        // amber focus, teal live/success state, and restrained destructive red.
        colors[ImGuiCol_Text] = ImVec4(0.93f, 0.91f, 0.86f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.58f, 0.54f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.090f, 0.086f, 0.080f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.115f, 0.110f, 0.102f, 1.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.125f, 0.119f, 0.110f, 0.99f);
        colors[ImGuiCol_Border] = ImVec4(0.50f, 0.46f, 0.38f, 1.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.155f, 0.147f, 0.135f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.215f, 0.198f, 0.168f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.265f, 0.228f, 0.165f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.100f, 0.095f, 0.087f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.145f, 0.135f, 0.115f, 1.00f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.115f, 0.108f, 0.098f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.215f, 0.178f, 0.112f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.42f, 0.31f, 0.14f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.62f, 0.43f, 0.16f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.245f, 0.205f, 0.140f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.42f, 0.315f, 0.155f, 1.00f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.58f, 0.405f, 0.16f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.38f, 0.76f, 0.69f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.34f, 0.66f, 0.61f, 1.00f);
        colors[ImGuiCol_Tab] = ImVec4(0.130f, 0.123f, 0.113f, 1.00f);
        colors[ImGuiCol_TabActive] = ImVec4(0.255f, 0.208f, 0.132f, 1.00f);
        colors[ImGuiCol_TabSelectedOverline] = ImVec4(0.88f, 0.62f, 0.25f, 1.00f);
        colors[ImGuiCol_NavHighlight] = ImVec4(0.91f, 0.66f, 0.28f, 1.00f);
        colors[ImGuiCol_TextSelectedBg] = ImVec4(0.55f, 0.39f, 0.17f, 0.55f);
    }

    // The approved campaign-editor identity is the final palette authority.
    // Keeping this call here also preserves the existing high-contrast branch.
    gm::approved_ui::applyPalette(highContrast);
}

void ApplyEditorSizing(bool compactTables)
{
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowPadding = compactTables ? ImVec2(13.0f, 11.0f) : ImVec2(16.0f, 14.0f);
    style.FramePadding = compactTables ? ImVec2(9.0f, 5.0f) : ImVec2(11.0f, 7.0f);
    style.CellPadding = compactTables ? ImVec2(7.0f, 5.0f) : ImVec2(10.0f, 8.0f);
    style.ItemSpacing = compactTables ? ImVec2(9.0f, 7.0f) : ImVec2(11.0f, 10.0f);
    style.ItemInnerSpacing = compactTables ? ImVec2(7.0f, 5.0f) : ImVec2(9.0f, 7.0f);
    style.ScrollbarSize = compactTables ? 17.0f : 18.0f;
    style.GrabMinSize = compactTables ? 15.0f : 17.0f;
    gm::approved_ui::applyGeometry(compactTables);
}

void ApplyEditorTheme(ImGuiIO &io)
{
    io.FontGlobalScale = 1.0f;

    ImGuiStyle &style = ImGui::GetStyle();
    ImGui::StyleColorsDark();
    style.FontScaleMain = 1.0f;
    ApplyEditorSizing(false);
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.TabBorderSize = 0.0f;
    style.WindowRounding = 8.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.DisabledAlpha = 0.68f;
    style.IndentSpacing = 20.0f;

    ApplyEditorPalette(false);
    gm::approved_ui::applyGeometry(false);
}
} // namespace gm::application_detail
