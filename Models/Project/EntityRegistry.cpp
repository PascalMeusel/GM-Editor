#include "EntityRegistry.hpp"

#include <algorithm>

namespace
{
std::string fallbackName(const std::string &displayName, const std::string &fallback)
{
    return displayName.empty() ? fallback : displayName;
}
}

EntityRegistry &EntityRegistry::getInstance()
{
    static EntityRegistry registry;
    return registry;
}

void EntityRegistry::rebuild(const Campaign &campaign)
{
    EntityRegistry replacement;
    replacement.populate(campaign);
    _entries.swap(replacement._entries);
}

void EntityRegistry::populate(const Campaign &campaign)
{
    const EntityId campaignId = campaign.metadata.id;
    add(campaign.metadata.id, "campaign", fallbackName(campaign.name, "Campaign"), campaignId);

    for (const CharacterModel &character : campaign.characters)
        add(character.metadata.id, "character", fallbackName(std::string(character.nameBuffer), "Unnamed character"), campaignId);

    for (const NPCModel &npc : campaign.npcs)
        add(npc.metadata.id, "npc", fallbackName(npc.name, "Unnamed NPC"), campaignId);

    for (const FactionModel &faction : campaign.factions)
        add(faction.metadata.id, "faction", fallbackName(faction.name, "Unnamed faction"), campaignId);

    for (const DeityModel &deity : campaign.deities)
        add(deity.metadata.id, "deity", fallbackName(deity.name, "Unnamed deity"), campaignId);

    for (const NationModel &nation : campaign.nations)
    {
        add(nation.metadata.id, "nation", fallbackName(nation.name, "Unnamed nation"), campaignId);
        for (const NationTerritoryModel &territory : nation.territories)
            add(territory.metadata.id, "nation_territory", fallbackName(territory.name, "Unnamed territory"), campaignId);
    }

    for (const WorldPlaceModel &place : campaign.worldPlaces)
        add(place.metadata.id, "world_place", fallbackName(place.name, "Unnamed place"), campaignId);

    for (const ItemModel &item : campaign.customItems)
        add(item.metadata.id, "item", fallbackName(item.name, "Unnamed item"), campaignId);

    for (const SpellModel &spell : campaign.customSpells)
        add(spell.metadata.id, "spell", fallbackName(spell.name, "Unnamed spell"), campaignId);

    for (const RandomTableModel &table : campaign.randomTables)
    {
        add(table.metadata.id, "random_table", fallbackName(table.name, "Unnamed random table"), campaignId);
        for (const RandomTableEntryModel &entry : table.entries)
            add(entry.metadata.id, "random_table_entry", fallbackName(entry.result, "Unnamed table entry"), campaignId);
    }

    for (const MonsterDefinitionModel &monster : campaign.customMonsters)
        add(monster.metadata.id, "monster", fallbackName(monster.name, "Unnamed monster"), campaignId);

    for (const MapModel &map : campaign.maps)
        add(map.metadata.id, "map", fallbackName(map.name, "Unnamed map"), campaignId);

    for (const DungeonModel &dungeon : campaign.dungeons)
    {
        add(dungeon.metadata.id, "dungeon", fallbackName(dungeon.name, "Unnamed dungeon"), campaignId);
        for (const DungeonRoomModel &room : dungeon.rooms)
            add(room.metadata.id, "dungeon_room", fallbackName(room.name, "Unnamed room"), campaignId);
    }

    for (const SessionModel &session : campaign.sessions)
        add(session.metadata.id, "session", fallbackName(session.title, "Untitled session"), campaignId);

    for (const TimelineEventModel &event : campaign.timelineEvents)
        add(event.metadata.id, "timeline", fallbackName(event.title, "Timeline event"), campaignId);

    for (const AudioAssetModel &asset : campaign.audioAssets)
        add(asset.metadata.id, "audio", fallbackName(asset.name, "Unnamed audio"), campaignId);

    for (const ShopModel &shop : campaign.shops)
        add(shop.metadata.id, "shop", fallbackName(shop.name, "Unnamed shop"), campaignId);

    for (const EncounterModel &encounter : campaign.encounters)
    {
        add(encounter.metadata.id, "encounter", fallbackName(encounter.name, "Unnamed encounter"), campaignId);
        for (const EncounterParticipantModel &participant : encounter.participants)
            add(participant.metadata.id, "combatant", fallbackName(participant.displayName, "Unnamed combatant"), campaignId);
        for (const CombatLogEntryModel &entry : encounter.combatLog)
            add(entry.metadata.id, "combat_log", fallbackName(entry.summary, "Combat log entry"), campaignId);
    }

    for (const StoryThreadModel &thread : campaign.storyThreads)
    {
        add(thread.metadata.id, "story_thread", fallbackName(thread.title, "Untitled story"), campaignId);
        for (const StoryLinkModel &link : thread.links)
            add(link.metadata.id, "story_link", fallbackName(link.label, "Story link"), campaignId);
        for (const StoryBeatModel &beat : thread.beats)
        {
            add(beat.metadata.id, "story_beat", fallbackName(beat.title, "Untitled beat"), campaignId);
            for (const StoryLinkModel &link : beat.links)
                add(link.metadata.id, "story_link", fallbackName(link.label, "Story link"), campaignId);
        }
    }
}

const EntityRegistryEntry *EntityRegistry::find(const EntityId &id) const
{
    const auto it = _entries.find(id);
    return it == _entries.end() ? nullptr : &it->second;
}

bool EntityRegistry::exists(const EntityId &id) const
{
    const EntityRegistryEntry *entry = find(id);
    return entry != nullptr && entry->exists;
}

std::string EntityRegistry::displayNameFor(const EntityId &id) const
{
    const EntityRegistryEntry *entry = find(id);
    return entry == nullptr ? std::string{} : entry->displayName;
}

std::vector<EntityRegistryEntry> EntityRegistry::entries() const
{
    std::vector<EntityRegistryEntry> result;
    result.reserve(_entries.size());
    for (const auto &entry : _entries)
        result.push_back(entry.second);

    std::sort(result.begin(), result.end(), [](const EntityRegistryEntry &left, const EntityRegistryEntry &right) {
        if (left.type == right.type)
            return left.displayName < right.displayName;
        return left.type < right.type;
    });
    return result;
}

void EntityRegistry::add(const EntityId &id, const std::string &type, const std::string &displayName, const EntityId &campaignId)
{
    if (id.empty())
        return;

    _entries[id] = EntityRegistryEntry{id, type, displayName, campaignId, true};
}
