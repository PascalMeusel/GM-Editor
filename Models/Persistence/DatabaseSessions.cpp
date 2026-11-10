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

void DatabaseManager::saveCampaignSessions(int64_t campaignId, const Campaign &campaign)
{
    deleteRowsMissingStableIds("sessions", campaignId, sessionIds(campaign.sessions));
    SQLite::Statement query_delete_session_notes(*m_db, R"(
            DELETE FROM session_notes
            WHERE session_id IN (SELECT id FROM sessions WHERE campaign_id = ?);
        )");
    query_delete_session_notes.bind(1, campaignId);
    query_delete_session_notes.exec();

    SQLite::Statement query_insert_session(*m_db, R"(
            INSERT INTO sessions
            (stable_id, campaign_id, title, scheduled_at_utc, started_at_utc, ended_at_utc, summary, recap, unresolved_threads, loot_awarded, xp_milestones, npcs_met, locations_visited, open_hooks, participant_character_ids, encounter_ids, story_document_ids, live_mode_active, tags, created_at, updated_at, rough_notes, plan_entries_json, live_state_json)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(stable_id) DO UPDATE SET
                campaign_id = excluded.campaign_id,
                title = excluded.title,
                scheduled_at_utc = excluded.scheduled_at_utc,
                started_at_utc = excluded.started_at_utc,
                ended_at_utc = excluded.ended_at_utc,
                summary = excluded.summary,
                recap = excluded.recap,
                unresolved_threads = excluded.unresolved_threads,
                loot_awarded = excluded.loot_awarded,
                xp_milestones = excluded.xp_milestones,
                npcs_met = excluded.npcs_met,
                locations_visited = excluded.locations_visited,
                open_hooks = excluded.open_hooks,
                participant_character_ids = excluded.participant_character_ids,
                encounter_ids = excluded.encounter_ids,
                story_document_ids = excluded.story_document_ids,
                live_mode_active = excluded.live_mode_active,
                rough_notes = excluded.rough_notes,
                plan_entries_json = excluded.plan_entries_json,
                live_state_json = excluded.live_state_json,
                tags = excluded.tags,
                updated_at = excluded.updated_at;
        )");
    SQLite::Statement query_session_id(*m_db, "SELECT id FROM sessions WHERE stable_id = ? LIMIT 1;");
    SQLite::Statement query_insert_session_note(*m_db, R"(
            INSERT INTO session_notes
            (stable_id, session_id, timestamp_utc, body, linked_entity_id, linked_entity_type, linked_entity_label, tags, created_at, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
        )");
    for (const SessionModel &session : campaign.sessions)
    {
        query_insert_session.bind(1, session.metadata.id);
        query_insert_session.bind(2, campaignId);
        query_insert_session.bind(3, session.title.empty() ? "Untitled Session" : session.title);
        query_insert_session.bind(4, session.scheduledAtUtc);
        query_insert_session.bind(5, session.startedAtUtc);
        query_insert_session.bind(6, session.endedAtUtc);
        query_insert_session.bind(7, session.summary);
        query_insert_session.bind(8, session.recap);
        query_insert_session.bind(9, session.unresolvedThreads);
        query_insert_session.bind(10, session.lootAwarded);
        query_insert_session.bind(11, session.xpMilestones);
        query_insert_session.bind(12, session.npcsMet);
        query_insert_session.bind(13, session.locationsVisited);
        query_insert_session.bind(14, session.openHooks);
        query_insert_session.bind(15, serializeStringList(session.participantCharacterIds));
        query_insert_session.bind(16, serializeStringList(session.encounterIds));
        query_insert_session.bind(17, serializeStringList(session.storyDocumentIds));
        query_insert_session.bind(18, session.liveModeActive ? 1 : 0);
        query_insert_session.bind(19, serializeStringList(session.metadata.tags));
        query_insert_session.bind(20, session.metadata.createdAtUtc);
        query_insert_session.bind(21, currentUtcTimestamp());
        query_insert_session.bind(22, session.roughNotes);
        query_insert_session.bind(23, SessionPlan::serializeEntries(session.planEntries));
        query_insert_session.bind(24, SessionPlan::serializeLiveState(session.liveState));
        query_insert_session.exec();
        query_insert_session.reset();

        query_session_id.bind(1, session.metadata.id);
        if (query_session_id.executeStep())
        {
            const int64_t sessionRowId = query_session_id.getColumn(0).getInt64();
            for (const SessionNoteModel &note : session.notes)
            {
                if (note.body.empty())
                    continue;
                query_insert_session_note.bind(1, note.metadata.id);
                query_insert_session_note.bind(2, sessionRowId);
                query_insert_session_note.bind(3, note.timestampUtc.empty() ? note.metadata.createdAtUtc
                                                                            : note.timestampUtc);
                query_insert_session_note.bind(4, note.body);
                query_insert_session_note.bind(5, note.linkedEntityId);
                query_insert_session_note.bind(6, note.linkedEntityType);
                query_insert_session_note.bind(7, note.linkedEntityLabel);
                query_insert_session_note.bind(8, serializeStringList(note.metadata.tags));
                query_insert_session_note.bind(9, note.metadata.createdAtUtc);
                query_insert_session_note.bind(10, currentUtcTimestamp());
                query_insert_session_note.exec();
                query_insert_session_note.reset();
            }
        }
        query_session_id.reset();
    }
}

void DatabaseManager::loadCampaignSessions(Campaign &campaign)
{
    SQLite::Statement query_sessions(*m_db, R"(
                    SELECT id, stable_id, title, scheduled_at_utc, started_at_utc, ended_at_utc, summary, recap, unresolved_threads, loot_awarded, xp_milestones, npcs_met, locations_visited, open_hooks, participant_character_ids, encounter_ids, story_document_ids, live_mode_active, tags, created_at, updated_at, rough_notes, plan_entries_json, live_state_json
                    FROM sessions
                    WHERE campaign_id = ?
                    ORDER BY live_mode_active DESC, COALESCE(started_at_utc, scheduled_at_utc, updated_at) DESC, title ASC;
                )");
    query_sessions.bind(1, static_cast<int64_t>(campaign.id));

    while (query_sessions.executeStep())
    {
        SessionModel session;
        const int64_t sessionRowId = query_sessions.getColumn(0).getInt64();
        if (!query_sessions.getColumn(1).isNull())
            session.metadata.id = query_sessions.getColumn(1).getString();
        session.title = query_sessions.getColumn(2).getString();
        if (!query_sessions.getColumn(3).isNull())
            session.scheduledAtUtc = query_sessions.getColumn(3).getString();
        if (!query_sessions.getColumn(4).isNull())
            session.startedAtUtc = query_sessions.getColumn(4).getString();
        if (!query_sessions.getColumn(5).isNull())
            session.endedAtUtc = query_sessions.getColumn(5).getString();
        if (!query_sessions.getColumn(6).isNull())
            session.summary = query_sessions.getColumn(6).getString();
        if (!query_sessions.getColumn(7).isNull())
            session.recap = query_sessions.getColumn(7).getString();
        if (!query_sessions.getColumn(8).isNull())
            session.unresolvedThreads = query_sessions.getColumn(8).getString();
        if (!query_sessions.getColumn(9).isNull())
            session.lootAwarded = query_sessions.getColumn(9).getString();
        if (!query_sessions.getColumn(10).isNull())
            session.xpMilestones = query_sessions.getColumn(10).getString();
        if (!query_sessions.getColumn(11).isNull())
            session.npcsMet = query_sessions.getColumn(11).getString();
        if (!query_sessions.getColumn(12).isNull())
            session.locationsVisited = query_sessions.getColumn(12).getString();
        if (!query_sessions.getColumn(13).isNull())
            session.openHooks = query_sessions.getColumn(13).getString();
        if (!query_sessions.getColumn(14).isNull())
            session.participantCharacterIds = deserializeStringList(query_sessions.getColumn(14).getString());
        if (!query_sessions.getColumn(15).isNull())
            session.encounterIds = deserializeStringList(query_sessions.getColumn(15).getString());
        if (!query_sessions.getColumn(16).isNull())
            session.storyDocumentIds = deserializeStringList(query_sessions.getColumn(16).getString());
        if (!query_sessions.getColumn(17).isNull())
            session.liveModeActive = query_sessions.getColumn(17).getInt() != 0;
        if (!query_sessions.getColumn(18).isNull())
            session.metadata.tags = deserializeStringList(query_sessions.getColumn(18).getString());
        if (!query_sessions.getColumn(19).isNull())
            session.metadata.createdAtUtc = query_sessions.getColumn(19).getString();
        if (!query_sessions.getColumn(20).isNull())
            session.metadata.updatedAtUtc = query_sessions.getColumn(20).getString();
        if (!query_sessions.getColumn(21).isNull())
            session.roughNotes = query_sessions.getColumn(21).getString();
        if (!query_sessions.getColumn(22).isNull())
            session.planEntries = SessionPlan::deserializeEntries(query_sessions.getColumn(22).getString());
        if (!query_sessions.getColumn(23).isNull())
            session.liveState = SessionPlan::deserializeLiveState(query_sessions.getColumn(23).getString());

        SQLite::Statement query_session_notes(*m_db, R"(
                        SELECT stable_id, timestamp_utc, body, linked_entity_id, linked_entity_type, linked_entity_label, tags, created_at, updated_at
                        FROM session_notes
                        WHERE session_id = ?
                        ORDER BY timestamp_utc ASC, id ASC;
                    )");
        query_session_notes.bind(1, sessionRowId);
        while (query_session_notes.executeStep())
        {
            SessionNoteModel note;
            if (!query_session_notes.getColumn(0).isNull())
                note.metadata.id = query_session_notes.getColumn(0).getString();
            if (!query_session_notes.getColumn(1).isNull())
                note.timestampUtc = query_session_notes.getColumn(1).getString();
            note.body = query_session_notes.getColumn(2).getString();
            if (!query_session_notes.getColumn(3).isNull())
                note.linkedEntityId = query_session_notes.getColumn(3).getString();
            if (!query_session_notes.getColumn(4).isNull())
                note.linkedEntityType = query_session_notes.getColumn(4).getString();
            if (!query_session_notes.getColumn(5).isNull())
                note.linkedEntityLabel = query_session_notes.getColumn(5).getString();
            if (!query_session_notes.getColumn(6).isNull())
                note.metadata.tags = deserializeStringList(query_session_notes.getColumn(6).getString());
            if (!query_session_notes.getColumn(7).isNull())
                note.metadata.createdAtUtc = query_session_notes.getColumn(7).getString();
            if (!query_session_notes.getColumn(8).isNull())
                note.metadata.updatedAtUtc = query_session_notes.getColumn(8).getString();
            session.notes.push_back(note);
        }
        campaign.sessions.push_back(session);
        campaign.sessionIds.push_back(session.metadata.id);
    }
}
