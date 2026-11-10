#pragma once

#include "imgui.h"

namespace gm::approved_ui
{
inline constexpr ImVec4 Ink = ImVec4(0.055f, 0.063f, 0.059f, 1.0f);          // #0e100f
inline constexpr ImVec4 Canvas = ImVec4(0.071f, 0.082f, 0.075f, 1.0f);       // #121513
inline constexpr ImVec4 Surface = ImVec4(0.094f, 0.110f, 0.102f, 1.0f);      // #181c1a
inline constexpr ImVec4 SurfaceRaised = ImVec4(0.122f, 0.141f, 0.129f, 1.0f);
inline constexpr ImVec4 Bone = ImVec4(0.933f, 0.914f, 0.863f, 1.0f);         // #eee9dc
inline constexpr ImVec4 Muted = ImVec4(0.608f, 0.647f, 0.616f, 1.0f);
inline constexpr ImVec4 Brass = ImVec4(0.780f, 0.608f, 0.294f, 1.0f);        // #c79b4b
inline constexpr ImVec4 Verdigris = ImVec4(0.329f, 0.667f, 0.612f, 1.0f);    // #54aa9c
inline constexpr ImVec4 Danger = ImVec4(0.851f, 0.416f, 0.376f, 1.0f);       // #d96a60
inline constexpr ImVec4 Rule = ImVec4(0.208f, 0.239f, 0.220f, 1.0f);

inline void applyPalette(bool highContrast)
{
    ImVec4 *colors = ImGui::GetStyle().Colors;
    const ImVec4 text = highContrast ? ImVec4(1.0f, 0.985f, 0.94f, 1.0f) : Bone;
    const ImVec4 muted = highContrast ? ImVec4(0.78f, 0.82f, 0.79f, 1.0f) : Muted;
    const ImVec4 border = highContrast ? ImVec4(0.52f, 0.59f, 0.54f, 1.0f) : Rule;
    const ImVec4 accent = highContrast ? ImVec4(0.96f, 0.77f, 0.38f, 1.0f) : Brass;
    const ImVec4 live = highContrast ? ImVec4(0.43f, 0.88f, 0.79f, 1.0f) : Verdigris;

    colors[ImGuiCol_Text] = text;
    colors[ImGuiCol_TextDisabled] = muted;
    colors[ImGuiCol_WindowBg] = highContrast ? Ink : Canvas;
    colors[ImGuiCol_ChildBg] = Surface;
    colors[ImGuiCol_PopupBg] = Surface;
    colors[ImGuiCol_Border] = border;
    colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.34f);
    colors[ImGuiCol_FrameBg] = Ink;
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.15f, 0.18f, 0.16f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.19f, 0.22f, 0.19f, 1.0f);
    colors[ImGuiCol_TitleBg] = Ink;
    colors[ImGuiCol_TitleBgActive] = Surface;
    colors[ImGuiCol_TitleBgCollapsed] = Ink;
    colors[ImGuiCol_MenuBarBg] = Ink;
    colors[ImGuiCol_ScrollbarBg] = Ink;
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.25f, 0.29f, 0.26f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.35f, 0.40f, 0.36f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.44f, 0.49f, 0.45f, 1.0f);
    colors[ImGuiCol_CheckMark] = live;
    colors[ImGuiCol_SliderGrab] = Verdigris;
    colors[ImGuiCol_SliderGrabActive] = live;
    colors[ImGuiCol_Button] = ImVec4(0.145f, 0.157f, 0.145f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.29f, 0.24f, 0.14f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.43f, 0.32f, 0.15f, 1.0f);
    colors[ImGuiCol_Header] = ImVec4(Brass.x, Brass.y, Brass.z, 0.13f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(Brass.x, Brass.y, Brass.z, 0.24f);
    colors[ImGuiCol_HeaderActive] = ImVec4(Brass.x, Brass.y, Brass.z, 0.33f);
    colors[ImGuiCol_Separator] = border;
    colors[ImGuiCol_SeparatorHovered] = accent;
    colors[ImGuiCol_SeparatorActive] = accent;
    colors[ImGuiCol_ResizeGrip] = ImVec4(Verdigris.x, Verdigris.y, Verdigris.z, 0.42f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(Verdigris.x, Verdigris.y, Verdigris.z, 0.72f);
    colors[ImGuiCol_ResizeGripActive] = live;
    colors[ImGuiCol_Tab] = Canvas;
    colors[ImGuiCol_TabHovered] = ImVec4(Brass.x, Brass.y, Brass.z, 0.22f);
    colors[ImGuiCol_TabActive] = ImVec4(Brass.x, Brass.y, Brass.z, 0.13f);
    colors[ImGuiCol_TabSelectedOverline] = accent;
    colors[ImGuiCol_TabUnfocused] = Ink;
    colors[ImGuiCol_TabUnfocusedActive] = Surface;
    colors[ImGuiCol_DockingPreview] = ImVec4(Verdigris.x, Verdigris.y, Verdigris.z, 0.58f);
    colors[ImGuiCol_DockingEmptyBg] = Ink;
    colors[ImGuiCol_TableHeaderBg] = SurfaceRaised;
    colors[ImGuiCol_TableBorderStrong] = border;
    colors[ImGuiCol_TableBorderLight] = ImVec4(Rule.x, Rule.y, Rule.z, 0.72f);
    colors[ImGuiCol_TableRowBg] = Surface;
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.105f, 0.122f, 0.111f, 1.0f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(Verdigris.x, Verdigris.y, Verdigris.z, 0.38f);
    colors[ImGuiCol_DragDropTarget] = accent;
    colors[ImGuiCol_NavHighlight] = accent;
    colors[ImGuiCol_NavWindowingHighlight] = text;
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.46f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.62f);
    colors[ImGuiCol_PlotLines] = Verdigris;
    colors[ImGuiCol_PlotLinesHovered] = live;
    colors[ImGuiCol_PlotHistogram] = Brass;
    colors[ImGuiCol_PlotHistogramHovered] = accent;
}

inline void applyGeometry(bool compactTables)
{
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowPadding = compactTables ? ImVec2(12.0f, 10.0f) : ImVec2(16.0f, 14.0f);
    style.FramePadding = compactTables ? ImVec2(8.0f, 5.0f) : ImVec2(10.0f, 7.0f);
    style.CellPadding = compactTables ? ImVec2(7.0f, 5.0f) : ImVec2(9.0f, 7.0f);
    style.ItemSpacing = compactTables ? ImVec2(9.0f, 7.0f) : ImVec2(11.0f, 9.0f);
    style.ItemInnerSpacing = ImVec2(7.0f, 5.0f);
    style.ScrollbarSize = compactTables ? 16.0f : 17.0f;
    style.GrabMinSize = 15.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.TabBorderSize = 0.0f;
    style.WindowRounding = 3.0f;
    style.ChildRounding = 3.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 0.0f;
    style.DisabledAlpha = 0.68f;
    style.IndentSpacing = 20.0f;
}
}
