#pragma once

#include "Views/Editor/Widgets/UiAdaptiveFields.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorOutputPreviewRendering.hpp"
#include "GMScreenView.hpp"
#include "Controllers/Editor/Widgets/GMScreen/GMScreenController.hpp"
#include "Models/Map/MapImageImportService.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include "Models/Rulesets/CharacterRulesService.hpp"
#include "Models/Session/PlayerSafeDataFilter.hpp"
#include "Views/Editor/Widgets/ImageTextureCache.hpp"
#include "Views/Editor/Widgets/Inventory/InventoryDisplayHelpers.hpp"
#include "Views/Editor/Widgets/Map/MapTileVisuals.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"
#include "misc/cpp/imgui_stdlib.h"
#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace
{
struct AudioBreakCounts
{
    int active = 0;
    int playing = 0;
    int paused = 0;
};

AudioBreakCounts audioBreakCounts(const std::shared_ptr<AudioPlaybackService> &playbackService)
{
    AudioBreakCounts counts;
    if (!playbackService)
        return counts;

    const std::vector<EntityId> activeIds = playbackService->activeAssetIds();
    counts.active = static_cast<int>(activeIds.size());
    for (const EntityId &assetId : activeIds)
    {
        if (playbackService->isPaused(assetId))
            ++counts.paused;
        else
            ++counts.playing;
    }
    return counts;
}

std::string joinStrings(const std::vector<std::string> &values)
{
    if (values.empty())
        return "-";

    std::string result;
    for (const std::string &value : values)
    {
        if (!result.empty())
            result += ", ";
        result += value;
    }
    return result;
}

template <typename... Arguments>
std::string formatLocalizedUi(const char *englishFormat, Arguments... arguments)
{
    const std::string format = localizedUiLabel(englishFormat);
    const int required = std::snprintf(nullptr, 0, format.c_str(), arguments...);
    if (required <= 0)
        return format;

    std::vector<char> buffer(static_cast<std::size_t>(required) + 1U, '\0');
    std::snprintf(buffer.data(), buffer.size(), format.c_str(), arguments...);
    return std::string(buffer.data(), static_cast<std::size_t>(required));
}

bool hasTrackedSlots(CharacterSpellcastingModel spellcasting)
{
    normalizeSpellcasting(spellcasting);
    for (int level = 1; level <= 9; ++level)
    {
        if (spellcasting.slotMaximums[static_cast<std::size_t>(level)] > 0 || spellcasting.slotUsed[static_cast<std::size_t>(level)] > 0)
            return true;
    }
    return false;
}

bool containsId(const std::vector<EntityId> &ids, const EntityId &id)
{
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

std::string publicSceneLabel(const Campaign &campaign, const EntityId &sceneId)
{
    if (sceneId.empty())
        return localizedUiLabel("None");

    for (const StoryThreadModel &thread : campaign.storyThreads)
    {
        for (const StoryBeatModel &beat : thread.beats)
        {
            if (beat.metadata.id == sceneId)
                return thread.title + " / " + beat.title;
        }
    }
    return gm::localization::trUi("Missing scene");
}

std::string mapLabel(const Campaign &campaign, const EntityId &mapId)
{
    if (mapId.empty())
        return localizedUiLabel("None");

    for (const MapModel &map : campaign.maps)
    {
        if (map.metadata.id == mapId)
            return map.name.empty() ? localizedUiLabel("<Unnamed map>") : map.name;
    }
    return gm::localization::trUi("Missing map");
}

std::string encounterLabel(const Campaign &campaign, const EntityId &encounterId)
{
    if (encounterId.empty())
        return localizedUiLabel("None");

    for (const EncounterModel &encounter : campaign.encounters)
    {
        if (encounter.metadata.id == encounterId)
            return encounter.name.empty() ? localizedUiLabel("<Unnamed encounter>") : encounter.name;
    }
    return gm::localization::trUi("Missing encounter");
}

std::string publicDisplaySelectionLabel(const Campaign &campaign, const LocalPlayerDisplayModel &display)
{
    const std::string scene = publicSceneLabel(campaign, display.publicSceneId);
    const std::string map = mapLabel(campaign, display.publicMapId);
    const std::string encounter = encounterLabel(campaign, display.publicEncounterId);
    return formatLocalizedUi("Scene: %s | Map: %s | Initiative: %s", scene.c_str(), map.c_str(), encounter.c_str());
}

bool hasPublicDisplayPayload(const LocalPlayerDisplayModel &display)
{
    return display.enabled ||
           !display.publicSceneId.empty() ||
           !display.publicMapId.empty() ||
           !display.publicEncounterId.empty() ||
           !display.visibleNpcIds.empty() ||
           !display.publicHandoutIds.empty() ||
           !display.sharedArtAssetIds.empty();
}

int activeEffectCount(const Campaign &campaign)
{
    int count = 0;
    for (const CharacterModel &character : campaign.characters)
        count += static_cast<int>(character.conditions.size());
    for (const NPCModel &npc : campaign.npcs)
        count += static_cast<int>(npc.conditions.size());
    return count;
}

int spellSlotTrackerCount(const Campaign &campaign)
{
    int count = 0;
    for (const CharacterModel &character : campaign.characters)
    {
        if (character.spellcasting.enabled || hasTrackedSlots(character.spellcasting))
            ++count;
    }
    for (const NPCModel &npc : campaign.npcs)
    {
        if (npc.spellcasting.enabled || hasTrackedSlots(npc.spellcasting))
            ++count;
    }
    return count;
}

std::string publicVisibilitySummary(const LocalPlayerDisplayModel &display)
{
    std::vector<std::string> enabledPanels;
    if (display.showMap)
        enabledPanels.push_back(localizedUiLabel("Map"));
    if (display.showInitiative)
        enabledPanels.push_back(localizedUiLabel("Initiative"));
    if (display.showVisibleNpcs)
        enabledPanels.push_back(localizedUiLabel("NPCs"));
    if (display.showPublicHandouts)
        enabledPanels.push_back(localizedUiLabel("Handouts"));
    if (display.showSharedArt)
        enabledPanels.push_back(localizedUiLabel("Shared Art"));
    return joinStrings(enabledPanels);
}

std::string handoutStatusLabel(const Campaign &campaign, const LocalPlayerDisplayModel &display)
{
    return formatLocalizedUi("%d public / %d total",
                             static_cast<int>(display.publicHandoutIds.size()),
                             static_cast<int>(campaign.handouts.size()));
}
}
