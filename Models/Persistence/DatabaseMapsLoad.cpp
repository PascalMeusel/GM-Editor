#include "Models/DataBaseManager.hpp"
#include <string>
#include "Models/Session/SessionPlanService.hpp"
#include "Models/Persistence/PersistenceSerialization.hpp"
#include "Models/Map/MapAtlasService.hpp"
#include "Models/Project/ApplicationPaths.hpp"
#include "Models/Project/ProjectLogger.hpp"
#include "Models/Rulesets/RulesetDefinitionSerialization.hpp"
#include "Models/Rulesets/DefinitionBackedRuleSet.hpp"
#include "Models/Session/InWorldTimeService.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <unordered_set>
#include "Models/Rulesets/Clockwork/ClockworkBloodline.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "Models/Persistence/DatabaseRows.hpp"

using namespace PersistenceSerialization;

void DatabaseManager::loadCampaignMaps(Campaign &campaign)
{
    SQLite::Statement query_maps(*m_db, R"(
                    SELECT id, stable_id, name, description, scope, grid_type, width, height, cell_size, is_main_map, parent_map_id, background_image_path, background_image_opacity, regions, features, routes, generation_data, cells, tags, created_at, updated_at, atlas_data
                    FROM maps
                    WHERE campaign_id = ?
                    ORDER BY is_main_map DESC, name ASC;
                )");
    query_maps.bind(1, static_cast<int64_t>(campaign.id));

    while (query_maps.executeStep())
    {
        MapModel map;
        const int64_t mapRowId = query_maps.getColumn(0).getInt64();
        if (!query_maps.getColumn(1).isNull())
            map.metadata.id = query_maps.getColumn(1).getString();
        map.name = query_maps.getColumn(2).getString();
        if (!query_maps.getColumn(3).isNull())
            map.description = query_maps.getColumn(3).getString();
        if (!query_maps.getColumn(4).isNull())
            map.scope = static_cast<MapScope>(query_maps.getColumn(4).getInt());
        if (!query_maps.getColumn(5).isNull())
            map.gridType = static_cast<GridType>(query_maps.getColumn(5).getInt());
        if (!query_maps.getColumn(6).isNull())
            map.width = query_maps.getColumn(6).getInt();
        if (!query_maps.getColumn(7).isNull())
            map.height = query_maps.getColumn(7).getInt();
        if (!query_maps.getColumn(8).isNull())
            map.cellSize = static_cast<float>(query_maps.getColumn(8).getDouble());
        if (!query_maps.getColumn(9).isNull())
            map.isMainMap = query_maps.getColumn(9).getInt() != 0;
        if (!query_maps.getColumn(10).isNull())
            map.parentMapId = query_maps.getColumn(10).getString();
        if (!query_maps.getColumn(11).isNull())
            map.backgroundImagePath = query_maps.getColumn(11).getString();
        if (!query_maps.getColumn(12).isNull())
            map.backgroundImageOpacity = static_cast<float>(query_maps.getColumn(12).getDouble());
        if (!query_maps.getColumn(13).isNull())
            map.regions = deserializeMapRegions(query_maps.getColumn(13).getString());
        if (!query_maps.getColumn(14).isNull())
            map.features = deserializeMapFeatures(query_maps.getColumn(14).getString());
        if (!query_maps.getColumn(15).isNull())
            map.routes = deserializeMapRoutes(query_maps.getColumn(15).getString());
        const std::string serializedGeneration =
            query_maps.getColumn(16).isNull() ? std::string{} : query_maps.getColumn(16).getString();
        if (!query_maps.getColumn(17).isNull())
            map.cells = deserializeMapCells(query_maps.getColumn(17).getString());
        if (!query_maps.getColumn(18).isNull())
            map.metadata.tags = deserializeStringList(query_maps.getColumn(18).getString());
        if (!query_maps.getColumn(19).isNull())
            map.metadata.createdAtUtc = query_maps.getColumn(19).getString();
        if (!query_maps.getColumn(20).isNull())
            map.metadata.updatedAtUtc = query_maps.getColumn(20).getString();
        const std::string serializedAtlas =
            query_maps.getColumn(21).isNull() ? std::string{} : query_maps.getColumn(21).getString();
        deserializeMapPersistenceMetadata(serializedGeneration, serializedAtlas, map);

        SQLite::Statement query_layers(*m_db, R"(
                        SELECT stable_id, name, visible, locked, sort_order, tags, created_at, updated_at
                        FROM map_layers
                        WHERE map_id = ?
                        ORDER BY sort_order ASC, name ASC;
                    )");
        query_layers.bind(1, mapRowId);
        while (query_layers.executeStep())
        {
            MapLayerModel layer;
            if (!query_layers.getColumn(0).isNull())
                layer.metadata.id = query_layers.getColumn(0).getString();
            if (!query_layers.getColumn(1).isNull())
                layer.name = query_layers.getColumn(1).getString();
            if (!query_layers.getColumn(2).isNull())
                layer.visible = query_layers.getColumn(2).getInt() != 0;
            if (!query_layers.getColumn(3).isNull())
                layer.locked = query_layers.getColumn(3).getInt() != 0;
            if (!query_layers.getColumn(4).isNull())
                layer.sortOrder = query_layers.getColumn(4).getInt();
            if (!query_layers.getColumn(5).isNull())
                layer.metadata.tags = deserializeStringList(query_layers.getColumn(5).getString());
            if (!query_layers.getColumn(6).isNull())
                layer.metadata.createdAtUtc = query_layers.getColumn(6).getString();
            if (!query_layers.getColumn(7).isNull())
                layer.metadata.updatedAtUtc = query_layers.getColumn(7).getString();
            map.layers.push_back(layer);
        }

        SQLite::Statement query_tokens(*m_db, R"(
                        SELECT stable_id, name, kind, linked_entity_type, linked_entity_id, asset_id, layer_id, x, y, rotation, size, hidden_from_players, tags, created_at, updated_at
                        FROM map_tokens
                        WHERE map_id = ?
                        ORDER BY name ASC;
                    )");
        query_tokens.bind(1, mapRowId);
        while (query_tokens.executeStep())
        {
            MapTokenModel token;
            if (!query_tokens.getColumn(0).isNull())
                token.metadata.id = query_tokens.getColumn(0).getString();
            if (!query_tokens.getColumn(1).isNull())
                token.name = query_tokens.getColumn(1).getString();
            if (!query_tokens.getColumn(2).isNull())
                token.kind = static_cast<MapTokenKind>(
                    std::clamp(query_tokens.getColumn(2).getInt(), 0, static_cast<int>(MapTokenKind::Party)));
            if (!query_tokens.getColumn(3).isNull())
                token.linkedEntityType = query_tokens.getColumn(3).getString();
            if (!query_tokens.getColumn(4).isNull())
                token.linkedEntityId = query_tokens.getColumn(4).getString();
            if (!query_tokens.getColumn(5).isNull())
                token.assetId = query_tokens.getColumn(5).getString();
            if (!query_tokens.getColumn(6).isNull())
                token.layerId = query_tokens.getColumn(6).getString();
            if (!query_tokens.getColumn(7).isNull())
                token.x = static_cast<float>(query_tokens.getColumn(7).getDouble());
            if (!query_tokens.getColumn(8).isNull())
                token.y = static_cast<float>(query_tokens.getColumn(8).getDouble());
            if (!query_tokens.getColumn(9).isNull())
                token.rotation = static_cast<float>(query_tokens.getColumn(9).getDouble());
            if (!query_tokens.getColumn(10).isNull())
                token.size = static_cast<float>(query_tokens.getColumn(10).getDouble());
            if (!query_tokens.getColumn(11).isNull())
                token.hiddenFromPlayers = query_tokens.getColumn(11).getInt() != 0;
            if (!query_tokens.getColumn(12).isNull())
                token.metadata.tags = deserializeStringList(query_tokens.getColumn(12).getString());
            if (!query_tokens.getColumn(13).isNull())
                token.metadata.createdAtUtc = query_tokens.getColumn(13).getString();
            if (!query_tokens.getColumn(14).isNull())
                token.metadata.updatedAtUtc = query_tokens.getColumn(14).getString();
            map.tokens.push_back(token);
        }

        resizeMapCells(map);
        ensureDefaultMapLayer(map);
        campaign.maps.push_back(map);
    }
}

