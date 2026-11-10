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

void DatabaseManager::loadCampaignHandoutsAndDocuments(Campaign &campaign)
{
    SQLite::Statement query_handouts(*m_db, R"(
                    SELECT stable_id, title, body, file_path, gm_notes, tags, created_at, updated_at
                    FROM handouts
                    WHERE campaign_id = ?
                    ORDER BY title ASC;
                )");
    query_handouts.bind(1, static_cast<int64_t>(campaign.id));
    while (query_handouts.executeStep())
    {
        HandoutModel handout;
        if (!query_handouts.getColumn(0).isNull())
            handout.metadata.id = query_handouts.getColumn(0).getString();
        handout.title = query_handouts.getColumn(1).getString();
        if (!query_handouts.getColumn(2).isNull())
            handout.body = query_handouts.getColumn(2).getString();
        if (!query_handouts.getColumn(3).isNull())
            handout.filePath = query_handouts.getColumn(3).getString();
        if (!query_handouts.getColumn(4).isNull())
            handout.gmNotes = query_handouts.getColumn(4).getString();
        if (!query_handouts.getColumn(5).isNull())
            handout.metadata.tags = deserializeStringList(query_handouts.getColumn(5).getString());
        if (!query_handouts.getColumn(6).isNull())
            handout.metadata.createdAtUtc = query_handouts.getColumn(6).getString();
        if (!query_handouts.getColumn(7).isNull())
            handout.metadata.updatedAtUtc = query_handouts.getColumn(7).getString();
        campaign.handouts.push_back(handout);
    }

    SQLite::Statement query_story_document_ids(*m_db, R"(
                    SELECT stable_id
                    FROM story_documents
                    WHERE campaign_id = ?
                    ORDER BY title ASC;
                )");
    query_story_document_ids.bind(1, static_cast<int64_t>(campaign.id));
    while (query_story_document_ids.executeStep())
    {
        if (!query_story_document_ids.getColumn(0).isNull())
            campaign.storyDocumentIds.push_back(query_story_document_ids.getColumn(0).getString());
    }
}

