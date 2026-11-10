#pragma once

#include "Views/Editor/Widgets/UiHelp.hpp"
#include "Views/Editor/Widgets/UiTaskLayout.hpp"
#include "Views/Editor/ApprovedUi/ApprovedUiTheme.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "imgui.h"
#include "Models/UX/LegacySurfaceRegistry.hpp"

#include <algorithm>
#include <array>
#include <string>

using EditorWorkspace = gm::ux::WorkspaceTarget;

struct EditorWorkspaceDefinition
{
    EditorWorkspace workspace;
    const char *label;
    const char *shortLabel;
    const char *description;
};

inline constexpr std::array<EditorWorkspaceDefinition, 8> kEditorWorkspaces{{
    {EditorWorkspace::Home, "Session", "SE", "Prepare scenes and linked material for the next session."},
    {EditorWorkspace::Story, "Story", "ST", "Arcs, scenes, clues, timeline, and session staging."},
    {EditorWorkspace::Cast, "Cast", "CA", "Party members, NPCs, creatures, and character sheets."},
    {EditorWorkspace::World, "World", "WO", "Maps, travel, dungeons, factions, deities, and shops."},
    {EditorWorkspace::Encounters, "Encounters", "EN", "Encounter preparation, battlefields, and combat readiness."},
    {EditorWorkspace::Compendium, "Compendium", "CO", "Items, spells, random tables, inventories, and campaign audio."},
    {EditorWorkspace::Live, "Live", "LI", "Run scenes, exploration, combat, audio, time, and player display."},
    {EditorWorkspace::Settings, "Settings", "SE", "Project health, save/recovery, accessibility, and diagnostics."},
}};

inline const EditorWorkspaceDefinition &editorWorkspaceDefinition(EditorWorkspace workspace)
{
    for (const EditorWorkspaceDefinition &definition : kEditorWorkspaces)
    {
        if (definition.workspace == workspace)
            return definition;
    }
    return kEditorWorkspaces.front();
}

inline gm::localization::TextId editorWorkspaceLabelId(EditorWorkspace workspace)
{
    using gm::localization::TextId;
    switch (workspace)
    {
    case EditorWorkspace::Story: return TextId::WorkspaceStory;
    case EditorWorkspace::Cast: return TextId::WorkspaceCast;
    case EditorWorkspace::World: return TextId::WorkspaceWorld;
    case EditorWorkspace::Encounters: return TextId::WorkspaceEncounters;
    case EditorWorkspace::Compendium: return TextId::WorkspaceCompendium;
    case EditorWorkspace::Live: return TextId::WorkspaceLive;
    case EditorWorkspace::Settings: return TextId::WorkspaceSettings;
    case EditorWorkspace::Home:
    default: return TextId::WorkspaceHome;
    }
}

inline gm::localization::TextId editorWorkspaceDescriptionId(EditorWorkspace workspace)
{
    using gm::localization::TextId;
    switch (workspace)
    {
    case EditorWorkspace::Story: return TextId::WorkspaceStoryDescription;
    case EditorWorkspace::Cast: return TextId::WorkspaceCastDescription;
    case EditorWorkspace::World: return TextId::WorkspaceWorldDescription;
    case EditorWorkspace::Encounters: return TextId::WorkspaceEncountersDescription;
    case EditorWorkspace::Compendium: return TextId::WorkspaceCompendiumDescription;
    case EditorWorkspace::Live: return TextId::WorkspaceLiveDescription;
    case EditorWorkspace::Settings: return TextId::WorkspaceSettingsDescription;
    case EditorWorkspace::Home:
    default: return TextId::WorkspaceHomeDescription;
    }
}

inline const char *editorWorkspaceLabel(EditorWorkspace workspace)
{
    if (workspace == EditorWorkspace::Home)
        return gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German ? "Sitzung" : "Session";
    return gm::localization::tr(editorWorkspaceLabelId(workspace));
}

inline const char *editorWorkspaceDescription(EditorWorkspace workspace)
{
    return gm::localization::tr(editorWorkspaceDescriptionId(workspace));
}

namespace WorkspaceUi
{
inline constexpr ImVec4 Canvas = gm::approved_ui::Canvas;
inline constexpr ImVec4 Surface = gm::approved_ui::Surface;
inline constexpr ImVec4 SurfaceRaised = gm::approved_ui::SurfaceRaised;
inline constexpr ImVec4 Text = gm::approved_ui::Bone;
inline constexpr ImVec4 Muted = gm::approved_ui::Muted;
inline constexpr ImVec4 Amber = gm::approved_ui::Brass;
inline constexpr ImVec4 Teal = gm::approved_ui::Verdigris;
// Keeps normal-size destructive/status text above 4.5:1 on SurfaceRaised.
inline constexpr ImVec4 Danger = gm::approved_ui::Danger;
inline constexpr float NavigationHorizontalPadding = 10.0f;

inline bool navigationItem(const char *label, const char *shortLabel, bool selected, const char *description)
{
    (void)shortLabel;
    ImGui::PushID(label);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(NavigationHorizontalPadding, 9.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
    if (selected)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(Amber.x, Amber.y, Amber.z, 0.20f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(Amber.x, Amber.y, Amber.z, 0.28f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(Amber.x, Amber.y, Amber.z, 0.34f));
        ImGui::PushStyleColor(ImGuiCol_Text, Text);
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.055f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.09f));
        ImGui::PushStyleColor(ImGuiCol_Text, Muted);
    }

