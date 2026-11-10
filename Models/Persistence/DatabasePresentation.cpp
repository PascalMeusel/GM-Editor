#include "Models/DataBaseManager.hpp"
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

void DatabaseManager::saveCampaignPlayerDisplay(int64_t campaignId, const Campaign &campaign)
{
    const LocalPlayerDisplayModel &display = campaign.localPlayerDisplay;
    SQLite::Statement query_upsert_local_display(*m_db, R"(
            INSERT INTO local_player_display
            (campaign_id, stable_id, enabled, public_scene_id, public_map_id, public_encounter_id, show_map, show_initiative, show_visible_npcs, show_party, show_party_spell_slots, show_party_inventory, show_public_handouts, show_shared_art, visible_npc_ids, visible_character_ids, public_handout_ids, shared_art_asset_ids, map_only, preferred_monitor_index, fullscreen_map_only, camera_center_x, camera_center_y, camera_zoom, follow_public_camera, player_text_scale, tags, created_at, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(campaign_id) DO UPDATE SET
                stable_id = excluded.stable_id,
                enabled = excluded.enabled,
                public_scene_id = excluded.public_scene_id,
                public_map_id = excluded.public_map_id,
                public_encounter_id = excluded.public_encounter_id,
                show_map = excluded.show_map,
                show_initiative = excluded.show_initiative,
                show_visible_npcs = excluded.show_visible_npcs,
                show_party = excluded.show_party,
                show_party_spell_slots = excluded.show_party_spell_slots,
                show_party_inventory = excluded.show_party_inventory,
                show_public_handouts = excluded.show_public_handouts,
                show_shared_art = excluded.show_shared_art,
                visible_npc_ids = excluded.visible_npc_ids,
                visible_character_ids = excluded.visible_character_ids,
                public_handout_ids = excluded.public_handout_ids,
                shared_art_asset_ids = excluded.shared_art_asset_ids,
                map_only = excluded.map_only,
                preferred_monitor_index = excluded.preferred_monitor_index,
                fullscreen_map_only = excluded.fullscreen_map_only,
                camera_center_x = excluded.camera_center_x,
                camera_center_y = excluded.camera_center_y,
                camera_zoom = excluded.camera_zoom,
                follow_public_camera = excluded.follow_public_camera,
                player_text_scale = excluded.player_text_scale,
                tags = excluded.tags,
                updated_at = excluded.updated_at;
        )");
    query_upsert_local_display.bind(1, campaignId);
    query_upsert_local_display.bind(2, display.metadata.id);
    query_upsert_local_display.bind(3, display.enabled ? 1 : 0);
    query_upsert_local_display.bind(4, display.publicSceneId);
    query_upsert_local_display.bind(5, display.publicMapId);
    query_upsert_local_display.bind(6, display.publicEncounterId);
    query_upsert_local_display.bind(7, display.showMap ? 1 : 0);
    query_upsert_local_display.bind(8, display.showInitiative ? 1 : 0);
    query_upsert_local_display.bind(9, display.showVisibleNpcs ? 1 : 0);
    query_upsert_local_display.bind(10, display.showParty ? 1 : 0);
    query_upsert_local_display.bind(11, display.showPartySpellSlots ? 1 : 0);
    query_upsert_local_display.bind(12, display.showPartyInventory ? 1 : 0);
    query_upsert_local_display.bind(13, display.showPublicHandouts ? 1 : 0);
    query_upsert_local_display.bind(14, display.showSharedArt ? 1 : 0);
    query_upsert_local_display.bind(15, serializeStringList(display.visibleNpcIds)); query_upsert_local_display.bind(16, serializeStringList(display.visibleCharacterIds));
    query_upsert_local_display.bind(17, serializeStringList(display.publicHandoutIds)); query_upsert_local_display.bind(18, serializeStringList(display.sharedArtAssetIds));
    query_upsert_local_display.bind(19, display.mapOnly ? 1 : 0); query_upsert_local_display.bind(20, display.preferredMonitorIndex);
    query_upsert_local_display.bind(21, display.fullscreenMapOnly ? 1 : 0); query_upsert_local_display.bind(22, display.publicCamera.normalizedCenterX);
    query_upsert_local_display.bind(23, display.publicCamera.normalizedCenterY); query_upsert_local_display.bind(24, display.publicCamera.zoom);
    query_upsert_local_display.bind(25, display.publicCamera.followGmCamera ? 1 : 0); query_upsert_local_display.bind(26, display.playerTextScale);
    query_upsert_local_display.bind(27, serializeStringList(display.metadata.tags)); query_upsert_local_display.bind(28, display.metadata.createdAtUtc); query_upsert_local_display.bind(29, currentUtcTimestamp());
    query_upsert_local_display.exec();
}

void DatabaseManager::loadCampaignPlayerDisplay(Campaign &campaign)
{
    SQLite::Statement query_local_display(*m_db, R"(
                    SELECT stable_id, enabled, public_scene_id, public_map_id, public_encounter_id, show_map, show_initiative, show_visible_npcs, show_party, show_party_spell_slots, show_party_inventory, show_public_handouts, show_shared_art, visible_npc_ids, visible_character_ids, public_handout_ids, shared_art_asset_ids, map_only, preferred_monitor_index, fullscreen_map_only, camera_center_x, camera_center_y, camera_zoom, follow_public_camera, player_text_scale, tags, created_at, updated_at
                    FROM local_player_display
                    WHERE campaign_id = ?
                    LIMIT 1;
                )");
    query_local_display.bind(1, static_cast<int64_t>(campaign.id));
    if (query_local_display.executeStep())
    {
        LocalPlayerDisplayModel &display = campaign.localPlayerDisplay;
        if (!query_local_display.getColumn(0).isNull())
            display.metadata.id = query_local_display.getColumn(0).getString();
        if (!query_local_display.getColumn(1).isNull())
            display.enabled = query_local_display.getColumn(1).getInt() != 0;
        if (!query_local_display.getColumn(2).isNull())
            display.publicSceneId = query_local_display.getColumn(2).getString();
        if (!query_local_display.getColumn(3).isNull())
            display.publicMapId = query_local_display.getColumn(3).getString();
        if (!query_local_display.getColumn(4).isNull())
            display.publicEncounterId = query_local_display.getColumn(4).getString();
        if (!query_local_display.getColumn(5).isNull())
            display.showMap = query_local_display.getColumn(5).getInt() != 0;
        if (!query_local_display.getColumn(6).isNull())
            display.showInitiative = query_local_display.getColumn(6).getInt() != 0;
        if (!query_local_display.getColumn(7).isNull())
            display.showVisibleNpcs = query_local_display.getColumn(7).getInt() != 0;
        if (!query_local_display.getColumn(8).isNull())
            display.showParty = query_local_display.getColumn(8).getInt() != 0;
        if (!query_local_display.getColumn(9).isNull())
            display.showPartySpellSlots = query_local_display.getColumn(9).getInt() != 0;
        if (!query_local_display.getColumn(10).isNull())
            display.showPartyInventory = query_local_display.getColumn(10).getInt() != 0;
        if (!query_local_display.getColumn(11).isNull())
            display.showPublicHandouts = query_local_display.getColumn(11).getInt() != 0;
        if (!query_local_display.getColumn(12).isNull()) display.showSharedArt = query_local_display.getColumn(12).getInt() != 0;
        if (!query_local_display.getColumn(13).isNull()) display.visibleNpcIds = deserializeStringList(query_local_display.getColumn(13).getString());
        if (!query_local_display.getColumn(14).isNull()) display.visibleCharacterIds = deserializeStringList(query_local_display.getColumn(14).getString());
        if (!query_local_display.getColumn(15).isNull()) display.publicHandoutIds = deserializeStringList(query_local_display.getColumn(15).getString());
        if (!query_local_display.getColumn(16).isNull()) display.sharedArtAssetIds = deserializeStringList(query_local_display.getColumn(16).getString());
        if (!query_local_display.getColumn(17).isNull()) display.mapOnly = query_local_display.getColumn(17).getInt() != 0;
        if (!query_local_display.getColumn(18).isNull()) display.preferredMonitorIndex = query_local_display.getColumn(18).getInt();
        if (!query_local_display.getColumn(19).isNull()) display.fullscreenMapOnly = query_local_display.getColumn(19).getInt() != 0;
        if (!query_local_display.getColumn(20).isNull()) display.publicCamera.normalizedCenterX = static_cast<float>(query_local_display.getColumn(20).getDouble());
        if (!query_local_display.getColumn(21).isNull()) display.publicCamera.normalizedCenterY = static_cast<float>(query_local_display.getColumn(21).getDouble());
        if (!query_local_display.getColumn(22).isNull()) display.publicCamera.zoom = static_cast<float>(query_local_display.getColumn(22).getDouble());
        if (!query_local_display.getColumn(23).isNull()) display.publicCamera.followGmCamera = query_local_display.getColumn(23).getInt() != 0;
        if (!query_local_display.getColumn(24).isNull()) display.playerTextScale = static_cast<float>(query_local_display.getColumn(24).getDouble());
        if (!query_local_display.getColumn(25).isNull()) display.metadata.tags = deserializeStringList(query_local_display.getColumn(25).getString());
        if (!query_local_display.getColumn(26).isNull()) display.metadata.createdAtUtc = query_local_display.getColumn(26).getString();
        if (!query_local_display.getColumn(27).isNull()) display.metadata.updatedAtUtc = query_local_display.getColumn(27).getString();
    }
    prepareLocalPlayerDisplayForApplicationStart(campaign.localPlayerDisplay);
}