void DatabaseManager::loadCampaignStory(Campaign &campaign)
{
    SQLite::Statement query_story_threads(*m_db, R"(
                    SELECT id, stable_id, title, thread_type, status, summary, stakes, gm_notes, tags, created_at, updated_at
                    FROM story_threads
                    WHERE campaign_id = ?
                    ORDER BY updated_at DESC, title ASC;
                )");
    query_story_threads.bind(1, static_cast<int64_t>(campaign.id));

    while (query_story_threads.executeStep())
    {
        StoryThreadModel thread;
        const int64_t threadRowId = query_story_threads.getColumn(0).getInt64();
        if (!query_story_threads.getColumn(1).isNull())
            thread.metadata.id = query_story_threads.getColumn(1).getString();
        thread.title = query_story_threads.getColumn(2).getString();
        if (!query_story_threads.getColumn(3).isNull())
            thread.type = static_cast<StoryThreadType>(query_story_threads.getColumn(3).getInt());
        if (!query_story_threads.getColumn(4).isNull())
            thread.status = static_cast<StoryStatus>(query_story_threads.getColumn(4).getInt());
        if (!query_story_threads.getColumn(5).isNull())
            thread.summary = query_story_threads.getColumn(5).getString();
        if (!query_story_threads.getColumn(6).isNull())
            thread.stakes = query_story_threads.getColumn(6).getString();
        if (!query_story_threads.getColumn(7).isNull())
            thread.gmNotes = query_story_threads.getColumn(7).getString();
        if (!query_story_threads.getColumn(8).isNull())
            thread.metadata.tags = deserializeStringList(query_story_threads.getColumn(8).getString());
        if (!query_story_threads.getColumn(9).isNull())
            thread.metadata.createdAtUtc = query_story_threads.getColumn(9).getString();
        if (!query_story_threads.getColumn(10).isNull())
            thread.metadata.updatedAtUtc = query_story_threads.getColumn(10).getString();

        SQLite::Statement query_thread_links(*m_db, R"(
                        SELECT stable_id, link_type, label, target_entity_id, target_entity_type, tags, created_at, updated_at
                        FROM story_links
                        WHERE campaign_id = ? AND owner_entity_id = ? AND owner_kind = 0
                        ORDER BY sort_order ASC, id ASC;
                    )");
        query_thread_links.bind(1, static_cast<int64_t>(campaign.id));
        query_thread_links.bind(2, thread.metadata.id);

        while (query_thread_links.executeStep())
        {
            StoryLinkModel link;
            if (!query_thread_links.getColumn(0).isNull())
                link.metadata.id = query_thread_links.getColumn(0).getString();
            if (!query_thread_links.getColumn(1).isNull())
                link.type = static_cast<StoryLinkType>(query_thread_links.getColumn(1).getInt());
            if (!query_thread_links.getColumn(2).isNull())
                link.label = query_thread_links.getColumn(2).getString();
            if (!query_thread_links.getColumn(3).isNull())
                link.targetEntityId = query_thread_links.getColumn(3).getString();
            if (!query_thread_links.getColumn(4).isNull())
                link.targetEntityType = query_thread_links.getColumn(4).getString();
            if (!query_thread_links.getColumn(5).isNull())
                link.metadata.tags = deserializeStringList(query_thread_links.getColumn(5).getString());
            if (!query_thread_links.getColumn(6).isNull())
                link.metadata.createdAtUtc = query_thread_links.getColumn(6).getString();
            if (!query_thread_links.getColumn(7).isNull())
                link.metadata.updatedAtUtc = query_thread_links.getColumn(7).getString();
            thread.links.push_back(link);
        }

        SQLite::Statement query_story_beats(*m_db, R"(
                        SELECT stable_id, title, summary, player_summary, read_aloud_text, clues, consequences, gm_notes, status, in_world_date, cue_audio_asset_id, cue_volume, cue_loop, tags, created_at, updated_at
                        FROM story_beats
                        WHERE thread_id = ?
                        ORDER BY sort_order ASC, id ASC;
                    )");
        query_story_beats.bind(1, threadRowId);

        while (query_story_beats.executeStep())
        {
            StoryBeatModel beat;
            if (!query_story_beats.getColumn(0).isNull())
                beat.metadata.id = query_story_beats.getColumn(0).getString();
            beat.title = query_story_beats.getColumn(1).getString();
            if (!query_story_beats.getColumn(2).isNull())
                beat.summary = query_story_beats.getColumn(2).getString();
            if (!query_story_beats.getColumn(3).isNull())
                beat.playerSummary = query_story_beats.getColumn(3).getString();
            if (!query_story_beats.getColumn(4).isNull())
                beat.readAloudText = query_story_beats.getColumn(4).getString();
            if (!query_story_beats.getColumn(5).isNull())
                beat.clues = query_story_beats.getColumn(5).getString();
            if (!query_story_beats.getColumn(6).isNull())
                beat.consequences = query_story_beats.getColumn(6).getString();
            if (!query_story_beats.getColumn(7).isNull())
                beat.gmNotes = query_story_beats.getColumn(7).getString();
            if (!query_story_beats.getColumn(8).isNull())
                beat.status = static_cast<StoryStatus>(query_story_beats.getColumn(8).getInt());
            if (!query_story_beats.getColumn(9).isNull())
                beat.inWorldDate = query_story_beats.getColumn(9).getString();
            if (!query_story_beats.getColumn(10).isNull())
                beat.cueAudioAssetId = query_story_beats.getColumn(10).getString();
            if (!query_story_beats.getColumn(11).isNull())
                beat.cueVolume = static_cast<float>(query_story_beats.getColumn(11).getDouble());
            if (!query_story_beats.getColumn(12).isNull())
                beat.cueLoop = query_story_beats.getColumn(12).getInt() != 0;
            if (!query_story_beats.getColumn(13).isNull())
                beat.metadata.tags = deserializeStringList(query_story_beats.getColumn(13).getString());
            if (!query_story_beats.getColumn(14).isNull())
                beat.metadata.createdAtUtc = query_story_beats.getColumn(14).getString();
            if (!query_story_beats.getColumn(15).isNull())
                beat.metadata.updatedAtUtc = query_story_beats.getColumn(15).getString();

            SQLite::Statement query_beat_links(*m_db, R"(
                            SELECT stable_id, link_type, label, target_entity_id, target_entity_type, tags, created_at, updated_at
                            FROM story_links
                            WHERE campaign_id = ? AND owner_entity_id = ? AND owner_kind = 1
                            ORDER BY sort_order ASC, id ASC;
                        )");
            query_beat_links.bind(1, static_cast<int64_t>(campaign.id));
            query_beat_links.bind(2, beat.metadata.id);

            while (query_beat_links.executeStep())
            {
                StoryLinkModel link;
                if (!query_beat_links.getColumn(0).isNull())
                    link.metadata.id = query_beat_links.getColumn(0).getString();
                if (!query_beat_links.getColumn(1).isNull())
                    link.type = static_cast<StoryLinkType>(query_beat_links.getColumn(1).getInt());
                if (!query_beat_links.getColumn(2).isNull())
                    link.label = query_beat_links.getColumn(2).getString();
                if (!query_beat_links.getColumn(3).isNull())
                    link.targetEntityId = query_beat_links.getColumn(3).getString();
                if (!query_beat_links.getColumn(4).isNull())
                    link.targetEntityType = query_beat_links.getColumn(4).getString();
                if (!query_beat_links.getColumn(5).isNull())
                    link.metadata.tags = deserializeStringList(query_beat_links.getColumn(5).getString());
                if (!query_beat_links.getColumn(6).isNull())
                    link.metadata.createdAtUtc = query_beat_links.getColumn(6).getString();
                if (!query_beat_links.getColumn(7).isNull())
                    link.metadata.updatedAtUtc = query_beat_links.getColumn(7).getString();
                beat.links.push_back(link);
            }

            thread.beats.push_back(beat);
        }

        campaign.storyThreads.push_back(thread);
    }
}

