#include "Models/Project/WorldContextLinks.hpp"
#include "CampaignReferenceService.hpp"

#include "Models/Project/CampaignTagService.hpp"

#include <algorithm>

namespace
{
bool containsEntityId(const std::vector<EntityId> &ids, const EntityId &entityId)
{
    return std::find(ids.begin(), ids.end(), entityId) != ids.end();
}

bool isKnownReference(const Campaign &campaign, const EntityId &entityId)
{
    return CampaignTags::findEntityMetadata(campaign, entityId) != nullptr || containsEntityId(campaign.storyDocumentIds, entityId);
}

void addBacklink(std::vector<CampaignBacklink> &links,
                 const EntityId &targetEntityId,
                 const EntityId &referencedEntityId,
                 const EntityId &sourceEntityId,
                 const std::string &sourceType,
                 const std::string &sourceTitle,
                 const std::string &relationship)
{
    if (targetEntityId.empty() || referencedEntityId != targetEntityId || sourceEntityId == targetEntityId)
        return;

    links.push_back(CampaignBacklink{sourceEntityId, sourceType, sourceTitle.empty() ? sourceType : sourceTitle, relationship});
}

void addBrokenReference(std::vector<CampaignBrokenReference> &diagnostics,
                        const Campaign &campaign,
                        const EntityId &referencedEntityId,
                        const EntityId &sourceEntityId,
                        const std::string &sourceType,
                        const std::string &sourceTitle,
                        const std::string &relationship)
{
    if (referencedEntityId.empty() || sourceEntityId == referencedEntityId || isKnownReference(campaign, referencedEntityId))
        return;

    diagnostics.push_back(CampaignBrokenReference{sourceEntityId, sourceType, sourceTitle.empty() ? sourceType : sourceTitle, relationship, referencedEntityId});
}

std::string characterName(const CharacterModel &character)
{
    return std::string(character.nameBuffer).empty() ? "Unnamed character" : std::string(character.nameBuffer);
}

std::string fallback(const std::string &value, const std::string &fallbackValue)
{
    return value.empty() ? fallbackValue : value;
}
}

