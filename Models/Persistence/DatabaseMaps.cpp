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

namespace {
void validateMapIdentities(SQLite::Database &db, int64_t campaignId, const Campaign &campaign)
{
    std::unordered_set<std::string> maps, layers, tokens;
    SQLite::Statement mapOwner(db, "SELECT campaign_id FROM maps WHERE stable_id = ?;");
    SQLite::Statement layerOwner(db, "SELECT m.campaign_id FROM map_layers c JOIN maps m ON m.id=c.map_id WHERE c.stable_id = ?;");
    SQLite::Statement tokenOwner(db, "SELECT m.campaign_id FROM map_tokens c JOIN maps m ON m.id=c.map_id WHERE c.stable_id = ?;");
    const auto check = [&](const std::string &id, std::unordered_set<std::string> &seen, SQLite::Statement &owner) {
        if (!seen.insert(id).second) throw std::runtime_error("Duplicate map, layer or token identity in campaign.");
        owner.bind(1, id);
        if (owner.executeStep() && owner.getColumn(0).getInt64() != campaignId)
            throw std::runtime_error("Map, layer or token identity belongs to another campaign; create an independent copy.");
        owner.reset();
    };
    // Check before any map row moves or deletion can conceal its previous owner.
    // A unique child can move between maps of this campaign without losing its ID.
    for (const auto &map : campaign.maps) {
        check(map.metadata.id, maps, mapOwner);
        for (const auto &layer : map.layers) check(layer.metadata.id, layers, layerOwner);
        for (const auto &token : map.tokens) check(token.metadata.id, tokens, tokenOwner);
    }
}

template<class Children>
void removeMissingMapChildren(SQLite::Database &db, const char *table, int64_t mapId, const Children &children)
{
    std::unordered_set<std::string> retained;
    for (const auto &child : children) retained.insert(child.metadata.id);
    // Table names are internal constants; all data values remain bound parameters.
    SQLite::Statement rows(db, std::string("SELECT stable_id FROM ") + table + " WHERE map_id = ?;");
    rows.bind(1, mapId);
    std::vector<std::string> removed;
    while (rows.executeStep()) {
        auto id = rows.getColumn(0).getString();
        if (!retained.count(id)) removed.push_back(std::move(id));
    }
    SQLite::Statement erase(db, std::string("DELETE FROM ") + table + " WHERE map_id = ? AND stable_id = ?;");
    for (const auto &id : removed) {
        erase.bind(1, mapId); erase.bind(2, id); erase.exec(); erase.reset();
    }
}
}