void DatabaseManager::loadCampaignTimeline(Campaign &campaign)
{
    SQLite::Statement query_timeline_events(*m_db, R"(
                    SELECT stable_id, title, description, in_world_date, related_entity_ids, tags, created_at, updated_at
                    FROM timeline_events
                    WHERE campaign_id = ?
                    ORDER BY in_world_date ASC, title ASC;
                )");
    query_timeline_events.bind(1, static_cast<int64_t>(campaign.id));
    while (query_timeline_events.executeStep())
    {
        TimelineEventModel event;
        if (!query_timeline_events.getColumn(0).isNull())
            event.metadata.id = query_timeline_events.getColumn(0).getString();
        event.title = query_timeline_events.getColumn(1).getString();
        if (!query_timeline_events.getColumn(2).isNull())
            event.description = query_timeline_events.getColumn(2).getString();
        if (!query_timeline_events.getColumn(3).isNull())
            event.inWorldDate = query_timeline_events.getColumn(3).getString();
        if (!query_timeline_events.getColumn(4).isNull())
            event.relatedEntityIds = deserializeStringList(query_timeline_events.getColumn(4).getString());
        if (!query_timeline_events.getColumn(5).isNull())
            event.metadata.tags = deserializeStringList(query_timeline_events.getColumn(5).getString());
        if (!query_timeline_events.getColumn(6).isNull())
            event.metadata.createdAtUtc = query_timeline_events.getColumn(6).getString();
        if (!query_timeline_events.getColumn(7).isNull())
            event.metadata.updatedAtUtc = query_timeline_events.getColumn(7).getString();
        campaign.timelineEvents.push_back(event);
    }
}
