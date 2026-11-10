#pragma once
#include "Views/Editor/Widgets/UiAdaptiveFields.hpp"
#include "Views/Editor/Widgets/UiPortrait.hpp"
#include "NPCOverviewView.hpp"
#include "Controllers/Editor/Widgets/NPCOverview/NPCOverviewController.hpp"
#include "Views/Editor/Widgets/Inventory/InventoryDisplayHelpers.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"
#include "misc/cpp/imgui_stdlib.h"
#include "imgui.h"
#include "Views/Editor/Widgets/UiTaskLayout.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>


namespace
{
struct StoryReference
{
    int threadIndex = -1;
    std::string sceneTitle;
    std::string note;
};

struct EncounterReference
{
    std::string name;
    int round = 1;
    bool defeated = false;
};

bool linkTargetsEntity(const StoryLinkModel &link, const EntityId &entityId, const std::string &entityType)
{
    return link.targetEntityType == entityType && link.targetEntityId == entityId;
}

std::vector<StoryReference> findStoryReferences(const Campaign &campaign, const EntityId &entityId, const std::string &entityType)
{
    std::vector<StoryReference> references;
    for (int threadIndex = 0; threadIndex < static_cast<int>(campaign.storyThreads.size()); ++threadIndex)
    {
        const StoryThreadModel &thread = campaign.storyThreads[threadIndex];
        for (const StoryLinkModel &link : thread.links)
        {
            if (linkTargetsEntity(link, entityId, entityType))
                references.push_back({threadIndex, "", link.label});
        }

        for (const StoryBeatModel &beat : thread.beats)
        {
            for (const StoryLinkModel &link : beat.links)
            {
                if (linkTargetsEntity(link, entityId, entityType))
                    references.push_back({threadIndex, beat.title, link.label});
            }
        }
    }
    return references;
}

std::vector<EncounterReference> findEncounterReferences(const Campaign &campaign, const EntityId &entityId)
{
    std::vector<EncounterReference> references;
    for (const EncounterModel &encounter : campaign.encounters)
    {
        for (const EncounterParticipantModel &participant : encounter.participants)
        {
            if (participant.sourceEntityId == entityId || participant.monsterDefinitionId == entityId)
            {
                references.push_back({encounter.name.empty() ? "<Unnamed Encounter>" : encounter.name, encounter.round, participant.defeated});
                break;
            }
        }
    }
    return references;
}

bool containsEntityId(const std::vector<EntityId> &ids, const EntityId &id)
{
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

bool hasVisibleText(const std::string &value)
{
    return std::any_of(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) == 0;
    });
}

int npcEncounterReferenceCount(const Campaign &campaign, const EntityId &entityId)
{
    int count = 0;
    for (const EncounterModel &encounter : campaign.encounters)
    {
        for (const EncounterParticipantModel &participant : encounter.participants)
        {
            if (participant.sourceEntityId == entityId || participant.monsterDefinitionId == entityId)
            {
                ++count;
                break;
            }
        }
    }
    return count;
}

int npcStoryReferenceCount(const Campaign &campaign, const EntityId &entityId, const std::string &entityType)
{
    return static_cast<int>(findStoryReferences(campaign, entityId, entityType).size());
}

const FactionModel *findFactionById(const Campaign &campaign, const EntityId &factionId)
{
    const auto found = std::find_if(campaign.factions.begin(), campaign.factions.end(), [&](const FactionModel &faction) {
        return faction.metadata.id == factionId;
    });
    return found == campaign.factions.end() ? nullptr : &*found;
}

const DeityModel *findDeityById(const Campaign &campaign, const EntityId &deityId)
{
    const auto found = std::find_if(campaign.deities.begin(), campaign.deities.end(), [&](const DeityModel &deity) {
        return deity.metadata.id == deityId;
    });
    return found == campaign.deities.end() ? nullptr : &*found;
}

std::string factionDisplayName(const Campaign &campaign, const NPCModel &npc)
{
    if (const FactionModel *faction = findFactionById(campaign, npc.factionId))
        return faction->name.empty() ? localizedUiLabel("<Unnamed Faction>") : faction->name;
    return npc.faction.empty() ? localizedUiLabel("None") : npc.faction;
}

std::string npcConditionsLabel(const NPCModel &npc)
{
    if (npc.conditions.empty())
        return localizedUiLabel("None");

    std::string label;
    for (const std::string &condition : npc.conditions)
    {
        if (!label.empty())
            label += ", ";
        label += condition;
    }
    return label;
}

std::string npcSpellSlotsLabel(CharacterSpellcastingModel spellcasting)
{
    if (!spellcasting.enabled)
        return localizedUiLabel("No magic");

    normalizeSpellcasting(spellcasting);
    int maximumTotal = 0;
    int usedTotal = 0;
    for (int level = 1; level <= 9; ++level)
    {
        maximumTotal += spellcasting.slotMaximums[static_cast<std::size_t>(level)];
        usedTotal += spellcasting.slotUsed[static_cast<std::size_t>(level)];
    }

    if (maximumTotal <= 0)
        return localizedUiLabel("Magic, no slots");
    return std::to_string(std::max(0, maximumTotal - usedTotal)) + "/" + std::to_string(maximumTotal) +
           " " + localizedUiLabel("spell slots left");
}
}
