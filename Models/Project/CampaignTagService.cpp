#include "CampaignTagService.hpp"

#include <algorithm>
#include <cctype>
#include <set>

namespace
{
std::string trimCopy(const std::string &value)
{
    std::size_t first = 0;
    while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first])))
        ++first;

    std::size_t last = value.size();
    while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1])))
        --last;

    return value.substr(first, last - first);
}

std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

template <typename T>
EntityMetadata *findInVector(std::vector<T> &items, const EntityId &entityId)
{
    for (T &item : items)
    {
        if (item.metadata.id == entityId)
            return &item.metadata;
    }
    return nullptr;
}

template <typename T>
const EntityMetadata *findInVector(const std::vector<T> &items, const EntityId &entityId)
{
    for (const T &item : items)
    {
        if (item.metadata.id == entityId)
            return &item.metadata;
    }
    return nullptr;
}

void collectTags(const EntityMetadata &metadata, std::set<std::string> &seen, std::vector<std::string> &tags)
{
    for (const std::string &tag : metadata.tags)
    {
        const std::string normalized = trimCopy(tag);
        const std::string key = lowerCopy(normalized);
        if (normalized.empty() || !seen.insert(key).second)
            continue;
        tags.push_back(normalized);
    }
}
}

namespace CampaignTags
{
std::vector<std::string> normalize(std::vector<std::string> tags)
{
    std::vector<std::string> normalizedTags;
    std::set<std::string> seen;
    for (const std::string &tag : tags)
    {
        const std::string normalized = trimCopy(tag);
        const std::string key = lowerCopy(normalized);
        if (normalized.empty() || !seen.insert(key).second)
            continue;
        normalizedTags.push_back(normalized);
    }
    return normalizedTags;
}

EntityMetadata *findEntityMetadata(Campaign &campaign, const EntityId &entityId)
{
    if (campaign.metadata.id == entityId)
        return &campaign.metadata;

    if (EntityMetadata *metadata = findInVector(campaign.characters, entityId))
        return metadata;
    if (EntityMetadata *metadata = findInVector(campaign.npcs, entityId))
        return metadata;
    if (EntityMetadata *metadata = findInVector(campaign.factions, entityId))
        return metadata;
    if (EntityMetadata *metadata = findInVector(campaign.deities, entityId))
        return metadata;
    if (EntityMetadata *metadata = findInVector(campaign.nations, entityId))
        return metadata;
    if (EntityMetadata *metadata = findInVector(campaign.worldPlaces, entityId))
        return metadata;
    if (EntityMetadata *metadata = findInVector(campaign.customItems, entityId))
        return metadata;
    if (EntityMetadata *metadata = findInVector(campaign.customSpells, entityId))
        return metadata;
    if (EntityMetadata *metadata = findInVector(campaign.randomTables, entityId))
        return metadata;
    if (EntityMetadata *metadata = findInVector(campaign.storyObjectives, entityId))
        return metadata;
    if (EntityMetadata *metadata = findInVector(campaign.storyClues, entityId))
        return metadata;
    if (EntityMetadata *metadata = findInVector(campaign.customMonsters, entityId))
        return metadata;
    if (EntityMetadata *metadata = findInVector(campaign.audioAssets, entityId))
        return metadata;
    if (EntityMetadata *metadata = findInVector(campaign.audioPlaylists, entityId))
        return metadata;
    if (EntityMetadata *metadata = findInVector(campaign.audioAmbienceLayers, entityId))
        return metadata;
    if (EntityMetadata *metadata = findInVector(campaign.audioTriggers, entityId))
        return metadata;
    if (EntityMetadata *metadata = findInVector(campaign.shops, entityId))
        return metadata;
    if (EntityMetadata *metadata = findInVector(campaign.lootParcels, entityId))
        return metadata;
    if (EntityMetadata *metadata = findInVector(campaign.handouts, entityId))
        return metadata;
    if (EntityMetadata *metadata = findInVector(campaign.timelineEvents, entityId))
        return metadata;
    if (EntityMetadata *metadata = findInVector(campaign.rulesetDefinitions, entityId))
        return metadata;
    if (EntityMetadata *metadata = findInVector(campaign.rulesetContentPacks, entityId))
        return metadata;
    if (EntityMetadata *metadata = findInVector(campaign.houseRules, entityId))
        return metadata;
    if (campaign.localPlayerDisplay.metadata.id == entityId)
        return &campaign.localPlayerDisplay.metadata;
    if (campaign.partyTravel.metadata.id == entityId)
        return &campaign.partyTravel.metadata;
    if (campaign.partyInventory.metadata.id == entityId)
        return &campaign.partyInventory.metadata;

    for (MapModel &map : campaign.maps)
    {
        if (map.metadata.id == entityId)
            return &map.metadata;
        if (EntityMetadata *metadata = findInVector(map.layers, entityId))
            return metadata;
        if (EntityMetadata *metadata = findInVector(map.tokens, entityId))
            return metadata;
        if (EntityMetadata *metadata = findInVector(map.regions, entityId))
            return metadata;
        if (EntityMetadata *metadata = findInVector(map.features, entityId))
            return metadata;
        if (EntityMetadata *metadata = findInVector(map.routes, entityId))
            return metadata;
    }

    for (NationModel &nation : campaign.nations)
    {
        if (EntityMetadata *metadata = findInVector(nation.territories, entityId))
            return metadata;
    }

    for (DungeonModel &dungeon : campaign.dungeons)
    {
        if (dungeon.metadata.id == entityId)
            return &dungeon.metadata;
        for (DungeonRoomModel &room : dungeon.rooms)
        {
            if (room.metadata.id == entityId)
                return &room.metadata;
            if (EntityMetadata *metadata = findInVector(room.connections, entityId))
                return metadata;
        }
    }

    for (SessionModel &session : campaign.sessions)
    {
        if (session.metadata.id == entityId)
            return &session.metadata;
        if (EntityMetadata *metadata = findInVector(session.notes, entityId))
            return metadata;
        if (EntityMetadata *metadata = findInVector(session.planEntries, entityId))
            return metadata;
    }

    for (EncounterModel &encounter : campaign.encounters)
    {
        if (encounter.metadata.id == entityId)
            return &encounter.metadata;
        if (EntityMetadata *metadata = findInVector(encounter.combatLog, entityId))
            return metadata;
        if (EntityMetadata *metadata = findInVector(encounter.diceRollPresets, entityId))
            return metadata;
        for (EncounterParticipantModel &participant : encounter.participants)
        {
            if (participant.metadata.id == entityId)
                return &participant.metadata;
            if (EntityMetadata *metadata = findInVector(participant.activeEffects, entityId))
                return metadata;
        }
    }

    for (StoryThreadModel &thread : campaign.storyThreads)
    {
        if (thread.metadata.id == entityId)
            return &thread.metadata;
        if (EntityMetadata *metadata = findInVector(thread.links, entityId))
            return metadata;
        for (StoryBeatModel &beat : thread.beats)
        {
            if (beat.metadata.id == entityId)
                return &beat.metadata;
            if (EntityMetadata *metadata = findInVector(beat.links, entityId))
                return metadata;
        }
    }

    for (RandomTableModel &table : campaign.randomTables)
        if (EntityMetadata *metadata = findInVector(table.entries, entityId))
            return metadata;

    return nullptr;
}

const EntityMetadata *findEntityMetadata(const Campaign &campaign, const EntityId &entityId)
{
    return findEntityMetadata(const_cast<Campaign &>(campaign), entityId);
}

std::vector<std::string> tagsForEntity(const Campaign &campaign, const EntityId &entityId)
{
    const EntityMetadata *metadata = findEntityMetadata(campaign, entityId);
    return metadata == nullptr ? std::vector<std::string>{} : metadata->tags;
}

bool setTagsForEntity(Campaign &campaign, const EntityId &entityId, const std::vector<std::string> &tags)
{
    EntityMetadata *metadata = findEntityMetadata(campaign, entityId);
    if (metadata == nullptr)
        return false;

    metadata->tags = normalize(tags);
    metadata->touch();
    return true;
}

bool isEntityArchived(const Campaign &campaign, const EntityId &entityId)
{
    const EntityMetadata *metadata = findEntityMetadata(campaign, entityId);
    if (metadata == nullptr)
        return false;

    return std::any_of(metadata->tags.begin(), metadata->tags.end(), [](const std::string &tag) {
        return lowerCopy(trimCopy(tag)) == "archived";
    });
}

bool setEntityArchived(Campaign &campaign, const EntityId &entityId, bool archived)
{
    EntityMetadata *metadata = findEntityMetadata(campaign, entityId);
    if (metadata == nullptr)
        return false;

    std::vector<std::string> tags;
    tags.reserve(metadata->tags.size() + (archived ? 1U : 0U));
    for (const std::string &tag : metadata->tags)
    {
        if (lowerCopy(trimCopy(tag)) != "archived")
            tags.push_back(tag);
    }
    if (archived)
        tags.push_back("archived");

    tags = normalize(std::move(tags));
    if (metadata->tags == tags)
        return true;

    metadata->tags = std::move(tags);
    metadata->touch();
    return true;
}

std::vector<std::string> collectAllTags(const Campaign &campaign)
{
    std::set<std::string> seen;
    std::vector<std::string> tags;
    collectTags(campaign.metadata, seen, tags);

    const auto collectVector = [&](const auto &items) {
        for (const auto &item : items)
            collectTags(item.metadata, seen, tags);
    };

    collectVector(campaign.characters);
    collectVector(campaign.npcs);
    collectVector(campaign.factions);
    collectVector(campaign.deities);
    collectVector(campaign.nations);
    collectVector(campaign.worldPlaces);
    collectVector(campaign.customItems);
    collectVector(campaign.customSpells);
    collectVector(campaign.customMonsters);
    collectVector(campaign.audioAssets);
    collectVector(campaign.audioPlaylists);
    collectVector(campaign.audioAmbienceLayers);
    collectVector(campaign.audioTriggers);
    collectVector(campaign.shops);
    collectVector(campaign.lootParcels);
    collectVector(campaign.handouts);
    collectVector(campaign.sessions);
    collectVector(campaign.storyThreads);
    collectVector(campaign.randomTables);
    collectVector(campaign.timelineEvents);
    collectVector(campaign.rulesetDefinitions);
    collectVector(campaign.rulesetContentPacks);
    collectVector(campaign.houseRules);
    collectVector(campaign.maps);
    collectVector(campaign.dungeons);
    collectVector(campaign.encounters);

    for (const MapModel &map : campaign.maps)
    {
        collectVector(map.layers);
        collectVector(map.tokens);
        collectVector(map.regions);
        collectVector(map.features);
        collectVector(map.routes);
    }
    for (const NationModel &nation : campaign.nations)
        collectVector(nation.territories);
    for (const DungeonModel &dungeon : campaign.dungeons)
    {
        collectVector(dungeon.rooms);
        for (const DungeonRoomModel &room : dungeon.rooms)
            collectVector(room.connections);
    }
    for (const SessionModel &session : campaign.sessions)
        collectVector(session.notes);
    for (const EncounterModel &encounter : campaign.encounters)
    {
        collectVector(encounter.participants);
        collectVector(encounter.combatLog);
        collectVector(encounter.diceRollPresets);
        for (const EncounterParticipantModel &participant : encounter.participants)
            collectVector(participant.activeEffects);
    }
    for (const StoryThreadModel &thread : campaign.storyThreads)
    {
        collectVector(thread.links);
        collectVector(thread.beats);
        for (const StoryBeatModel &beat : thread.beats)
            collectVector(beat.links);
    }
    for (const RandomTableModel &table : campaign.randomTables)
        collectVector(table.entries);

    std::sort(tags.begin(), tags.end(), [](const std::string &left, const std::string &right) {
        return lowerCopy(left) < lowerCopy(right);
    });
    return tags;
}
}
