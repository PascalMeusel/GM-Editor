#pragma once

#include "Models/Campaign/Campaign.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Views/Editor/ApprovedUi/ApprovedUiTheme.hpp"
#include "imgui.h"

#include <string>

namespace gm::approved_ui
{
inline const char *toolkitText(const char *english, const char *german)
{
    return gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
               ? german
               : english;
}

inline void toolkitMetric(const char *label, const std::string &value, const ImVec4 &accent)
{
    ImGui::TableNextColumn();
    ImGui::PushStyleColor(ImGuiCol_Text, Muted);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, accent);
    ImGui::TextUnformatted(value.c_str());
    ImGui::PopStyleColor();
}

inline void renderToolkitHeader(const Campaign &campaign, const std::string &statusMessage)
{
    const bool wide = ImGui::GetContentRegionAvail().x >= 680.0f;
    const int columns = wide ? 4 : 2;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, Surface);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
    ImGui::BeginChild("ApprovedToolkitHeader", ImVec2(0.0f, 0.0f),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysAutoResize,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PushStyleColor(ImGuiCol_Text, Brass);
    ImGui::TextUnformatted(toolkitText("LIVE TABLE CONSOLE", "LIVE-TISCHKONSOLE"));
    ImGui::PopStyleColor();

    if (ImGui::BeginTable("ToolkitSummary", columns,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
    {
        toolkitMetric(toolkitText("MACROS", "MAKROS"), std::to_string(campaign.diceMacros.size()), Brass);
        toolkitMetric(toolkitText("OBJECTIVES", "ZIELE"), std::to_string(campaign.storyObjectives.size()), Verdigris);
        toolkitMetric(toolkitText("CLUES", "HINWEISE"), std::to_string(campaign.storyClues.size()), Verdigris);
        const auto &safety = campaign.safetyProfile;
        toolkitMetric(toolkitText("SAFETY", "SICHERHEIT"),
                      safety.pauseActive ? toolkitText("PAUSED", "PAUSE")
                                         : (safety.enabled ? toolkitText("READY", "BEREIT")
                                                           : toolkitText("OFF", "AUS")),
                      safety.pauseActive ? Danger : (safety.enabled ? Verdigris : Muted));
        ImGui::EndTable();
    }

    if (!statusMessage.empty())
    {
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text, Verdigris);
        ImGui::TextWrapped("%s", statusMessage.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

inline void toolkitSection(const char *title, const char *description)
{
    ImGui::PushStyleColor(ImGuiCol_Text, Brass);
    ImGui::SeparatorText(title);
    ImGui::PopStyleColor();
    if (description && description[0])
    {
        ImGui::PushStyleColor(ImGuiCol_Text, Muted);
        ImGui::TextWrapped("%s", description);
        ImGui::PopStyleColor();
    }
}

inline void publicSection(const char *title)
{
    ImGui::PushStyleColor(ImGuiCol_Text, Verdigris);
    ImGui::SeparatorText(title);
    ImGui::PopStyleColor();
}
}