namespace CampaignReferences
{
std::vector<CampaignBacklink> backlinksTo(const Campaign &campaign, const EntityId &targetEntityId)
{
    std::vector<CampaignBacklink> links;

    for (const CharacterModel &character : campaign.characters)
    {
        for (const InventoryEntryModel &entry : character.inventory.entries)
            addBacklink(links, targetEntityId, entry.itemId, character.metadata.id, "Player", characterName(character), "inventory item");
        for (const CharacterRelationshipModel &relationship : character.relationships)
            addBacklink(links, targetEntityId, relationship.targetEntityId, character.metadata.id, "Player", characterName(character), relationship.label.empty() ? "relationship" : relationship.label);
        for (const CharacterSpellEntryModel &spell : character.spellcasting.spells)
            addBacklink(links, targetEntityId, spell.spellId, character.metadata.id, "Player", characterName(character), "known spell");
    }

    for (const NPCModel &npc : campaign.npcs)
    {
        addBacklink(links, targetEntityId, npc.factionId, npc.metadata.id, "NPC", fallback(npc.name, "Unnamed NPC"), "faction");
        addBacklink(links, targetEntityId, npc.deityId, npc.metadata.id, "NPC", fallback(npc.name, "Unnamed NPC"), "deity");
        for (const InventoryEntryModel &entry : npc.inventory.entries)
            addBacklink(links, targetEntityId, entry.itemId, npc.metadata.id, "NPC", fallback(npc.name, "Unnamed NPC"), "inventory item");
        for (const CharacterSpellEntryModel &spell : npc.spellcasting.spells)
            addBacklink(links, targetEntityId, spell.spellId, npc.metadata.id, "NPC", fallback(npc.name, "Unnamed NPC"), "known spell");
    }

    for (const FactionModel &faction : campaign.factions)
        addBacklink(links, targetEntityId, faction.leaderNpcId, faction.metadata.id, "Faction", fallback(faction.name, "Unnamed faction"), "leader");

    for (const NationModel &nation : campaign.nations)
    {
        addBacklink(links, targetEntityId, nation.capitalPlaceId, nation.metadata.id, "Nation", fallback(nation.name, "Unnamed nation"), "capital");
        for (const EntityId &factionId : nation.factionIds)
            addBacklink(links, targetEntityId, factionId, nation.metadata.id, "Nation", fallback(nation.name, "Unnamed nation"), "linked faction");
    }

    for (const WorldPlaceModel &place : campaign.worldPlaces)
    {
        addBacklink(links, targetEntityId, place.nationId, place.metadata.id, "World Place", fallback(place.name, "Unnamed place"), "nation");
        addBacklink(links, targetEntityId, place.linkedMapId, place.metadata.id, "World Place", fallback(place.name, "Unnamed place"), "linked map");
        addBacklink(links, targetEntityId, place.linkedFeatureId, place.metadata.id, "World Place", fallback(place.name, "Unnamed place"), "linked map feature");
    }

    for (const ItemModel &item : campaign.customItems)
        addBacklink(links, targetEntityId, item.linkedSpellId, item.metadata.id, "Item", fallback(item.name, "Unnamed item"), "linked spell");

    for (const ShopModel &shop : campaign.shops)
    {
        addBacklink(links, targetEntityId, shop.inventoryId, shop.metadata.id, "Shop", fallback(shop.name, "Unnamed shop"), "shop inventory");
        for (const ShopStockEntryModel &stock : shop.stock)
            addBacklink(links, targetEntityId, stock.itemId, shop.metadata.id, "Shop", fallback(shop.name, "Unnamed shop"), "stock item");
    }

    for (const LootParcelModel &parcel : campaign.lootParcels)
    {
        for (const LootParcelItemModel &item : parcel.items)
            addBacklink(links, targetEntityId, item.itemId, parcel.metadata.id, "Loot", fallback(parcel.name, "Loot parcel"), "loot item");
    }

    for (const MapModel &map : campaign.maps)
    {
        addBacklink(links, targetEntityId, map.parentMapId, map.metadata.id, "Map", fallback(map.name, "Unnamed map"), "parent map");
        for (const auto &fact : map.generation.recipe.facts)
            if (fact.sourceText == "map_setup:existing")
                for (std::size_t index = 0; index < std::min<std::size_t>(2, fact.entityRefs.size()); ++index)
                    addBacklink(links, targetEntityId, fact.entityRefs[index], map.metadata.id, "Map", fallback(map.name, "Unnamed map"), "generated place or nation");
        for (const MapTokenModel &token : map.tokens)
            addBacklink(links, targetEntityId, token.linkedEntityId, map.metadata.id, "Map", fallback(map.name, "Unnamed map"), "map token: " + fallback(token.name, "token"));
        for (const MapFeatureModel &feature : map.features)
        {
            addBacklink(links, targetEntityId, feature.linkedEntityId, map.metadata.id, "Map", fallback(map.name, "Unnamed map"), "map feature: " + fallback(feature.name, "feature"));
            addBacklink(links, targetEntityId, feature.targetMapId, map.metadata.id, "Map", fallback(map.name, "Unnamed map"), "map transition");
        }
        for (const MapRouteModel &route : map.routes)
        {
            addBacklink(links, targetEntityId, route.fromMapId, map.metadata.id, "Map", fallback(map.name, "Unnamed map"), "route origin");
            addBacklink(links, targetEntityId, route.toMapId, map.metadata.id, "Map", fallback(map.name, "Unnamed map"), "route destination");
        }
    }

    for (const DungeonModel &dungeon : campaign.dungeons)
    {
        addBacklink(links, targetEntityId, dungeon.parentMapId, dungeon.metadata.id, "Dungeon", fallback(dungeon.name, "Unnamed dungeon"), "parent map");
        addBacklink(links, targetEntityId, dungeon.activeRoomId, dungeon.metadata.id, "Dungeon", fallback(dungeon.name, "Unnamed dungeon"), "active room");
        for (const EntityId &mapId : dungeon.mapIds)
            addBacklink(links, targetEntityId, mapId, dungeon.metadata.id, "Dungeon", fallback(dungeon.name, "Unnamed dungeon"), "dungeon map");
        for (const DungeonRoomModel &room : dungeon.rooms)
        {
            for (const EntityId &encounterId : room.encounterIds)
                addBacklink(links, targetEntityId, encounterId, room.metadata.id, "Dungeon Room", fallback(room.name, "Unnamed room"), "room encounter");
            for (const EntityId &lootId : room.lootItemIds)
                addBacklink(links, targetEntityId, lootId, room.metadata.id, "Dungeon Room", fallback(room.name, "Unnamed room"), "room loot");
            for (const DungeonRoomConnectionModel &connection : room.connections)
                addBacklink(links, targetEntityId, connection.targetRoomId, room.metadata.id, "Dungeon Room", fallback(room.name, "Unnamed room"), "room connection");
        }
    }

    for (const EncounterModel &encounter : campaign.encounters)
    {
        addBacklink(links, targetEntityId, encounter.mapId, encounter.metadata.id, "Encounter", fallback(encounter.name, "Unnamed encounter"), "encounter map");
        for (const EncounterParticipantModel &participant : encounter.participants)
        {
            addBacklink(links, targetEntityId, participant.sourceEntityId, encounter.metadata.id, "Encounter", fallback(encounter.name, "Unnamed encounter"), "combat participant");
            addBacklink(links, targetEntityId, participant.monsterDefinitionId, encounter.metadata.id, "Encounter", fallback(encounter.name, "Unnamed encounter"), "monster definition");
        }
    }

    for (const SessionModel &session : campaign.sessions)
    {
        for (const EntityId &characterId : session.participantCharacterIds)
            addBacklink(links, targetEntityId, characterId, session.metadata.id, "Session", fallback(session.title, "Untitled session"), "session participant");
        for (const EntityId &encounterId : session.encounterIds)
            addBacklink(links, targetEntityId, encounterId, session.metadata.id, "Session", fallback(session.title, "Untitled session"), "session encounter");
        for (const EntityId &storyId : session.storyDocumentIds)
            addBacklink(links, targetEntityId, storyId, session.metadata.id, "Session", fallback(session.title, "Untitled session"), "session story");
        for (const SessionPlanEntry &entry : session.planEntries)
            addBacklink(links, targetEntityId, entry.storyBeatId, session.metadata.id, "Session", fallback(session.title, "Untitled session"), "planned scene");
        addBacklink(links, targetEntityId, session.liveState.activeEncounterId, session.metadata.id, "Session", fallback(session.title, "Untitled session"), "active encounter");
        for (const SessionNoteModel &note : session.notes)
            addBacklink(links, targetEntityId, note.linkedEntityId, session.metadata.id, "Session", fallback(session.title, "Untitled session"), "session note");
    }

    for (const StoryThreadModel &thread : campaign.storyThreads)
    {
        for (const StoryLinkModel &link : thread.links)
            addBacklink(links, targetEntityId, link.targetEntityId, thread.metadata.id, "Story Arc", fallback(thread.title, "Untitled story"), fallback(link.label, "story link"));
        for (const StoryBeatModel &beat : thread.beats)
        {
            addBacklink(links, targetEntityId, beat.cueAudioAssetId, beat.metadata.id, "Story Beat", fallback(beat.title, "Untitled beat"), "audio cue");
            for (const StoryLinkModel &link : beat.links)
                addBacklink(links, targetEntityId, link.targetEntityId, beat.metadata.id, "Story Beat", fallback(beat.title, "Untitled beat"), fallback(link.label, "story link"));
        }
    }

    for (const TimelineEventModel &event : campaign.timelineEvents)
    {
        for (const EntityId &relatedId : event.relatedEntityIds)
            addBacklink(links, targetEntityId, relatedId, event.metadata.id, "Timeline", fallback(event.title, "Timeline event"), "related entity");
    }

    for (const RandomTableModel &table : campaign.randomTables)
        for (const RandomTableEntryModel &entry : table.entries)
            addBacklink(links, targetEntityId, entry.linkedEntityId, table.metadata.id, "Random Table",
                        fallback(table.name, "Unnamed random table"), fallback(entry.result, "table result"));

    for (const AudioTriggerModel &trigger : campaign.audioTriggers)
    {
        addBacklink(links, targetEntityId, trigger.sourceEntityId, trigger.metadata.id, "Audio Trigger", fallback(trigger.name, "Audio trigger"), "trigger source");
        addBacklink(links, targetEntityId, trigger.audioAssetId, trigger.metadata.id, "Audio Trigger", fallback(trigger.name, "Audio trigger"), "trigger audio");
    }

    std::sort(links.begin(), links.end(), [](const CampaignBacklink &left, const CampaignBacklink &right) {
        if (left.sourceType == right.sourceType)
            return left.sourceTitle < right.sourceTitle;
        return left.sourceType < right.sourceType;
    });
    for (const auto &link : WorldContextLinks::links(campaign))
        addBacklink(links, targetEntityId, link.targetId, link.sourceId, link.sourceType, link.sourceName,
            std::string("Context: ") + WorldContextLinks::targetType(link.kind));
    return links;
}

std::vector<CampaignBrokenReference> brokenReferences(const Campaign &campaign)
{
    std::vector<CampaignBrokenReference> diagnostics;

    for (const CharacterModel &character : campaign.characters)
    {
        for (const InventoryEntryModel &entry : character.inventory.entries)
            addBrokenReference(diagnostics, campaign, entry.itemId, character.metadata.id, "Player", characterName(character), "inventory item");
        for (const CharacterRelationshipModel &relationship : character.relationships)
            addBrokenReference(diagnostics, campaign, relationship.targetEntityId, character.metadata.id, "Player", characterName(character), relationship.label.empty() ? "relationship" : relationship.label);
        for (const CharacterSpellEntryModel &spell : character.spellcasting.spells)
            addBrokenReference(diagnostics, campaign, spell.spellId, character.metadata.id, "Player", characterName(character), "known spell");
    }

    for (const NPCModel &npc : campaign.npcs)
    {
        addBrokenReference(diagnostics, campaign, npc.factionId, npc.metadata.id, "NPC", fallback(npc.name, "Unnamed NPC"), "faction");
        addBrokenReference(diagnostics, campaign, npc.deityId, npc.metadata.id, "NPC", fallback(npc.name, "Unnamed NPC"), "deity");
        for (const InventoryEntryModel &entry : npc.inventory.entries)
            addBrokenReference(diagnostics, campaign, entry.itemId, npc.metadata.id, "NPC", fallback(npc.name, "Unnamed NPC"), "inventory item");
        for (const CharacterSpellEntryModel &spell : npc.spellcasting.spells)
            addBrokenReference(diagnostics, campaign, spell.spellId, npc.metadata.id, "NPC", fallback(npc.name, "Unnamed NPC"), "known spell");
    }

    for (const NationModel &nation : campaign.nations)
    {
        addBrokenReference(diagnostics, campaign, nation.capitalPlaceId, nation.metadata.id, "Nation", fallback(nation.name, "Unnamed nation"), "capital");
        for (const EntityId &factionId : nation.factionIds)
            addBrokenReference(diagnostics, campaign, factionId, nation.metadata.id, "Nation", fallback(nation.name, "Unnamed nation"), "linked faction");
    }

    for (const WorldPlaceModel &place : campaign.worldPlaces)
    {
        addBrokenReference(diagnostics, campaign, place.nationId, place.metadata.id, "World Place", fallback(place.name, "Unnamed place"), "nation");
        addBrokenReference(diagnostics, campaign, place.linkedMapId, place.metadata.id, "World Place", fallback(place.name, "Unnamed place"), "linked map");
        addBrokenReference(diagnostics, campaign, place.linkedFeatureId, place.metadata.id, "World Place", fallback(place.name, "Unnamed place"), "linked map feature");
    }

    for (const ItemModel &item : campaign.customItems)
        addBrokenReference(diagnostics, campaign, item.linkedSpellId, item.metadata.id, "Item", fallback(item.name, "Unnamed item"), "linked spell");

    for (const ShopModel &shop : campaign.shops)
    {
        addBrokenReference(diagnostics, campaign, shop.inventoryId, shop.metadata.id, "Shop", fallback(shop.name, "Unnamed shop"), "shop inventory");
        for (const ShopStockEntryModel &stock : shop.stock)
            addBrokenReference(diagnostics, campaign, stock.itemId, shop.metadata.id, "Shop", fallback(shop.name, "Unnamed shop"), "stock item");
    }

    for (const LootParcelModel &parcel : campaign.lootParcels)
    {
        for (const LootParcelItemModel &item : parcel.items)
            addBrokenReference(diagnostics, campaign, item.itemId, parcel.metadata.id, "Loot", fallback(parcel.name, "Loot parcel"), "loot item");
    }

    for (const MapModel &map : campaign.maps)
    {
        addBrokenReference(diagnostics, campaign, map.parentMapId, map.metadata.id, "Map", fallback(map.name, "Unnamed map"), "parent map");
        for (const auto &fact : map.generation.recipe.facts)
            if (fact.sourceText == "map_setup:existing")
                for (std::size_t index = 0; index < std::min<std::size_t>(2, fact.entityRefs.size()); ++index)
                    addBrokenReference(diagnostics, campaign, fact.entityRefs[index], map.metadata.id, "Map", fallback(map.name, "Unnamed map"), "generated place or nation");
        for (const MapTokenModel &token : map.tokens)
            addBrokenReference(diagnostics, campaign, token.linkedEntityId, map.metadata.id, "Map", fallback(map.name, "Unnamed map"), "map token: " + fallback(token.name, "token"));
        for (const MapFeatureModel &feature : map.features)
        {
            addBrokenReference(diagnostics, campaign, feature.linkedEntityId, map.metadata.id, "Map", fallback(map.name, "Unnamed map"), "map feature: " + fallback(feature.name, "feature"));
            addBrokenReference(diagnostics, campaign, feature.targetMapId, map.metadata.id, "Map", fallback(map.name, "Unnamed map"), "map transition");
        }
        for (const MapRouteModel &route : map.routes)
        {
            addBrokenReference(diagnostics, campaign, route.fromMapId, map.metadata.id, "Map", fallback(map.name, "Unnamed map"), "route origin");
            addBrokenReference(diagnostics, campaign, route.toMapId, map.metadata.id, "Map", fallback(map.name, "Unnamed map"), "route destination");
        }
    }

    for (const DungeonModel &dungeon : campaign.dungeons)
    {
        addBrokenReference(diagnostics, campaign, dungeon.parentMapId, dungeon.metadata.id, "Dungeon", fallback(dungeon.name, "Unnamed dungeon"), "parent map");
        addBrokenReference(diagnostics, campaign, dungeon.activeRoomId, dungeon.metadata.id, "Dungeon", fallback(dungeon.name, "Unnamed dungeon"), "active room");
        for (const EntityId &mapId : dungeon.mapIds)
            addBrokenReference(diagnostics, campaign, mapId, dungeon.metadata.id, "Dungeon", fallback(dungeon.name, "Unnamed dungeon"), "dungeon map");
        for (const DungeonRoomModel &room : dungeon.rooms)
        {
            for (const EntityId &encounterId : room.encounterIds)
                addBrokenReference(diagnostics, campaign, encounterId, room.metadata.id, "Dungeon Room", fallback(room.name, "Unnamed room"), "room encounter");
            for (const EntityId &lootId : room.lootItemIds)
                addBrokenReference(diagnostics, campaign, lootId, room.metadata.id, "Dungeon Room", fallback(room.name, "Unnamed room"), "room loot");
            for (const DungeonRoomConnectionModel &connection : room.connections)
                addBrokenReference(diagnostics, campaign, connection.targetRoomId, room.metadata.id, "Dungeon Room", fallback(room.name, "Unnamed room"), "room connection");
        }
    }

    for (const EncounterModel &encounter : campaign.encounters)
    {
        addBrokenReference(diagnostics, campaign, encounter.mapId, encounter.metadata.id, "Encounter", fallback(encounter.name, "Unnamed encounter"), "encounter map");
        for (const EncounterParticipantModel &participant : encounter.participants)
        {
            addBrokenReference(diagnostics, campaign, participant.sourceEntityId, encounter.metadata.id, "Encounter", fallback(encounter.name, "Unnamed encounter"), "combat participant");
            addBrokenReference(diagnostics, campaign, participant.monsterDefinitionId, encounter.metadata.id, "Encounter", fallback(encounter.name, "Unnamed encounter"), "monster definition");
        }
    }

    for (const SessionModel &session : campaign.sessions)
    {
        for (const EntityId &characterId : session.participantCharacterIds)
            addBrokenReference(diagnostics, campaign, characterId, session.metadata.id, "Session", fallback(session.title, "Untitled session"), "session participant");
        for (const EntityId &encounterId : session.encounterIds)
            addBrokenReference(diagnostics, campaign, encounterId, session.metadata.id, "Session", fallback(session.title, "Untitled session"), "session encounter");
        for (const EntityId &storyId : session.storyDocumentIds)
            addBrokenReference(diagnostics, campaign, storyId, session.metadata.id, "Session", fallback(session.title, "Untitled session"), "session story");
        for (const SessionPlanEntry &entry : session.planEntries)
            addBrokenReference(diagnostics, campaign, entry.storyBeatId, session.metadata.id, "Session", fallback(session.title, "Untitled session"), "planned scene");
        addBrokenReference(diagnostics, campaign, session.liveState.activeEncounterId, session.metadata.id, "Session", fallback(session.title, "Untitled session"), "active encounter");
        addBrokenReference(diagnostics, campaign, session.liveState.activePlanEntryId, session.metadata.id, "Session", fallback(session.title, "Untitled session"), "active plan entry");
        for (const SessionNoteModel &note : session.notes)
            addBrokenReference(diagnostics, campaign, note.linkedEntityId, session.metadata.id, "Session", fallback(session.title, "Untitled session"), "session note");
    }

    for (const StoryThreadModel &thread : campaign.storyThreads)
    {
        for (const StoryLinkModel &link : thread.links)
            addBrokenReference(diagnostics, campaign, link.targetEntityId, thread.metadata.id, "Story Arc", fallback(thread.title, "Untitled story"), fallback(link.label, "story link"));
        for (const StoryBeatModel &beat : thread.beats)
        {
            addBrokenReference(diagnostics, campaign, beat.cueAudioAssetId, beat.metadata.id, "Story Beat", fallback(beat.title, "Untitled beat"), "audio cue");
            for (const StoryLinkModel &link : beat.links)
                addBrokenReference(diagnostics, campaign, link.targetEntityId, beat.metadata.id, "Story Beat", fallback(beat.title, "Untitled beat"), fallback(link.label, "story link"));
        }
    }

    for (const TimelineEventModel &event : campaign.timelineEvents)
    {
        for (const EntityId &relatedId : event.relatedEntityIds)
            addBrokenReference(diagnostics, campaign, relatedId, event.metadata.id, "Timeline", fallback(event.title, "Timeline event"), "related entity");
    }

    for (const RandomTableModel &table : campaign.randomTables)
        for (const RandomTableEntryModel &entry : table.entries)
            addBrokenReference(diagnostics, campaign, entry.linkedEntityId, table.metadata.id, "Random Table",
                               fallback(table.name, "Unnamed random table"), fallback(entry.result, "table result"));

    for (const AudioTriggerModel &trigger : campaign.audioTriggers)
    {
        addBrokenReference(diagnostics, campaign, trigger.sourceEntityId, trigger.metadata.id, "Audio Trigger", fallback(trigger.name, "Audio trigger"), "trigger source");
        addBrokenReference(diagnostics, campaign, trigger.audioAssetId, trigger.metadata.id, "Audio Trigger", fallback(trigger.name, "Audio trigger"), "trigger audio");
    }

    std::sort(diagnostics.begin(), diagnostics.end(), [](const CampaignBrokenReference &left, const CampaignBrokenReference &right) {
        if (left.sourceType == right.sourceType)
            return left.sourceTitle < right.sourceTitle;
        return left.sourceType < right.sourceType;
    });
    for (const auto &link : WorldContextLinks::links(campaign))
        addBrokenReference(diagnostics, campaign, link.targetId, link.sourceId, link.sourceType, link.sourceName,
            std::string("Context: ") + WorldContextLinks::targetType(link.kind));
    return diagnostics;
}
}
