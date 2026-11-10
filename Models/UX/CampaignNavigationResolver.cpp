#include "Models/UX/CampaignNavigationResolver.hpp"
#include <algorithm>

namespace gm::ux
{
namespace
{
bool archived(const EntityMetadata &metadata)
{
    return std::find(metadata.tags.begin(), metadata.tags.end(), "archived") != metadata.tags.end();
}
template<class Collection>
NavigationEntity find(const Collection &collection, const EntityId &id, const char *type)
{
    for (const auto &entity : collection)
        if (entity.metadata.id == id) return {type, !archived(entity.metadata)};
    return {};
}
}

NavigationEntity resolveNavigationEntity(const Campaign &campaign, const EntityId &id)
{
    if (id.empty()) return {};
    if (id == campaign.partyInventory.metadata.id) return {"PartyInventory", true};
    for (auto result : {
        find(campaign.characters, id, "Player"), find(campaign.npcs, id, "NPC"),
        find(campaign.customMonsters, id, "Monster"), find(campaign.factions, id, "Faction"),
        find(campaign.deities, id, "Deity"), find(campaign.nations, id, "Nation"),
        find(campaign.worldPlaces, id, "Place"), find(campaign.customItems, id, "Item"),
        find(campaign.customSpells, id, "Spell"), find(campaign.maps, id, "Map"),
        find(campaign.dungeons, id, "Dungeon"), find(campaign.shops, id, "Shop"),
        find(campaign.encounters, id, "Encounter"), find(campaign.audioAssets, id, "Audio"),
        find(campaign.randomTables, id, "Random Table"),
        find(campaign.sessions, id, "Session"), find(campaign.storyThreads, id, "story"),
        find(campaign.storyObjectives, id, "Objective"), find(campaign.storyClues, id, "Clue")})
        if (!result.type.empty()) return result;
    for (const auto &session : campaign.sessions)
        for (const auto &entry : session.planEntries)
            if (entry.metadata.id == id) return {"PlanEntry", !archived(session.metadata)};
    for (const auto &thread : campaign.storyThreads)
        for (const auto &scene : thread.beats)
            if (scene.metadata.id == id) return {"scene", !archived(thread.metadata) && !archived(scene.metadata)};
    if (campaign.ruleSet)
    {
        const auto item = find(campaign.ruleSet->getItems(), id, "Item");
        if (!item.type.empty()) return item;
        const auto spell = find(campaign.ruleSet->getSpells(), id, "Spell");
        if (!spell.type.empty()) return spell;
        const auto monster = find(campaign.ruleSet->getMonsters(), id, "Monster");
        if (!monster.type.empty()) return monster;
    }
    return {};
}
}
