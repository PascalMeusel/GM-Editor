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

void DatabaseManager::saveCampaignAudio(int64_t campaignId, const Campaign &campaign)
{
    deleteRowsMissingStableIds("audio_assets", campaignId, audioAssetIds(campaign.audioAssets));

    SQLite::Statement query_insert_audio_asset(*m_db, R"(
            INSERT INTO audio_assets
            (stable_id, campaign_id, name, original_file_name, original_source_path, file_path, asset_type, default_volume, loop, duration_seconds, missing_file, last_played_at_utc, tags, created_at, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(stable_id) DO UPDATE SET
                campaign_id = excluded.campaign_id,
                name = excluded.name,
                original_file_name = excluded.original_file_name,
                original_source_path = excluded.original_source_path,
                file_path = excluded.file_path,
                asset_type = excluded.asset_type,
                default_volume = excluded.default_volume,
                loop = excluded.loop,
                duration_seconds = excluded.duration_seconds,
                missing_file = excluded.missing_file,
                last_played_at_utc = excluded.last_played_at_utc,
                tags = excluded.tags,
                updated_at = excluded.updated_at;
        )");
    for (const auto &asset : campaign.audioAssets)
    {
        query_insert_audio_asset.bind(1, asset.metadata.id);
        query_insert_audio_asset.bind(2, campaignId);
        query_insert_audio_asset.bind(3, asset.name);
        query_insert_audio_asset.bind(4, asset.originalFileName);
        query_insert_audio_asset.bind(5, asset.originalSourcePath);
        query_insert_audio_asset.bind(6, asset.filePath);
        query_insert_audio_asset.bind(7, static_cast<int>(asset.type));
        query_insert_audio_asset.bind(8, asset.defaultVolume);
        query_insert_audio_asset.bind(9, asset.loop ? 1 : 0);
        query_insert_audio_asset.bind(10, asset.durationSeconds);
        query_insert_audio_asset.bind(11, asset.missingFile ? 1 : 0);
        query_insert_audio_asset.bind(12, asset.lastPlayedAtUtc);
        query_insert_audio_asset.bind(13, serializeStringList(asset.metadata.tags));
        query_insert_audio_asset.bind(14, asset.metadata.createdAtUtc);
        query_insert_audio_asset.bind(15, currentUtcTimestamp());

        query_insert_audio_asset.exec();
        query_insert_audio_asset.reset();
    }

    deleteRowsMissingStableIds("audio_playlists", campaignId, audioPlaylistIds(campaign.audioPlaylists));

    SQLite::Statement query_insert_audio_playlist(*m_db, R"(
            INSERT INTO audio_playlists
            (stable_id, campaign_id, name, audio_asset_ids, shuffle, repeat, one_shot, crossfade_seconds, tags, created_at, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(stable_id) DO UPDATE SET
                campaign_id = excluded.campaign_id,
                name = excluded.name,
                audio_asset_ids = excluded.audio_asset_ids,
                shuffle = excluded.shuffle,
                repeat = excluded.repeat,
                one_shot = excluded.one_shot,
                crossfade_seconds = excluded.crossfade_seconds,
                tags = excluded.tags,
                updated_at = excluded.updated_at;
        )");
    for (const AudioPlaylistModel &playlist : campaign.audioPlaylists)
    {
        query_insert_audio_playlist.bind(1, playlist.metadata.id);
        query_insert_audio_playlist.bind(2, campaignId);
        query_insert_audio_playlist.bind(3, playlist.name);
        query_insert_audio_playlist.bind(4, serializeStringList(playlist.audioAssetIds));
        query_insert_audio_playlist.bind(5, playlist.shuffle ? 1 : 0);
        query_insert_audio_playlist.bind(6, playlist.repeat ? 1 : 0);
        query_insert_audio_playlist.bind(7, playlist.oneShot ? 1 : 0);
        query_insert_audio_playlist.bind(8, playlist.crossfadeSeconds);
        query_insert_audio_playlist.bind(9, serializeStringList(playlist.metadata.tags));
        query_insert_audio_playlist.bind(10, playlist.metadata.createdAtUtc);
        query_insert_audio_playlist.bind(11, currentUtcTimestamp());

        query_insert_audio_playlist.exec();
        query_insert_audio_playlist.reset();
    }

    deleteRowsMissingStableIds("audio_ambience_layers", campaignId,
                               audioAmbienceLayerIds(campaign.audioAmbienceLayers));

    SQLite::Statement query_insert_audio_ambience_layer(*m_db, R"(
            INSERT INTO audio_ambience_layers
            (stable_id, campaign_id, name, audio_asset_id, volume, loop, active, sort_order, tags, created_at, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(stable_id) DO UPDATE SET
                campaign_id = excluded.campaign_id,
                name = excluded.name,
                audio_asset_id = excluded.audio_asset_id,
                volume = excluded.volume,
                loop = excluded.loop,
                active = excluded.active,
                sort_order = excluded.sort_order,
                tags = excluded.tags,
                updated_at = excluded.updated_at;
        )");
    for (const AudioAmbienceLayerModel &layer : campaign.audioAmbienceLayers)
    {
        query_insert_audio_ambience_layer.bind(1, layer.metadata.id);
        query_insert_audio_ambience_layer.bind(2, campaignId);
        query_insert_audio_ambience_layer.bind(3, layer.name);
        query_insert_audio_ambience_layer.bind(4, layer.audioAssetId);
        query_insert_audio_ambience_layer.bind(5, layer.volume);
        query_insert_audio_ambience_layer.bind(6, layer.loop ? 1 : 0);
        query_insert_audio_ambience_layer.bind(7, layer.active ? 1 : 0);
        query_insert_audio_ambience_layer.bind(8, layer.sortOrder);
        query_insert_audio_ambience_layer.bind(9, serializeStringList(layer.metadata.tags));
        query_insert_audio_ambience_layer.bind(10, layer.metadata.createdAtUtc);
        query_insert_audio_ambience_layer.bind(11, currentUtcTimestamp());

        query_insert_audio_ambience_layer.exec();
        query_insert_audio_ambience_layer.reset();
    }

    deleteRowsMissingStableIds("audio_triggers", campaignId, audioTriggerIds(campaign.audioTriggers));

    SQLite::Statement query_insert_audio_trigger(*m_db, R"(
            INSERT INTO audio_triggers
            (stable_id, campaign_id, name, source_type, source_entity_id, event_name, audio_asset_id, volume, loop, enabled, tags, created_at, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(stable_id) DO UPDATE SET
                campaign_id = excluded.campaign_id,
                name = excluded.name,
                source_type = excluded.source_type,
                source_entity_id = excluded.source_entity_id,
                event_name = excluded.event_name,
                audio_asset_id = excluded.audio_asset_id,
                volume = excluded.volume,
                loop = excluded.loop,
                enabled = excluded.enabled,
                tags = excluded.tags,
                updated_at = excluded.updated_at;
        )");
    for (const AudioTriggerModel &trigger : campaign.audioTriggers)
    {
        query_insert_audio_trigger.bind(1, trigger.metadata.id);
        query_insert_audio_trigger.bind(2, campaignId);
        query_insert_audio_trigger.bind(3, trigger.name.empty() ? "Audio Trigger" : trigger.name);
        query_insert_audio_trigger.bind(4, trigger.sourceType);
        query_insert_audio_trigger.bind(5, trigger.sourceEntityId);
        query_insert_audio_trigger.bind(6, trigger.eventName);
        query_insert_audio_trigger.bind(7, trigger.audioAssetId);
        query_insert_audio_trigger.bind(8, trigger.volume);
        query_insert_audio_trigger.bind(9, trigger.loop ? 1 : 0);
        query_insert_audio_trigger.bind(10, trigger.enabled ? 1 : 0);
        query_insert_audio_trigger.bind(11, serializeStringList(trigger.metadata.tags));
        query_insert_audio_trigger.bind(12, trigger.metadata.createdAtUtc);
        query_insert_audio_trigger.bind(13, currentUtcTimestamp());

        query_insert_audio_trigger.exec();
        query_insert_audio_trigger.reset();
    }
}

void DatabaseManager::loadCampaignAudio(Campaign &campaign)
{
    SQLite::Statement query_audio_assets(*m_db, R"(
                    SELECT stable_id, name, original_file_name, original_source_path, file_path, asset_type, default_volume, loop, duration_seconds, missing_file, last_played_at_utc, tags, created_at, updated_at
                    FROM audio_assets
                    WHERE campaign_id = ?
                    ORDER BY asset_type ASC, name ASC;
                )");
    query_audio_assets.bind(1, static_cast<int64_t>(campaign.id));

    while (query_audio_assets.executeStep())
    {
        AudioAssetModel asset;
        if (!query_audio_assets.getColumn(0).isNull())
            asset.metadata.id = query_audio_assets.getColumn(0).getString();
        asset.name = query_audio_assets.getColumn(1).getString();
        if (!query_audio_assets.getColumn(2).isNull())
            asset.originalFileName = query_audio_assets.getColumn(2).getString();
        if (!query_audio_assets.getColumn(3).isNull())
            asset.originalSourcePath = query_audio_assets.getColumn(3).getString();
        if (!query_audio_assets.getColumn(4).isNull())
            asset.filePath = query_audio_assets.getColumn(4).getString();
        if (!query_audio_assets.getColumn(5).isNull())
            asset.type = static_cast<AudioAssetType>(query_audio_assets.getColumn(5).getInt());
        if (!query_audio_assets.getColumn(6).isNull())
            asset.defaultVolume = static_cast<float>(query_audio_assets.getColumn(6).getDouble());
        if (!query_audio_assets.getColumn(7).isNull())
            asset.loop = query_audio_assets.getColumn(7).getInt() != 0;
        if (!query_audio_assets.getColumn(8).isNull())
            asset.durationSeconds = query_audio_assets.getColumn(8).getInt();
        if (!query_audio_assets.getColumn(9).isNull())
            asset.missingFile = query_audio_assets.getColumn(9).getInt() != 0;
        if (!query_audio_assets.getColumn(10).isNull())
            asset.lastPlayedAtUtc = query_audio_assets.getColumn(10).getString();
        if (!query_audio_assets.getColumn(11).isNull())
            asset.metadata.tags = deserializeStringList(query_audio_assets.getColumn(11).getString());
        if (!query_audio_assets.getColumn(12).isNull())
            asset.metadata.createdAtUtc = query_audio_assets.getColumn(12).getString();
        if (!query_audio_assets.getColumn(13).isNull())
            asset.metadata.updatedAtUtc = query_audio_assets.getColumn(13).getString();

        campaign.audioAssets.push_back(asset);
    }

    SQLite::Statement query_audio_playlists(*m_db, R"(
                    SELECT stable_id, name, audio_asset_ids, shuffle, repeat, one_shot, crossfade_seconds, tags, created_at, updated_at
                    FROM audio_playlists
                    WHERE campaign_id = ?
                    ORDER BY name ASC;
                )");
    query_audio_playlists.bind(1, static_cast<int64_t>(campaign.id));

    while (query_audio_playlists.executeStep())
    {
        AudioPlaylistModel playlist;
        if (!query_audio_playlists.getColumn(0).isNull())
            playlist.metadata.id = query_audio_playlists.getColumn(0).getString();
        playlist.name = query_audio_playlists.getColumn(1).getString();
        if (!query_audio_playlists.getColumn(2).isNull())
            playlist.audioAssetIds = deserializeStringList(query_audio_playlists.getColumn(2).getString());
        if (!query_audio_playlists.getColumn(3).isNull())
            playlist.shuffle = query_audio_playlists.getColumn(3).getInt() != 0;
        if (!query_audio_playlists.getColumn(4).isNull())
            playlist.repeat = query_audio_playlists.getColumn(4).getInt() != 0;
        if (!query_audio_playlists.getColumn(5).isNull())
            playlist.oneShot = query_audio_playlists.getColumn(5).getInt() != 0;
        if (!query_audio_playlists.getColumn(6).isNull())
            playlist.crossfadeSeconds = static_cast<float>(query_audio_playlists.getColumn(6).getDouble());
        if (!query_audio_playlists.getColumn(7).isNull())
            playlist.metadata.tags = deserializeStringList(query_audio_playlists.getColumn(7).getString());
        if (!query_audio_playlists.getColumn(8).isNull())
            playlist.metadata.createdAtUtc = query_audio_playlists.getColumn(8).getString();
        if (!query_audio_playlists.getColumn(9).isNull())
            playlist.metadata.updatedAtUtc = query_audio_playlists.getColumn(9).getString();

        campaign.audioPlaylists.push_back(playlist);
        campaign.audioPlaylistIds.push_back(playlist.metadata.id);
    }

    SQLite::Statement query_audio_ambience_layers(*m_db, R"(
                    SELECT stable_id, name, audio_asset_id, volume, loop, active, sort_order, tags, created_at, updated_at
                    FROM audio_ambience_layers
                    WHERE campaign_id = ?
                    ORDER BY sort_order ASC, name ASC;
                )");
    query_audio_ambience_layers.bind(1, static_cast<int64_t>(campaign.id));

    while (query_audio_ambience_layers.executeStep())
    {
        AudioAmbienceLayerModel layer;
        if (!query_audio_ambience_layers.getColumn(0).isNull())
            layer.metadata.id = query_audio_ambience_layers.getColumn(0).getString();
        layer.name = query_audio_ambience_layers.getColumn(1).getString();
        if (!query_audio_ambience_layers.getColumn(2).isNull())
            layer.audioAssetId = query_audio_ambience_layers.getColumn(2).getString();
        if (!query_audio_ambience_layers.getColumn(3).isNull())
            layer.volume = static_cast<float>(query_audio_ambience_layers.getColumn(3).getDouble());
        if (!query_audio_ambience_layers.getColumn(4).isNull())
            layer.loop = query_audio_ambience_layers.getColumn(4).getInt() != 0;
        if (!query_audio_ambience_layers.getColumn(5).isNull())
            layer.active = query_audio_ambience_layers.getColumn(5).getInt() != 0;
        if (!query_audio_ambience_layers.getColumn(6).isNull())
            layer.sortOrder = query_audio_ambience_layers.getColumn(6).getInt();
        if (!query_audio_ambience_layers.getColumn(7).isNull())
            layer.metadata.tags = deserializeStringList(query_audio_ambience_layers.getColumn(7).getString());
        if (!query_audio_ambience_layers.getColumn(8).isNull())
            layer.metadata.createdAtUtc = query_audio_ambience_layers.getColumn(8).getString();
        if (!query_audio_ambience_layers.getColumn(9).isNull())
            layer.metadata.updatedAtUtc = query_audio_ambience_layers.getColumn(9).getString();

        campaign.audioAmbienceLayers.push_back(layer);
        campaign.audioAmbienceLayerIds.push_back(layer.metadata.id);
    }

    SQLite::Statement query_audio_triggers(*m_db, R"(
                    SELECT stable_id, name, source_type, source_entity_id, event_name, audio_asset_id, volume, loop, enabled, tags, created_at, updated_at
                    FROM audio_triggers
                    WHERE campaign_id = ?
                    ORDER BY source_type ASC, event_name ASC, name ASC;
                )");
    query_audio_triggers.bind(1, static_cast<int64_t>(campaign.id));

    while (query_audio_triggers.executeStep())
    {
        AudioTriggerModel trigger;
        if (!query_audio_triggers.getColumn(0).isNull())
            trigger.metadata.id = query_audio_triggers.getColumn(0).getString();
        trigger.name = query_audio_triggers.getColumn(1).getString();
        if (!query_audio_triggers.getColumn(2).isNull())
            trigger.sourceType = query_audio_triggers.getColumn(2).getString();
        if (!query_audio_triggers.getColumn(3).isNull())
            trigger.sourceEntityId = query_audio_triggers.getColumn(3).getString();
        if (!query_audio_triggers.getColumn(4).isNull())
            trigger.eventName = query_audio_triggers.getColumn(4).getString();
        if (!query_audio_triggers.getColumn(5).isNull())
            trigger.audioAssetId = query_audio_triggers.getColumn(5).getString();
        if (!query_audio_triggers.getColumn(6).isNull())
            trigger.volume = static_cast<float>(query_audio_triggers.getColumn(6).getDouble());
        if (!query_audio_triggers.getColumn(7).isNull())
            trigger.loop = query_audio_triggers.getColumn(7).getInt() != 0;
        if (!query_audio_triggers.getColumn(8).isNull())
            trigger.enabled = query_audio_triggers.getColumn(8).getInt() != 0;
        if (!query_audio_triggers.getColumn(9).isNull())
            trigger.metadata.tags = deserializeStringList(query_audio_triggers.getColumn(9).getString());
        if (!query_audio_triggers.getColumn(10).isNull())
            trigger.metadata.createdAtUtc = query_audio_triggers.getColumn(10).getString();
        if (!query_audio_triggers.getColumn(11).isNull())
            trigger.metadata.updatedAtUtc = query_audio_triggers.getColumn(11).getString();

        campaign.audioTriggers.push_back(trigger);
        campaign.audioTriggerIds.push_back(trigger.metadata.id);
    }
}