void DatabaseManager::loadCampaignDungeons(Campaign &campaign)
{
    SQLite::Statement query_dungeons(*m_db, R"(
                    SELECT id, stable_id, name, description, parent_map_id, active_room_id, gm_notes, map_ids, tags, created_at, updated_at
                    FROM dungeons
                    WHERE campaign_id = ?
                    ORDER BY name ASC;
                )");
    query_dungeons.bind(1, static_cast<int64_t>(campaign.id));

    while (query_dungeons.executeStep())
    {
        DungeonModel dungeon;
        const int64_t dungeonRowId = query_dungeons.getColumn(0).getInt64();
        if (!query_dungeons.getColumn(1).isNull())
            dungeon.metadata.id = query_dungeons.getColumn(1).getString();
        dungeon.name = query_dungeons.getColumn(2).getString();
        if (!query_dungeons.getColumn(3).isNull())
            dungeon.description = query_dungeons.getColumn(3).getString();
        if (!query_dungeons.getColumn(4).isNull())
            dungeon.parentMapId = query_dungeons.getColumn(4).getString();
        if (!query_dungeons.getColumn(5).isNull())
            dungeon.activeRoomId = query_dungeons.getColumn(5).getString();
        if (!query_dungeons.getColumn(6).isNull())
            dungeon.gmNotes = query_dungeons.getColumn(6).getString();
        if (!query_dungeons.getColumn(7).isNull())
            dungeon.mapIds = deserializeStringList(query_dungeons.getColumn(7).getString());
        if (!query_dungeons.getColumn(8).isNull())
            dungeon.metadata.tags = deserializeStringList(query_dungeons.getColumn(8).getString());
        if (!query_dungeons.getColumn(9).isNull())
            dungeon.metadata.createdAtUtc = query_dungeons.getColumn(9).getString();
        if (!query_dungeons.getColumn(10).isNull())
            dungeon.metadata.updatedAtUtc = query_dungeons.getColumn(10).getString();

        SQLite::Statement query_rooms(*m_db, R"(
                        SELECT stable_id, name, description, keyed_description, read_aloud_text, secrets, traps, exploration_notes, exploration_state, map_x, map_y, encounter_ids, loot_item_ids, connections, tags, created_at, updated_at
                        FROM dungeon_rooms
                        WHERE dungeon_id = ?
                        ORDER BY map_y ASC, map_x ASC, name ASC;
                    )");
        query_rooms.bind(1, dungeonRowId);
        while (query_rooms.executeStep())
        {
            DungeonRoomModel room;
            if (!query_rooms.getColumn(0).isNull())
                room.metadata.id = query_rooms.getColumn(0).getString();
            room.name = query_rooms.getColumn(1).getString();
            if (!query_rooms.getColumn(2).isNull())
                room.description = query_rooms.getColumn(2).getString();
            if (!query_rooms.getColumn(3).isNull())
                room.keyedDescription = query_rooms.getColumn(3).getString();
            if (!query_rooms.getColumn(4).isNull())
                room.readAloudText = query_rooms.getColumn(4).getString();
            if (!query_rooms.getColumn(5).isNull())
                room.secrets = query_rooms.getColumn(5).getString();
            if (!query_rooms.getColumn(6).isNull())
                room.traps = query_rooms.getColumn(6).getString();
            if (!query_rooms.getColumn(7).isNull())
                room.explorationNotes = query_rooms.getColumn(7).getString();
            if (!query_rooms.getColumn(8).isNull())
                room.explorationState = static_cast<DungeonRoomExplorationState>(std::clamp(
                    query_rooms.getColumn(8).getInt(), 0, static_cast<int>(DungeonRoomExplorationState::Cleared)));
            if (!query_rooms.getColumn(9).isNull())
                room.mapX = query_rooms.getColumn(9).getInt();
            if (!query_rooms.getColumn(10).isNull())
                room.mapY = query_rooms.getColumn(10).getInt();
            if (!query_rooms.getColumn(11).isNull())
                room.encounterIds = deserializeStringList(query_rooms.getColumn(11).getString());
            if (!query_rooms.getColumn(12).isNull())
                room.lootItemIds = deserializeStringList(query_rooms.getColumn(12).getString());
            if (!query_rooms.getColumn(13).isNull())
                room.connections = deserializeDungeonRoomConnections(query_rooms.getColumn(13).getString());
            if (!query_rooms.getColumn(14).isNull())
                room.metadata.tags = deserializeStringList(query_rooms.getColumn(14).getString());
            if (!query_rooms.getColumn(15).isNull())
                room.metadata.createdAtUtc = query_rooms.getColumn(15).getString();
            if (!query_rooms.getColumn(16).isNull())
                room.metadata.updatedAtUtc = query_rooms.getColumn(16).getString();
            dungeon.rooms.push_back(room);
        }

        campaign.dungeons.push_back(dungeon);
    }
}
