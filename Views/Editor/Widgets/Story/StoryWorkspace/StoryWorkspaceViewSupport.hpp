#pragma once
#include "Views/Editor/Widgets/UiAdaptiveFields.hpp"
#include "StoryWorkspaceView.hpp"
#include "Controllers/Editor/Widgets/StoryWorkspace/StoryWorkspaceController.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"
#include "misc/cpp/imgui_stdlib.h"
#include "imgui.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <vector>

namespace
{
constexpr int kThreadTypeCount = 10;
constexpr int kStoryStatusCount = 9;
constexpr int kStoryLinkTypeCount = 11;

void drawWrappedMuted(const char *text)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.64f, 0.70f, 0.76f, 1.0f));
    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), localizedUiText(text));
    ImGui::PopStyleColor();
}

std::string storySelectionLabel(const StoryThreadModel *thread, const StoryBeatModel *beat)
{
    if (!thread)
        return localizedUiText("No arc selected");
    std::string label = thread->title.empty() ? localizedUiText("Untitled arc") : thread->title;
    if (beat)
        label += " / " + (beat->title.empty() ? std::string{localizedUiText("Untitled scene")} : beat->title);
    return label;
}

std::string sceneLaunchPreview(const StoryBeatModel &beat, const AudioAssetModel *cueAsset)
{
    std::string preview = localizedUiText("Launch marks this arc and scene Active, stages Player Display scene/map/initiative state from links, saves the campaign, and runs Scene Started triggers.");
    if (cueAsset)
        preview += std::string(localizedUiText(" Direct cue: ")) + cueAsset->name + ".";
    else if (!beat.cueAudioAssetId.empty())
        preview += localizedUiText(" The selected direct cue is missing from the audio library.");
    else
        preview += localizedUiText(" No direct cue selected.");
    preview += localizedUiText(" NPCs and handouts are revealed only from scene links marked Reveals.");
    return preview;
}

std::string lowerCopy(const std::string &value)
{
    std::string lowered = value;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return lowered;
}

bool storyEntityMatchesSearch(const StoryEntityOption &option, const std::string &query)
{
    if (query.empty())
        return true;

    const std::string loweredQuery = lowerCopy(query);
    return lowerCopy(option.label).find(loweredQuery) != std::string::npos ||
           lowerCopy(option.type).find(loweredQuery) != std::string::npos ||
           lowerCopy(option.id).find(loweredQuery) != std::string::npos;
}

enum class ChecklistState
{
    Ready,
    Open,
    Missing
};

struct SceneChecklistFacts
{
    int npcLinks = 0;
    int mapLinks = 0;
    int encounterLinks = 0;
    int encounterMaps = 0;
    int handoutLinks = 0;
    int audioLinks = 0;
    int clueLinks = 0;
    int npcSecrets = 0;
    int mapSecrets = 0;
    int rewardLinks = 0;
    bool directCue = false;
    bool missingCue = false;
};

bool checklistLinkIsClue(const StoryLinkModel &link)
{
    return link.type == StoryLinkType::Reveals || link.type == StoryLinkType::Foreshadows;
}

std::vector<const StoryLinkModel *> checklistLinksForType(const StoryBeatModel &beat, const std::string &targetType)
{
    std::vector<const StoryLinkModel *> links;
    for (const StoryLinkModel &link : beat.links)
    {
        if (link.targetEntityType == targetType)
            links.push_back(&link);
    }
    return links;
}

const NPCModel *findChecklistNpc(const Campaign &campaign, const EntityId &npcId)
{
    const auto found = std::find_if(campaign.npcs.begin(), campaign.npcs.end(), [&](const NPCModel &npc) {
        return npc.metadata.id == npcId;
    });
    return found == campaign.npcs.end() ? nullptr : &(*found);
}

const MapModel *findChecklistMap(const Campaign &campaign, const EntityId &mapId)
{
    const auto found = std::find_if(campaign.maps.begin(), campaign.maps.end(), [&](const MapModel &map) {
        return map.metadata.id == mapId;
    });
    return found == campaign.maps.end() ? nullptr : &(*found);
}

const EncounterModel *findChecklistEncounter(const Campaign &campaign, const EntityId &encounterId)
{
    const auto found = std::find_if(campaign.encounters.begin(), campaign.encounters.end(), [&](const EncounterModel &encounter) {
        return encounter.metadata.id == encounterId;
    });
    return found == campaign.encounters.end() ? nullptr : &(*found);
}

int checklistMapSecretCount(const MapModel *map)
{
    if (!map)
        return 0;

    int count = 0;
    for (const MapFeatureModel &feature : map->features)
    {
        if (feature.gmOnly || feature.kind == MapFeatureKind::SecretDoor)
            ++count;
    }
    return count;
}

