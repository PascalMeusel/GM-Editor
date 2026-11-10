#pragma once

#include "Models/Project/ProjectPackageImportCloneBasics.hpp"
#include "Models/Session/SessionOutcomeService.hpp"

namespace
{
void remapCampaignAsClone(Campaign &campaign, const std::string &cloneName)
{
    IdRemap ids;
    SessionOutcomes::assignCloneIds(campaign, ids);
    std::vector<std::array<EntityId, 3>> generatedTerritories;
    for (const auto &nation : campaign.nations)
        for (const auto &map : campaign.maps)
        {
            const EntityId territoryId = nation.metadata.id + "/map/" + map.metadata.id;
            if (std::any_of(nation.territories.begin(), nation.territories.end(),
                [&](const auto &territory) { return territory.metadata.id == territoryId; }))
                generatedTerritories.push_back({territoryId, nation.metadata.id, map.metadata.id});
        }
    assignCloneId(campaign.metadata, "campaign", ids);
    campaign.id = -1;
    campaign.name = cloneName;

    for (CharacterModel &character : campaign.characters)
    {
        assignCloneId(character.metadata, "character", ids);
        for (CharacterRelationshipModel &relationship : character.relationships)
            assignCloneId(relationship.metadata, "character_relationship", ids);
        assignInventoryCloneIds(character.inventory, ids);
        assignResourceCloneIds(character.resourceTrackers, ids);
        assignActionCloneIds(character.featureActions, ids);
    }
    for (NPCModel &npc : campaign.npcs)
    {
        assignCloneId(npc.metadata, "npc", ids);
        assignInventoryCloneIds(npc.inventory, ids);
        assignResourceCloneIds(npc.resourceTrackers, ids);
    }
    for (FactionModel &faction : campaign.factions)
        assignCloneId(faction.metadata, "faction", ids);
    for (DeityModel &deity : campaign.deities)
        assignCloneId(deity.metadata, "deity", ids);
    for (NationModel &nation : campaign.nations)
    {
        assignCloneId(nation.metadata, "nation", ids);
        for (NationTerritoryModel &territory : nation.territories)
            assignCloneId(territory.metadata, "nation_territory", ids);
    }
    for (WorldPlaceModel &place : campaign.worldPlaces)
        assignCloneId(place.metadata, "world_place", ids);
    for (ItemModel &item : campaign.customItems)
        assignCloneId(item.metadata, "item", ids);
    for (SpellModel &spell : campaign.customSpells)
        assignCloneId(spell.metadata, "spell", ids);
    for (RandomTableModel &table : campaign.randomTables)
    {
        assignCloneId(table.metadata, "random_table", ids);
        for (RandomTableEntryModel &entry : table.entries)
            assignCloneId(entry.metadata, "random_table_entry", ids);
    }
    for (MonsterDefinitionModel &monster : campaign.customMonsters)
        assignCloneId(monster.metadata, "monster", ids);
    for (MapModel &map : campaign.maps)
    {
        assignCloneId(map.metadata, "map", ids);
        for (MapLayerModel &layer : map.layers)
            assignCloneId(layer.metadata, "map_layer", ids);
        for (MapTokenModel &token : map.tokens)
            assignCloneId(token.metadata, "token", ids);
        for (MapRegionModel &region : map.regions)
            assignCloneId(region.metadata, "map_region", ids);
        for (MapFeatureModel &feature : map.features)
            assignCloneId(feature.metadata, "map_feature", ids);
        for (MapRouteModel &route : map.routes)
            assignCloneId(route.metadata, "map_route", ids);
    }
    for (EncounterModel &encounter : campaign.encounters)
    {
        assignCloneId(encounter.metadata, "encounter", ids);
        for (EncounterParticipantModel &participant : encounter.participants)
        {
            assignCloneId(participant.metadata, "combatant", ids);
            for (ActiveEffectModel &effect : participant.activeEffects)
                assignCloneId(effect.metadata, "active_effect", ids);
        }
        for (CombatLogEntryModel &entry : encounter.combatLog)
            assignCloneId(entry.metadata, "combat_log", ids);
        for (DiceRollPresetModel &preset : encounter.diceRollPresets)
            assignCloneId(preset.metadata, "dice_roll_preset", ids);
    }
    for (AudioAssetModel &asset : campaign.audioAssets)
        assignCloneId(asset.metadata, "audio", ids);
    for (AudioPlaylistModel &playlist : campaign.audioPlaylists)
        assignCloneId(playlist.metadata, "playlist", ids);
    for (AudioAmbienceLayerModel &layer : campaign.audioAmbienceLayers)
        assignCloneId(layer.metadata, "ambience_layer", ids);
    for (AudioTriggerModel &trigger : campaign.audioTriggers)
        assignCloneId(trigger.metadata, "audio_trigger", ids);
    for (ShopModel &shop : campaign.shops)
        assignCloneId(shop.metadata, "shop", ids);
    for (LootParcelModel &loot : campaign.lootParcels)
        assignCloneId(loot.metadata, "loot_parcel", ids);
    for (DungeonModel &dungeon : campaign.dungeons)
    {
        assignCloneId(dungeon.metadata, "dungeon", ids);
        for (DungeonRoomModel &room : dungeon.rooms)
        {
            assignCloneId(room.metadata, "room", ids);
            for (DungeonRoomConnectionModel &connection : room.connections)
                assignCloneId(connection.metadata, "room_connection", ids);
        }
    }
    for (HandoutModel &handout : campaign.handouts)
        assignCloneId(handout.metadata, "handout", ids);
    for (SessionModel &session : campaign.sessions)
    {
        assignCloneId(session.metadata, "session", ids);
        for (SessionPlanEntry &entry : session.planEntries)
            assignCloneId(entry.metadata, "session_plan_entry", ids);
        for (SessionNoteModel &note : session.notes)
            assignCloneId(note.metadata, "session_note", ids);
    }
    for (StoryThreadModel &thread : campaign.storyThreads)
    {
        assignCloneId(thread.metadata, "story_thread", ids);
        for (StoryLinkModel &link : thread.links)
            assignCloneId(link.metadata, "story_link", ids);
        for (StoryBeatModel &beat : thread.beats)
        {
            assignCloneId(beat.metadata, "story_beat", ids);
            for (StoryLinkModel &link : beat.links)
                assignCloneId(link.metadata, "story_link", ids);
        }
    }
    for (TimelineEventModel &event : campaign.timelineEvents)
        assignCloneId(event.metadata, "timeline", ids);
    for (RulesetDefinitionModel &definition : campaign.rulesetDefinitions)
        assignCloneId(definition.metadata, "ruleset_definition", ids);
    for (RulesetContentPackModel &pack : campaign.rulesetContentPacks)
        assignCloneId(pack.metadata, "ruleset_content_pack", ids);
    for (HouseRuleModel &rule : campaign.houseRules)
        assignCloneId(rule.metadata, "house_rule", ids);
    for (CampaignSavedFilterModel &filter : campaign.savedFilters)
        assignCloneId(filter.metadata, "saved_filter", ids);
    for (CampaignEntityBookmarkModel &bookmark : campaign.entityBookmarks)
        assignCloneId(bookmark.metadata, "entity_bookmark", ids);
    for (CampaignDiceMacroModel &macro : campaign.diceMacros)
        assignCloneId(macro.metadata, "dice_macro", ids);
    for (StoryObjectiveModel &objective : campaign.storyObjectives)
        assignCloneId(objective.metadata, "story_objective", ids);
    for (StoryClueModel &clue : campaign.storyClues)
        assignCloneId(clue.metadata, "story_clue", ids);
    for (SafetyBoundaryModel &boundary : campaign.safetyProfile.boundaries)
        assignCloneId(boundary.metadata, "safety_boundary", ids);
    for (SafetyContentWarningModel &warning : campaign.safetyProfile.warnings)
        assignCloneId(warning.metadata, "safety_warning", ids);
    assignCloneId(campaign.localPlayerDisplay.metadata, "local_player_display", ids);
    assignCloneId(campaign.partyTravel.metadata, "party_travel", ids);
    assignInventoryCloneIds(campaign.partyInventory, ids);
    for (const auto &binding : generatedTerritories)
    {
        const auto oldTerritory = ids.find(binding[0]);
        const auto nationId = ids.find(binding[1]);
        const auto mapId = ids.find(binding[2]);
        if (oldTerritory == ids.end() || nationId == ids.end() || mapId == ids.end()) continue;
        const EntityId generatedId = nationId->second + "/map/" + mapId->second;
        for (auto &nation : campaign.nations)
            for (auto &territory : nation.territories)
                if (territory.metadata.id == oldTerritory->second) territory.metadata.id = generatedId;
        oldTerritory->second = generatedId;
    }

    for (CharacterModel &character : campaign.characters)
    {
        for (CharacterRelationshipModel &relationship : character.relationships)
            rewriteId(relationship.targetEntityId, ids);
        rewriteId(character.portraitAssetId, ids);
        rewriteIds(character.inventoryIds, ids);
        rewriteInventoryIds(character.inventory, ids);
        for (CharacterSpellEntryModel &spell : character.spellcasting.spells)
            rewriteId(spell.spellId, ids);
        rewriteActionIds(character.featureActions, ids);
        rewriteResourceIds(character.resourceTrackers, ids);
    }
    for (NPCModel &npc : campaign.npcs)
    {
        rewriteId(npc.factionId, ids);
        rewriteId(npc.deityId, ids);
        rewriteIds(npc.inventoryIds, ids);
        rewriteInventoryIds(npc.inventory, ids);
        for (CharacterSpellEntryModel &spell : npc.spellcasting.spells)
            rewriteId(spell.spellId, ids);
        rewriteResourceIds(npc.resourceTrackers, ids);
    }
    for (FactionModel &faction : campaign.factions)
        rewriteId(faction.leaderNpcId, ids);
    for (NationModel &nation : campaign.nations)
    {
        rewriteId(nation.capitalPlaceId, ids);
        rewriteIds(nation.factionIds, ids);
    }
    for (WorldPlaceModel &place : campaign.worldPlaces)
    {
        rewriteId(place.nationId, ids);
        rewriteId(place.linkedMapId, ids);
        rewriteId(place.linkedFeatureId, ids);
    }
    for (ItemModel &item : campaign.customItems)
    {
        rewriteId(item.linkedSpellId, ids);
        rewriteActionIds(item.powers, ids);
    }
    for (RandomTableModel &table : campaign.randomTables)
        for (RandomTableEntryModel &entry : table.entries)
            rewriteId(entry.linkedEntityId, ids);
    for (MonsterDefinitionModel &monster : campaign.customMonsters)
    {
        rewriteId(monster.catalogSourceId, ids);
        rewriteId(monster.baseCatalogMonsterId, ids);
        rewriteActionIds(monster.actions, ids);
    }
    for (MapModel &map : campaign.maps)
    {
        rewriteId(map.parentMapId, ids);
        rewriteId(map.atlasPlacement.parentId, ids);
        for (auto &fact : map.generation.recipe.facts)
            if (fact.sourceText == "map_setup:new" || fact.sourceText == "map_setup:existing")
                for (std::size_t index = 0; index < std::min<std::size_t>(2, fact.entityRefs.size()); ++index)
                    rewriteId(fact.entityRefs[index], ids); // Slot 2 is map-local geometry, preserved by cloning.
        for (MapGeneratedFeature &generatedFeature : map.generation.graph.features)
            rewriteId(generatedFeature.lineage.sourceMapId, ids);
        for (MapTokenModel &token : map.tokens)
        {
            rewriteId(token.linkedEntityId, ids);
            rewriteId(token.assetId, ids);
            rewriteId(token.layerId, ids);
        }
        for (MapFeatureModel &feature : map.features)
        {
            rewriteId(feature.linkedEntityId, ids);
            rewriteId(feature.targetMapId, ids);
        }
        for (MapRouteModel &route : map.routes)
        {
            rewriteId(route.fromMapId, ids);
            rewriteId(route.toMapId, ids);
        }
    }
    for (EncounterModel &encounter : campaign.encounters)
    {
        rewriteId(encounter.mapId, ids);
        rewriteId(encounter.sourceMapId, ids);
        if (encounter.tacticalMapId)
            rewriteId(*encounter.tacticalMapId, ids);
        for (EncounterParticipantModel &participant : encounter.participants)
        {
            rewriteId(participant.sourceEntityId, ids);
            rewriteId(participant.monsterDefinitionId, ids);
            for (ActiveEffectModel &effect : participant.activeEffects)
            {
                rewriteId(effect.sourceEntityId, ids);
                rewriteId(effect.sourceSpellId, ids);
            }
            rewriteActionIds(participant.actions, ids);
        }
    }
    for (AudioPlaylistModel &playlist : campaign.audioPlaylists)
        rewriteIds(playlist.audioAssetIds, ids);
    for (AudioAmbienceLayerModel &layer : campaign.audioAmbienceLayers)
        rewriteId(layer.audioAssetId, ids);
    for (AudioTriggerModel &trigger : campaign.audioTriggers)
    {
        rewriteId(trigger.sourceEntityId, ids);
        rewriteId(trigger.audioAssetId, ids);
    }
    for (ShopModel &shop : campaign.shops)
    {
        rewriteId(shop.inventoryId, ids);
        for (ShopStockEntryModel &stock : shop.stock)
            rewriteId(stock.itemId, ids);
    }
    for (LootParcelModel &loot : campaign.lootParcels)
        for (LootParcelItemModel &item : loot.items)
            rewriteId(item.itemId, ids);
    for (DungeonModel &dungeon : campaign.dungeons)
    {
        rewriteId(dungeon.parentMapId, ids);
        rewriteId(dungeon.activeRoomId, ids);
        rewriteIds(dungeon.mapIds, ids);
        for (DungeonRoomModel &room : dungeon.rooms)
        {
            rewriteIds(room.encounterIds, ids);
            rewriteIds(room.lootItemIds, ids);
            for (DungeonRoomConnectionModel &connection : room.connections)
                rewriteId(connection.targetRoomId, ids);
        }
    }
    SessionOutcomes::remapCloneReferences(campaign, ids);
    for (SessionModel &session : campaign.sessions)
    {
        rewriteIds(session.participantCharacterIds, ids);
        rewriteIds(session.encounterIds, ids);
        rewriteIds(session.storyDocumentIds, ids);
        for (SessionPlanEntry &entry : session.planEntries)
            rewriteId(entry.storyBeatId, ids);
        rewriteId(session.liveState.activePlanEntryId, ids);
        rewriteId(session.liveState.activeEncounterId, ids);
        for (SessionNoteModel &note : session.notes)
            rewriteId(note.linkedEntityId, ids);
    }
    for (StoryThreadModel &thread : campaign.storyThreads)
    {
        for (StoryLinkModel &link : thread.links)
            rewriteId(link.targetEntityId, ids);
        for (StoryBeatModel &beat : thread.beats)
        {
            rewriteId(beat.cueAudioAssetId, ids);
            for (StoryLinkModel &link : beat.links)
                rewriteId(link.targetEntityId, ids);
        }
    }
    for (TimelineEventModel &event : campaign.timelineEvents)
        rewriteIds(event.relatedEntityIds, ids);
    for (CampaignEntityBookmarkModel &bookmark : campaign.entityBookmarks)
        rewriteId(bookmark.entityId, ids);
    for (StoryObjectiveModel &objective : campaign.storyObjectives)
    {
        rewriteId(objective.linkedStoryThreadId, ids);
        rewriteId(objective.linkedStoryBeatId, ids);
        rewriteIds(objective.assigneeCharacterIds, ids);
        rewriteIds(objective.prerequisiteObjectiveIds, ids);
    }
    for (StoryClueModel &clue : campaign.storyClues)
        rewriteId(clue.sourceEntityId, ids);
    for (SafetyContentWarningModel &warning : campaign.safetyProfile.warnings)
        rewriteId(warning.targetEntityId, ids);
    rewriteId(campaign.localPlayerDisplay.publicSceneId, ids);
    rewriteId(campaign.localPlayerDisplay.publicMapId, ids);
    rewriteId(campaign.localPlayerDisplay.publicEncounterId, ids);
    rewriteIds(campaign.localPlayerDisplay.visibleNpcIds, ids);
    rewriteIds(campaign.localPlayerDisplay.visibleCharacterIds, ids);
    rewriteIds(campaign.localPlayerDisplay.publicHandoutIds, ids);
    rewriteIds(campaign.localPlayerDisplay.sharedArtAssetIds, ids);
    rewriteId(campaign.partyLocation.mapId, ids);
    rewriteId(campaign.partyTravel.currentMapId, ids);
    rewriteId(campaign.partyTravel.routeId, ids);
    rewriteInventoryIds(campaign.partyInventory, ids);
    rewriteIds(campaign.npcIds, ids);
    rewriteIds(campaign.itemIds, ids);
    rewriteIds(campaign.inventoryIds, ids);
    rewriteIds(campaign.shopIds, ids);
    rewriteIds(campaign.mapIds, ids);
    rewriteIds(campaign.dungeonIds, ids);
    rewriteIds(campaign.encounterIds, ids);
    rewriteIds(campaign.audioAssetIds, ids);
    rewriteIds(campaign.audioPlaylistIds, ids);
    rewriteIds(campaign.audioAmbienceLayerIds, ids);
    rewriteIds(campaign.audioTriggerIds, ids);
    rewriteIds(campaign.storyDocumentIds, ids);
    rewriteIds(campaign.sessionIds, ids);
    WorldContextLinks::remapTargets(campaign, [&](EntityId target) { rewriteId(target, ids); return target; });
}
} // namespace
