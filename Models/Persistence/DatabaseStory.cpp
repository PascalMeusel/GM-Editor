#include "Models/Persistence/CollectionReconcile.hpp"
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

void DatabaseManager::saveCampaignHandouts(int64_t campaignId, const Campaign &campaign)
{
    deleteRowsMissingStableIds("handouts", campaignId, handoutIds(campaign.handouts));
    SQLite::Statement query_insert_handout(*m_db, R"(
            INSERT INTO handouts
            (stable_id, campaign_id, title, body, file_path, gm_notes, tags, created_at, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(stable_id) DO UPDATE SET
                campaign_id = excluded.campaign_id,
                title = excluded.title,
                body = excluded.body,
                file_path = excluded.file_path,
                gm_notes = excluded.gm_notes,
                tags = excluded.tags,
                updated_at = excluded.updated_at;
        )");
    for (const HandoutModel &handout : campaign.handouts)
    {
        query_insert_handout.bind(1, handout.metadata.id);
        query_insert_handout.bind(2, campaignId);
        query_insert_handout.bind(3, handout.title.empty() ? "Untitled Handout" : handout.title);
        query_insert_handout.bind(4, handout.body);
        query_insert_handout.bind(5, handout.filePath);
        query_insert_handout.bind(6, handout.gmNotes);
        query_insert_handout.bind(7, serializeStringList(handout.metadata.tags));
        query_insert_handout.bind(8, handout.metadata.createdAtUtc);
        query_insert_handout.bind(9, currentUtcTimestamp());
        query_insert_handout.exec();
        query_insert_handout.reset();
    }
}

void DatabaseManager::saveCampaignStory(int64_t campaignId, const Campaign &campaign)
{
    std::vector<std::string> threads, beats, links;
    for (const auto &thread : campaign.storyThreads) {
        threads.push_back(thread.metadata.id);
        int linkOrder = 0;
        for (const auto &link : thread.links) links.push_back(link.metadata.id);
        for (const auto &beat : thread.beats) {
            beats.push_back(beat.metadata.id);
            int linkOrder = 0;
            for (const auto &link : beat.links) links.push_back(link.metadata.id);
        }
    }
    CollectionReconcile::retain(*m_db, "story_links",
        "SELECT id, stable_id, campaign_id FROM story_links;", campaignId, links);
    CollectionReconcile::retain(*m_db, "story_beats",
        "SELECT b.id, b.stable_id, t.campaign_id FROM story_beats b JOIN story_threads t ON t.id=b.thread_id;", campaignId, beats);
    CollectionReconcile::retain(*m_db, "story_threads",
        "SELECT id, stable_id, campaign_id FROM story_threads;", campaignId, threads);

    SQLite::Statement query_insert_story_thread(*m_db, R"(
            INSERT INTO story_threads
            (stable_id, campaign_id, title, thread_type, status, summary, stakes, gm_notes, tags, created_at, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(stable_id) DO UPDATE SET
            campaign_id = excluded.campaign_id,
            title = excluded.title,
            thread_type = excluded.thread_type,
            status = excluded.status,
            summary = excluded.summary,
            stakes = excluded.stakes,
            gm_notes = excluded.gm_notes,
            tags = excluded.tags,
            updated_at = excluded.updated_at
        WHERE story_threads.campaign_id IS NOT excluded.campaign_id
           OR story_threads.title IS NOT excluded.title
           OR story_threads.thread_type IS NOT excluded.thread_type
           OR story_threads.status IS NOT excluded.status
           OR story_threads.summary IS NOT excluded.summary
           OR story_threads.stakes IS NOT excluded.stakes
           OR story_threads.gm_notes IS NOT excluded.gm_notes
           OR story_threads.tags IS NOT excluded.tags;
        )");
    SQLite::Statement query_insert_story_beat(*m_db, R"(
            INSERT INTO story_beats
            (stable_id, thread_id, title, summary, player_summary, read_aloud_text, clues, consequences, gm_notes, status, in_world_date, cue_audio_asset_id, cue_volume, cue_loop, tags, created_at, updated_at, sort_order)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(stable_id) DO UPDATE SET
            thread_id = excluded.thread_id,
            title = excluded.title,
            summary = excluded.summary,
            player_summary = excluded.player_summary,
            read_aloud_text = excluded.read_aloud_text,
            clues = excluded.clues,
            consequences = excluded.consequences,
            gm_notes = excluded.gm_notes,
            status = excluded.status,
            in_world_date = excluded.in_world_date,
            cue_audio_asset_id = excluded.cue_audio_asset_id,
            cue_volume = excluded.cue_volume,
            cue_loop = excluded.cue_loop,
            tags = excluded.tags,
            updated_at = excluded.updated_at,
            sort_order = excluded.sort_order
        WHERE story_beats.thread_id IS NOT excluded.thread_id
           OR story_beats.title IS NOT excluded.title
           OR story_beats.summary IS NOT excluded.summary
           OR story_beats.player_summary IS NOT excluded.player_summary
           OR story_beats.read_aloud_text IS NOT excluded.read_aloud_text
           OR story_beats.clues IS NOT excluded.clues
           OR story_beats.consequences IS NOT excluded.consequences
           OR story_beats.gm_notes IS NOT excluded.gm_notes
           OR story_beats.status IS NOT excluded.status
           OR story_beats.in_world_date IS NOT excluded.in_world_date
           OR story_beats.cue_audio_asset_id IS NOT excluded.cue_audio_asset_id
           OR story_beats.cue_volume IS NOT excluded.cue_volume
           OR story_beats.cue_loop IS NOT excluded.cue_loop
           OR story_beats.tags IS NOT excluded.tags
           OR story_beats.sort_order IS NOT excluded.sort_order;
        )");
    SQLite::Statement query_insert_story_link(*m_db, R"(
            INSERT INTO story_links
            (stable_id, campaign_id, owner_entity_id, owner_kind, link_type, label, target_entity_id, target_entity_type, tags, created_at, updated_at, sort_order)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(stable_id) DO UPDATE SET
            sort_order = excluded.sort_order,
            campaign_id = excluded.campaign_id,
            owner_entity_id = excluded.owner_entity_id,
            owner_kind = excluded.owner_kind,
            link_type = excluded.link_type,
            label = excluded.label,
            target_entity_id = excluded.target_entity_id,
            target_entity_type = excluded.target_entity_type,
            tags = excluded.tags,
            updated_at = excluded.updated_at
        WHERE story_links.sort_order IS NOT excluded.sort_order
           OR story_links.campaign_id IS NOT excluded.campaign_id
           OR story_links.owner_entity_id IS NOT excluded.owner_entity_id
           OR story_links.owner_kind IS NOT excluded.owner_kind
           OR story_links.link_type IS NOT excluded.link_type
           OR story_links.label IS NOT excluded.label
           OR story_links.target_entity_id IS NOT excluded.target_entity_id
           OR story_links.target_entity_type IS NOT excluded.target_entity_type
           OR story_links.tags IS NOT excluded.tags;
        )");

    for (const auto &thread : campaign.storyThreads)
    {
        query_insert_story_thread.bind(1, thread.metadata.id);
        query_insert_story_thread.bind(2, campaignId);
        query_insert_story_thread.bind(3, thread.title);
        query_insert_story_thread.bind(4, static_cast<int>(thread.type));
        query_insert_story_thread.bind(5, static_cast<int>(thread.status));
        query_insert_story_thread.bind(6, thread.summary);
        query_insert_story_thread.bind(7, thread.stakes);
        query_insert_story_thread.bind(8, thread.gmNotes);
        query_insert_story_thread.bind(9, serializeStringList(thread.metadata.tags));
        query_insert_story_thread.bind(10, thread.metadata.createdAtUtc);
        query_insert_story_thread.bind(11, currentUtcTimestamp());
        query_insert_story_thread.exec();
        const int64_t threadId = CollectionReconcile::rowId(*m_db, "story_threads", thread.metadata.id);
        query_insert_story_thread.reset();

        int linkOrder = 0;
        for (const auto &link : thread.links)
        {
            query_insert_story_link.bind(1, link.metadata.id);
            query_insert_story_link.bind(2, campaignId);
            query_insert_story_link.bind(3, thread.metadata.id);
            query_insert_story_link.bind(4, 0);
            query_insert_story_link.bind(5, static_cast<int>(link.type));
            query_insert_story_link.bind(6, link.label);
            query_insert_story_link.bind(7, link.targetEntityId);
            query_insert_story_link.bind(8, link.targetEntityType);
            query_insert_story_link.bind(9, serializeStringList(link.metadata.tags));
            query_insert_story_link.bind(10, link.metadata.createdAtUtc);
            query_insert_story_link.bind(11, currentUtcTimestamp());
            query_insert_story_link.bind(12, linkOrder++);
            query_insert_story_link.exec();
            query_insert_story_link.reset();
        }

        int sortOrder = 0;
        for (const auto &beat : thread.beats)
        {
            query_insert_story_beat.bind(1, beat.metadata.id);
            query_insert_story_beat.bind(2, threadId);
            query_insert_story_beat.bind(3, beat.title);
            query_insert_story_beat.bind(4, beat.summary);
            query_insert_story_beat.bind(5, beat.playerSummary);
            query_insert_story_beat.bind(6, beat.readAloudText);
            query_insert_story_beat.bind(7, beat.clues);
            query_insert_story_beat.bind(8, beat.consequences);
            query_insert_story_beat.bind(9, beat.gmNotes);
            query_insert_story_beat.bind(10, static_cast<int>(beat.status));
            query_insert_story_beat.bind(11, beat.inWorldDate);
            query_insert_story_beat.bind(12, beat.cueAudioAssetId);
            query_insert_story_beat.bind(13, beat.cueVolume);
            query_insert_story_beat.bind(14, beat.cueLoop ? 1 : 0);
            query_insert_story_beat.bind(15, serializeStringList(beat.metadata.tags));
            query_insert_story_beat.bind(16, beat.metadata.createdAtUtc);
            query_insert_story_beat.bind(17, currentUtcTimestamp());
            query_insert_story_beat.bind(18, sortOrder++);
            query_insert_story_beat.exec();
            query_insert_story_beat.reset();

            int linkOrder = 0;
            for (const auto &link : beat.links)
            {
                query_insert_story_link.bind(1, link.metadata.id);
                query_insert_story_link.bind(2, campaignId);
                query_insert_story_link.bind(3, beat.metadata.id);
                query_insert_story_link.bind(4, 1);
                query_insert_story_link.bind(5, static_cast<int>(link.type));
                query_insert_story_link.bind(6, link.label);
                query_insert_story_link.bind(7, link.targetEntityId);
                query_insert_story_link.bind(8, link.targetEntityType);
                query_insert_story_link.bind(9, serializeStringList(link.metadata.tags));
                query_insert_story_link.bind(10, link.metadata.createdAtUtc);
                query_insert_story_link.bind(11, currentUtcTimestamp());
                query_insert_story_link.bind(12, linkOrder++);
            query_insert_story_link.exec();
                query_insert_story_link.reset();
            }
        }
    }
}

void DatabaseManager::saveCampaignTimeline(int64_t campaignId, const Campaign &campaign)
{
    deleteRowsMissingStableIds("timeline_events", campaignId, timelineEventIds(campaign.timelineEvents));

    SQLite::Statement query_insert_timeline_event(*m_db, R"(
            INSERT INTO timeline_events
            (stable_id, campaign_id, title, description, in_world_date, related_entity_ids, tags, created_at, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(stable_id) DO UPDATE SET
                campaign_id = excluded.campaign_id,
                title = excluded.title,
                description = excluded.description,
                in_world_date = excluded.in_world_date,
                related_entity_ids = excluded.related_entity_ids,
                tags = excluded.tags,
                updated_at = excluded.updated_at;
        )");
    for (const TimelineEventModel &event : campaign.timelineEvents)
    {
        query_insert_timeline_event.bind(1, event.metadata.id);
        query_insert_timeline_event.bind(2, campaignId);
        query_insert_timeline_event.bind(3, event.title.empty() ? "Timeline Event" : event.title);
        query_insert_timeline_event.bind(4, event.description);
        query_insert_timeline_event.bind(5, event.inWorldDate);
        query_insert_timeline_event.bind(6, serializeStringList(event.relatedEntityIds));
        query_insert_timeline_event.bind(7, serializeStringList(event.metadata.tags));
        query_insert_timeline_event.bind(8, event.metadata.createdAtUtc);
        query_insert_timeline_event.bind(9, currentUtcTimestamp());
        query_insert_timeline_event.exec();
        query_insert_timeline_event.reset();
    }
}