SceneChecklistFacts buildSceneChecklistFacts(const Campaign &campaign, const StoryBeatModel &beat, const AudioAssetModel *cueAsset)
{
    SceneChecklistFacts facts;
    facts.directCue = !beat.cueAudioAssetId.empty() && cueAsset != nullptr;
    facts.missingCue = !beat.cueAudioAssetId.empty() && cueAsset == nullptr;

    for (const StoryLinkModel &link : beat.links)
    {
        if (link.targetEntityType == "NPC")
        {
            ++facts.npcLinks;
            const NPCModel *npc = findChecklistNpc(campaign, link.targetEntityId);
            if (npc && !npc->secrets.empty())
                ++facts.npcSecrets;
        }
        else if (link.targetEntityType == "Map")
        {
            ++facts.mapLinks;
            facts.mapSecrets += checklistMapSecretCount(findChecklistMap(campaign, link.targetEntityId));
        }
        else if (link.targetEntityType == "Encounter")
        {
            ++facts.encounterLinks;
            const EncounterModel *encounter = findChecklistEncounter(campaign, link.targetEntityId);
            if (encounter && !encounter->mapId.empty())
            {
                ++facts.encounterMaps;
                facts.mapSecrets += checklistMapSecretCount(findChecklistMap(campaign, encounter->mapId));
            }
        }
        else if (link.targetEntityType == "Handout")
        {
            ++facts.handoutLinks;
        }
        else if (link.targetEntityType == "Audio")
        {
            ++facts.audioLinks;
        }
        else if (link.targetEntityType == "Item" || link.targetEntityType == "Loot")
        {
            ++facts.rewardLinks;
        }

        if (checklistLinkIsClue(link))
            ++facts.clueLinks;
    }

    return facts;
}

const char *checklistStateLabel(ChecklistState state)
{
    switch (state)
    {
    case ChecklistState::Ready:
        return "Ready";
    case ChecklistState::Missing:
        return "Missing";
    case ChecklistState::Open:
        return "Open";
    }
    return "Open";
}

ImVec4 checklistStateColor(ChecklistState state)
{
    switch (state)
    {
    case ChecklistState::Ready:
        return ImVec4(0.48f, 0.78f, 0.54f, 1.0f);
    case ChecklistState::Missing:
        return ImVec4(0.95f, 0.42f, 0.38f, 1.0f);
    case ChecklistState::Open:
        return ImVec4(0.64f, 0.70f, 0.76f, 1.0f);
    }
    return ImVec4(0.64f, 0.70f, 0.76f, 1.0f);
}

void drawChecklistRow(const char *item, ChecklistState state, const std::string &detail, const char *emptyReason)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(localizedUiText(item));
    ImGui::TableNextColumn();
    ImGui::TextColored(checklistStateColor(state), localizedUiLabel("%s").c_str(), localizedUiText(checklistStateLabel(state)));
    ImGui::TableNextColumn();
    if (!detail.empty())
        tableTextWrapped(detail);
    else
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), localizedUiText(emptyReason));
}

void drawSceneStatusOverview(StoryThreadModel &thread, int &selectedBeatIndex)
{
    std::array<int, kStoryStatusCount> counts{};
    std::array<int, kStoryStatusCount> firstBeatIndex{};
    firstBeatIndex.fill(-1);
    for (int i = 0; i < static_cast<int>(thread.beats.size()); ++i)
    {
        const int statusIndex = static_cast<int>(thread.beats[static_cast<std::size_t>(i)].status);
        if (statusIndex < 0 || statusIndex >= kStoryStatusCount)
            continue;
        ++counts[static_cast<std::size_t>(statusIndex)];
        if (firstBeatIndex[static_cast<std::size_t>(statusIndex)] < 0)
            firstBeatIndex[static_cast<std::size_t>(statusIndex)] = i;
    }

    ImGui::SeparatorText(localizedUiLabel("Scene Board").c_str());
    showHelpTooltip("Scenes are grouped by status so prep, active play, and resolved consequences can be scanned quickly.");
    if (ImGui::BeginTable("StorySceneStatusBoard", 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        for (int statusIndex = 0; statusIndex < kStoryStatusCount; ++statusIndex)
        {
            if (statusIndex % 3 == 0)
                ImGui::TableNextRow();
            ImGui::TableNextColumn();

            const StoryStatus status = static_cast<StoryStatus>(statusIndex);
            const int count = counts[static_cast<std::size_t>(statusIndex)];
            const int firstIndex = firstBeatIndex[static_cast<std::size_t>(statusIndex)];
            const std::string label = std::string(localizedUiText(storyStatusLabel(status))) + " (" + std::to_string(count) + ")";
            ImGui::PushID(statusIndex);
            ImGui::TextColored(ImVec4(0.88f, 0.68f, 0.36f, 1.0f), localizedUiLabel("%s").c_str(), label.c_str());
            if (firstIndex >= 0)
            {
                const StoryBeatModel &beat = thread.beats[static_cast<std::size_t>(firstIndex)];
                const std::string sceneLabel = beat.title.empty() ? localizedUiText("<Untitled Scene>") : beat.title;
                if (ImGui::Selectable(sceneLabel.c_str(), selectedBeatIndex == firstIndex))
                    selectedBeatIndex = firstIndex;
                if (count > 1)
                    ImGui::TextDisabled(localizedUiLabel("+ %d more").c_str(), count - 1);
            }
            else
            {
                ImGui::TextDisabled(localizedUiLabel("No scenes").c_str());
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}
}