    const bool pressed = TaskUi::button(label, ImVec2(-1.0f, 0.0f));
    const ImVec2 itemMin = ImGui::GetItemRectMin();
    const ImVec2 itemMax = ImGui::GetItemRectMax();
    if (selected)
    {
        // A shape cue keeps the active workspace recognizable without relying
        // on the amber background alone (including for color-impaired users).
        ImGui::GetWindowDrawList()->AddRectFilled(
            itemMin,
            ImVec2(itemMin.x + 3.0f, itemMax.y),
            ImGui::GetColorU32(Amber),
            2.0f);
    }
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(3);
    // Keyboard users receive the same explanation as pointer users.
    showHelpTooltip(description, ImGuiHoveredFlags_DelayNormal);
    ImGui::PopID();
    return pressed;
}

inline void workspaceHeader(const char *title,
                            const char *breadcrumb,
                            const char *description,
                            const char *stateLabel = nullptr,
                            const ImVec4 &stateColor = Teal)
{
    ImGui::PushStyleColor(ImGuiCol_Text, Muted);
    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), uiTextOrFallback(breadcrumb));
    ImGui::PopStyleColor();
    ImGui::Spacing();

    const char *localizedTitle = localizedUiText(uiTextOrFallback(title));
    const char *localizedState = stateLabel && *stateLabel ? localizedUiText(stateLabel) : nullptr;
    ImGui::SetWindowFontScale(1.32f);
    ImGui::PushStyleColor(ImGuiCol_Text, Text);
    const float availableTitleWidth = ImGui::GetContentRegionAvail().x;
    const float titleWidth = ImGui::CalcTextSize(localizedTitle).x;
    const float stateWidth = localizedState ? ImGui::CalcTextSize(localizedState).x : 0.0f;
    const bool stateFitsBesideTitle = localizedState &&
                                      titleWidth + stateWidth + ImGui::GetStyle().ItemSpacing.x * 2.0f <= availableTitleWidth;
    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), localizedTitle);
    ImGui::PopStyleColor();

    if (localizedState)
    {
        if (stateFitsBesideTitle)
            ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, stateColor);
        ImGui::TextUnformatted(localizedState);
        ImGui::PopStyleColor();
    }
    ImGui::SetWindowFontScale(1.0f);

    ImGui::PushStyleColor(ImGuiCol_Text, Muted);
    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), localizedUiText(uiTextOrFallback(description)));
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 8.0f));
}

inline bool actionCard(const char *id,
                       const char *title,
                       const std::string &description,
                       const char *actionLabel,
                       UiButtonRole role = UiButtonRole::Secondary,
                       const ImVec2 &size = ImVec2(0.0f, 132.0f))
{
    const char *localizedTitle = localizedUiText(uiTextOrFallback(title));
    const char *localizedDescription = localizedUiText(description.c_str());
    const ImGuiStyle &style = ImGui::GetStyle();
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float outerWidth = size.x > 0.0f ? size.x : std::max(1.0f, availableWidth + size.x);
    const float textWidth = std::max(80.0f, outerWidth - 28.0f);
    const float titleHeight = ImGui::CalcTextSize(localizedTitle, nullptr, false, textWidth).y;
    const float descriptionHeight = ImGui::CalcTextSize(localizedDescription, nullptr, false, textWidth).y;
    const float contentHeight = 24.0f + titleHeight + descriptionHeight +
                                style.ItemSpacing.y * 2.0f + ImGui::GetFrameHeight();
    ImVec2 effectiveSize = size;
    effectiveSize.y = std::max(size.y, contentHeight);

    ImGui::PushID(id);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, SurfaceRaised);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
    ImGui::BeginChild("##card", effectiveSize, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PushStyleColor(ImGuiCol_Text, Amber);
    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), localizedTitle);
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Text, Muted);
    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), localizedDescription);
    ImGui::PopStyleColor();
    const float buttonHeight = ImGui::GetFrameHeight();
    const float buttonY = ImGui::GetWindowHeight() - ImGui::GetStyle().WindowPadding.y - buttonHeight;
    if (ImGui::GetCursorPosY() < buttonY)
        ImGui::SetCursorPosY(buttonY);
    const bool pressed = drawUiButton(actionLabel, role, ImVec2(-1.0f, 0.0f));
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    ImGui::PopID();
    return pressed;
}

inline void emptyState(const char *title, const char *description)
{
    ImGui::Dummy(ImVec2(0.0f, 18.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, Text);
    ImGui::TextUnformatted(localizedUiText(title));
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Text, Muted);
    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), localizedUiText(description));
    ImGui::PopStyleColor();
}
}