void DatabaseManager::saveCampaignDungeons(int64_t campaignId, const Campaign &campaign)
{
    deleteRowsMissingStableIds("dungeons", campaignId, dungeonIds(campaign.dungeons));
    SQLite::Statement query_delete_dungeon_rooms(*m_db, R"(
            DELETE FROM dungeon_rooms
            WHERE dungeon_id IN (SELECT id FROM dungeons WHERE campaign_id = ?);
        )");
    query_delete_dungeon_rooms.bind(1, campaignId);
    query_delete_dungeon_rooms.exec();

    SQLite::Statement query_insert_dungeon(*m_db, R"(
            INSERT INTO dungeons
            (stable_id, campaign_id, name, description, parent_map_id, active_room_id, gm_notes, map_ids, tags, created_at, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(stable_id) DO UPDATE SET
                campaign_id = excluded.campaign_id,
                name = excluded.name,
                description = excluded.description,
                parent_map_id = excluded.parent_map_id,
                active_room_id = excluded.active_room_id,
                gm_notes = excluded.gm_notes,
                map_ids = excluded.map_ids,
                tags = excluded.tags,
                updated_at = excluded.updated_at;
        )");
    SQLite::Statement query_dungeon_id(*m_db, "SELECT id FROM dungeons WHERE stable_id = ? LIMIT 1;");
    SQLite::Statement query_insert_dungeon_room(*m_db, R"(
            INSERT INTO dungeon_rooms
            (stable_id, dungeon_id, name, description, keyed_description, read_aloud_text, secrets, traps, exploration_notes, exploration_state, map_x, map_y, encounter_ids, loot_item_ids, connections, tags, created_at, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
        )");
    for (const DungeonModel &dungeon : campaign.dungeons)
    {
        query_insert_dungeon.bind(1, dungeon.metadata.id);
        query_insert_dungeon.bind(2, campaignId);
        query_insert_dungeon.bind(3, dungeon.name.empty() ? "Untitled Dungeon" : dungeon.name);
        query_insert_dungeon.bind(4, dungeon.description);
        query_insert_dungeon.bind(5, dungeon.parentMapId);
        query_insert_dungeon.bind(6, dungeon.activeRoomId);
        query_insert_dungeon.bind(7, dungeon.gmNotes);
        query_insert_dungeon.bind(8, serializeStringList(dungeon.mapIds));
        query_insert_dungeon.bind(9, serializeStringList(dungeon.metadata.tags));
        query_insert_dungeon.bind(10, dungeon.metadata.createdAtUtc);
        query_insert_dungeon.bind(11, currentUtcTimestamp());
        query_insert_dungeon.exec();
        query_insert_dungeon.reset();

        query_dungeon_id.bind(1, dungeon.metadata.id);
        if (query_dungeon_id.executeStep())
        {
            const int64_t dungeonRowId = query_dungeon_id.getColumn(0).getInt64();
            for (const DungeonRoomModel &room : dungeon.rooms)
            {
                query_insert_dungeon_room.bind(1, room.metadata.id);
                query_insert_dungeon_room.bind(2, dungeonRowId);
                query_insert_dungeon_room.bind(3, room.name.empty() ? "Room" : room.name);
                query_insert_dungeon_room.bind(4, room.description);
                query_insert_dungeon_room.bind(5, room.keyedDescription);
                query_insert_dungeon_room.bind(6, room.readAloudText);
                query_insert_dungeon_room.bind(7, room.secrets);
                query_insert_dungeon_room.bind(8, room.traps);
                query_insert_dungeon_room.bind(9, room.explorationNotes);
                query_insert_dungeon_room.bind(10, static_cast<int>(room.explorationState));
                query_insert_dungeon_room.bind(11, room.mapX);
                query_insert_dungeon_room.bind(12, room.mapY);
                query_insert_dungeon_room.bind(13, serializeStringList(room.encounterIds));
                query_insert_dungeon_room.bind(14, serializeStringList(room.lootItemIds));
                query_insert_dungeon_room.bind(15, serializeDungeonRoomConnections(room.connections));
                query_insert_dungeon_room.bind(16, serializeStringList(room.metadata.tags));
                query_insert_dungeon_room.bind(17, room.metadata.createdAtUtc);
                query_insert_dungeon_room.bind(18, currentUtcTimestamp());
                query_insert_dungeon_room.exec();
                query_insert_dungeon_room.reset();
            }
        }
        query_dungeon_id.reset();
    }
}

void DatabaseManager::saveCampaignMaps(int64_t campaignId, const Campaign &campaign)
{
    validateMapIdentities(*m_db, campaignId, campaign);
    deleteRowsMissingStableIds("maps", campaignId, mapIds(campaign.maps));

    SQLite::Statement query_insert_map(*m_db, R"(
            INSERT INTO maps
            (stable_id, campaign_id, name, description, scope, grid_type, width, height, cell_size, is_main_map, parent_map_id, background_image_path, background_image_opacity, regions, features, routes, generation_data, cells, tags, created_at, updated_at, atlas_data)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(stable_id) DO UPDATE SET
                campaign_id = excluded.campaign_id,
                name = excluded.name,
                description = excluded.description,
                scope = excluded.scope,
                grid_type = excluded.grid_type,
                width = excluded.width,
                height = excluded.height,
                cell_size = excluded.cell_size,
                is_main_map = excluded.is_main_map,
                parent_map_id = excluded.parent_map_id,
                background_image_path = excluded.background_image_path,
                background_image_opacity = excluded.background_image_opacity,
                regions = excluded.regions,
                features = excluded.features,
                routes = excluded.routes,
                generation_data = excluded.generation_data,
                cells = excluded.cells,
                tags = excluded.tags,
                atlas_data = excluded.atlas_data,
                updated_at = excluded.updated_at
            WHERE maps.campaign_id IS NOT excluded.campaign_id
                OR maps.name IS NOT excluded.name
                OR maps.description IS NOT excluded.description
                OR maps.scope IS NOT excluded.scope
                OR maps.grid_type IS NOT excluded.grid_type
                OR maps.width IS NOT excluded.width
                OR maps.height IS NOT excluded.height
                OR maps.cell_size IS NOT excluded.cell_size
                OR maps.is_main_map IS NOT excluded.is_main_map
                OR maps.parent_map_id IS NOT excluded.parent_map_id
                OR maps.background_image_path IS NOT excluded.background_image_path
                OR maps.background_image_opacity IS NOT excluded.background_image_opacity
                OR maps.regions IS NOT excluded.regions
                OR maps.features IS NOT excluded.features
                OR maps.routes IS NOT excluded.routes
                OR maps.generation_data IS NOT excluded.generation_data
                OR maps.cells IS NOT excluded.cells
                OR maps.tags IS NOT excluded.tags
                OR maps.atlas_data IS NOT excluded.atlas_data;
        )");
    SQLite::Statement query_insert_map_layer(*m_db, R"(
            INSERT INTO map_layers
            (stable_id, map_id, name, visible, locked, sort_order, tags, created_at, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(stable_id) DO UPDATE SET
                map_id = excluded.map_id,
                name = excluded.name,
                visible = excluded.visible,
                locked = excluded.locked,
                sort_order = excluded.sort_order,
                tags = excluded.tags,
                updated_at = excluded.updated_at
            WHERE map_layers.map_id IS NOT excluded.map_id
                OR map_layers.name IS NOT excluded.name
                OR map_layers.visible IS NOT excluded.visible
                OR map_layers.locked IS NOT excluded.locked
                OR map_layers.sort_order IS NOT excluded.sort_order
                OR map_layers.tags IS NOT excluded.tags;
        )");
    SQLite::Statement query_insert_map_token(*m_db, R"(
            INSERT INTO map_tokens
            (stable_id, map_id, name, kind, linked_entity_type, linked_entity_id, asset_id, layer_id, x, y, rotation, size, hidden_from_players, tags, created_at, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(stable_id) DO UPDATE SET
                map_id = excluded.map_id,
                name = excluded.name,
                kind = excluded.kind,
                linked_entity_type = excluded.linked_entity_type,
                linked_entity_id = excluded.linked_entity_id,
                asset_id = excluded.asset_id,
                layer_id = excluded.layer_id,
                x = excluded.x,
                y = excluded.y,
                rotation = excluded.rotation,
                size = excluded.size,
                hidden_from_players = excluded.hidden_from_players,
                tags = excluded.tags,
                updated_at = excluded.updated_at
            WHERE map_tokens.map_id IS NOT excluded.map_id
                OR map_tokens.name IS NOT excluded.name
                OR map_tokens.kind IS NOT excluded.kind
                OR map_tokens.linked_entity_type IS NOT excluded.linked_entity_type
                OR map_tokens.linked_entity_id IS NOT excluded.linked_entity_id
                OR map_tokens.asset_id IS NOT excluded.asset_id
                OR map_tokens.layer_id IS NOT excluded.layer_id
                OR map_tokens.x IS NOT excluded.x
                OR map_tokens.y IS NOT excluded.y
                OR map_tokens.rotation IS NOT excluded.rotation
                OR map_tokens.size IS NOT excluded.size
                OR map_tokens.hidden_from_players IS NOT excluded.hidden_from_players
                OR map_tokens.tags IS NOT excluded.tags;
        )");
    for (const auto &map : campaign.maps)
    {
        MapModel normalizedMap = map;
        resizeMapCells(normalizedMap);
        ensureDefaultMapLayer(normalizedMap);
        query_insert_map.bind(1, normalizedMap.metadata.id);
        query_insert_map.bind(2, campaignId);
        query_insert_map.bind(3, normalizedMap.name);
        query_insert_map.bind(4, normalizedMap.description);
        query_insert_map.bind(5, static_cast<int>(normalizedMap.scope));
        query_insert_map.bind(6, static_cast<int>(normalizedMap.gridType));
        query_insert_map.bind(7, normalizedMap.width);
        query_insert_map.bind(8, normalizedMap.height);
        query_insert_map.bind(9, normalizedMap.cellSize);
        query_insert_map.bind(10, normalizedMap.isMainMap ? 1 : 0);
        query_insert_map.bind(11, normalizedMap.parentMapId);
        query_insert_map.bind(12, normalizedMap.backgroundImagePath);
        query_insert_map.bind(13, normalizedMap.backgroundImageOpacity);
        query_insert_map.bind(14, serializeMapRegions(normalizedMap.regions));
        query_insert_map.bind(15, serializeMapFeatures(normalizedMap.features));
        query_insert_map.bind(16, serializeMapRoutes(normalizedMap.routes));
        query_insert_map.bind(17, serializeMapGenerationMetadata(normalizedMap.generation));
        query_insert_map.bind(18, cellSerializationCache.serialize(normalizedMap.metadata.id, normalizedMap.cells));
        query_insert_map.bind(19, serializeStringList(normalizedMap.metadata.tags));
        query_insert_map.bind(20, normalizedMap.metadata.createdAtUtc);
        query_insert_map.bind(21, currentUtcTimestamp());
        query_insert_map.bind(22, serializeMapAtlasMetadata(normalizedMap));

        query_insert_map.exec();
        query_insert_map.reset();

        SQLite::Statement query_map_id(*m_db, "SELECT id FROM maps WHERE stable_id = ? LIMIT 1;");
        query_map_id.bind(1, normalizedMap.metadata.id);
        if (!query_map_id.executeStep())
            continue;
        const int64_t mapRowId = query_map_id.getColumn(0).getInt64();

        removeMissingMapChildren(*m_db, "map_layers", mapRowId, normalizedMap.layers);
        for (const MapLayerModel &layer : normalizedMap.layers)
        {
            query_insert_map_layer.bind(1, layer.metadata.id);
            query_insert_map_layer.bind(2, mapRowId);
            query_insert_map_layer.bind(3, layer.name);
            query_insert_map_layer.bind(4, layer.visible ? 1 : 0);
            query_insert_map_layer.bind(5, layer.locked ? 1 : 0);
            query_insert_map_layer.bind(6, layer.sortOrder);
            query_insert_map_layer.bind(7, serializeStringList(layer.metadata.tags));
            query_insert_map_layer.bind(8, layer.metadata.createdAtUtc);
            query_insert_map_layer.bind(9, currentUtcTimestamp());
            query_insert_map_layer.exec();
            query_insert_map_layer.reset();
        }

        removeMissingMapChildren(*m_db, "map_tokens", mapRowId, normalizedMap.tokens);
        for (const MapTokenModel &token : normalizedMap.tokens)
        {
            query_insert_map_token.bind(1, token.metadata.id);
            query_insert_map_token.bind(2, mapRowId);
            query_insert_map_token.bind(3, token.name);
            query_insert_map_token.bind(4, static_cast<int>(token.kind));
            query_insert_map_token.bind(5, token.linkedEntityType);
            query_insert_map_token.bind(6, token.linkedEntityId);
            query_insert_map_token.bind(7, token.assetId);
            query_insert_map_token.bind(8, token.layerId);
            query_insert_map_token.bind(9, token.x);
            query_insert_map_token.bind(10, token.y);
            query_insert_map_token.bind(11, token.rotation);
            query_insert_map_token.bind(12, token.size);
            query_insert_map_token.bind(13, token.hiddenFromPlayers ? 1 : 0);
            query_insert_map_token.bind(14, serializeStringList(token.metadata.tags));
            query_insert_map_token.bind(15, token.metadata.createdAtUtc);
            query_insert_map_token.bind(16, currentUtcTimestamp());
            query_insert_map_token.exec();
            query_insert_map_token.reset();
        }
    }
}
