#pragma once

#include "Models/Campaign/Campaign.hpp"
#include "Models/Combat/EncounterMapEligibility.hpp"
#include "Views/Editor/WorkspaceUi.hpp"

#include <algorithm>
#include <string>

namespace gm::approved_ui
{
inline int storySceneCount(const Campaign &campaign)
{
    int count = 0;
    for (const StoryThreadModel &thread : campaign.storyThreads)
        count += static_cast<int>(thread.beats.size());
    return count;
}

inline int encounterReadyMapCount(const Campaign &campaign)
{
    return static_cast<int>(std::count_if(campaign.maps.begin(), campaign.maps.end(), isEncounterScaleMap));
}

inline void contextMetric(const char *label, int value)
{
    const char *localizedLabel = localizedUiText(label);
    const std::string count = std::to_string(value);
    const float countWidth = ImGui::CalcTextSize(count.c_str()).x;
    const float labelWidth = ImGui::CalcTextSize(localizedLabel).x;
    const float availableWidth = ImGui::GetContentRegionAvail().x;

    ImGui::PushStyleColor(ImGuiCol_Text, WorkspaceUi::Muted);
    if (labelWidth + ImGui::GetStyle().ItemSpacing.x + countWidth <= availableWidth)
        ImGui::TextUnformatted(localizedLabel);
    else
        ImGui::TextWrapped("%s", localizedLabel);
    ImGui::PopStyleColor();

    if (labelWidth + ImGui::GetStyle().ItemSpacing.x + countWidth <= availableWidth)
        ImGui::SameLine();
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowWidth() -
                                 ImGui::GetStyle().WindowPadding.x - countWidth));
    ImGui::TextUnformatted(count.c_str());
}

inline void renderWorkspaceContextRail(EditorWorkspace workspace,
                                       const Campaign &campaign,
                                       const std::string &selectedName,
                                       const std::string &selectedType)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Surface);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 13.0f));
    ImGui::BeginChild("ApprovedWorkspaceContextRail", ImVec2(0.0f, -1.0f), ImGuiChildFlags_Borders);

    const char *heading = "Workspace summary";
    if (workspace == EditorWorkspace::Story)
        heading = "Story readiness";
    else if (workspace == EditorWorkspace::Cast)
        heading = "At the table";
    else if (workspace == EditorWorkspace::World)
        heading = "Atlas context";
    else if (workspace == EditorWorkspace::Encounters)
        heading = "Encounter readiness";
    else if (workspace == EditorWorkspace::Compendium)
        heading = "Library usage";

    ImGui::PushStyleColor(ImGuiCol_Text, Brass);
    // The context rail deliberately stays narrow.  Some localized headings
    // are wider than the English source, so let the heading consume another
    // line instead of clipping into the rail boundary.
    ImGui::TextWrapped("%s", localizedUiText(heading));
    ImGui::PopStyleColor();
    ImGui::Separator();

    if (!selectedName.empty())
    {
        ImGui::TextWrapped("%s", selectedName.c_str());
        ImGui::PushStyleColor(ImGuiCol_Text, Muted);
        ImGui::TextWrapped("%s", localizedUiText(selectedType.c_str()));
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::Separator();
    }

    if (workspace == EditorWorkspace::Story)
    {
        contextMetric("Story arcs", static_cast<int>(campaign.storyThreads.size()));
        contextMetric("Scenes", storySceneCount(campaign));
        contextMetric("Sessions", static_cast<int>(campaign.sessions.size()));
        ImGui::SeparatorText(localizedUiText("Next step"));
        ImGui::TextWrapped("%s", localizedUiText("Open a scene, finish its table notes, then stage it for Live."));
    }
    else if (workspace == EditorWorkspace::Cast)
    {
        contextMetric("Player characters", static_cast<int>(campaign.characters.size()));
        contextMetric("NPCs", static_cast<int>(campaign.npcs.size()));
        contextMetric("Creatures", static_cast<int>(campaign.customMonsters.size()));
        ImGui::SeparatorText(localizedUiText("Quick reference"));
        ImGui::TextWrapped("%s", localizedUiText("The selected dossier stays available while you move between campaign material."));
    }
    else if (workspace == EditorWorkspace::World)
    {
        contextMetric("Maps", static_cast<int>(campaign.maps.size()));
        contextMetric("Places", static_cast<int>(campaign.nations.size() + campaign.worldPlaces.size()));
        contextMetric("Dungeons", static_cast<int>(campaign.dungeons.size()));
        contextMetric("Shops", static_cast<int>(campaign.shops.size()));
        ImGui::SeparatorText(localizedUiText("Map scale"));
        ImGui::TextWrapped("%s", localizedUiText("World and region maps organize the atlas; site maps support places and encounters."));
    }
    else if (workspace == EditorWorkspace::Encounters)
    {
        contextMetric("Prepared encounters", static_cast<int>(campaign.encounters.size()));
        contextMetric("Eligible maps", encounterReadyMapCount(campaign));
        ImGui::SeparatorText(localizedUiText("Battlefield maps"));
        ImGui::TextWrapped("%s", localizedUiText("Choose a dungeon, encounter, or city map. World maps remain in the atlas."));
    }
    else if (workspace == EditorWorkspace::Compendium)
    {
        contextMetric("Items", static_cast<int>(campaign.customItems.size()));
        contextMetric("Spells", static_cast<int>(campaign.customSpells.size()));
        contextMetric("Audio", static_cast<int>(campaign.audioAssets.size()));
        ImGui::SeparatorText(localizedUiText("Campaign context"));
        ImGui::TextWrapped("%s", localizedUiText("Open an entry to edit rules-facing details without losing your library selection."));
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}
}
