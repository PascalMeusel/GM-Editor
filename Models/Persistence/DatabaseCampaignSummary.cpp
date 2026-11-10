#include "Models/Rulesets/RulesetResolution.hpp"
#include "Models/DataBaseManager.hpp"
#include <cstddef>
#include <exception>
#include <string>
#include <utility>
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

std::vector<Campaign> DatabaseManager::loadCampaignSummaries()
{
    std::vector<Campaign> campaigns;
    try
    {
        SQLite::Statement query_campaigns(*m_db, R"(
            SELECT c.id, c.stable_id, c.name, c.ruleset_name, c.description, c.tags, c.created_at, c.updated_at,
                   c.ruleset_id, c.ruleset_version, c.ruleset_overlay_id, c.ruleset_overlay_version,
                   (SELECT COUNT(*) FROM characters WHERE campaign_id = c.id) AS character_count,
                   (SELECT COUNT(*) FROM npcs WHERE campaign_id = c.id) AS npc_count,
                   (SELECT COUNT(*) FROM custom_monsters WHERE campaign_id = c.id) AS monster_count,
                   (SELECT COUNT(*) FROM maps WHERE campaign_id = c.id) AS map_count,
                   (SELECT COUNT(*) FROM story_threads WHERE campaign_id = c.id) AS story_count,
                   (SELECT COUNT(*) FROM encounters WHERE campaign_id = c.id) AS encounter_count,
                   (SELECT COUNT(*) FROM sessions WHERE campaign_id = c.id) AS session_count,
                   c.content_language
            FROM campaigns c
            ORDER BY c.updated_at DESC, c.name ASC;
        )");

        while (query_campaigns.executeStep())
        {
            Campaign campaign;
            campaign.id = query_campaigns.getColumn(0).getInt64();
            if (!query_campaigns.getColumn(1).isNull())
                campaign.metadata.id = query_campaigns.getColumn(1).getString();
            campaign.name = query_campaigns.getColumn(2).getString();
            const std::string rulesetName = query_campaigns.getColumn(3).getString();
            if (!query_campaigns.getColumn(4).isNull())
                campaign.description = query_campaigns.getColumn(4).getString();
            if (!query_campaigns.getColumn(5).isNull())
                campaign.metadata.tags = deserializeStringList(query_campaigns.getColumn(5).getString());
            if (!query_campaigns.getColumn(6).isNull())
                campaign.metadata.createdAtUtc = query_campaigns.getColumn(6).getString();
            if (!query_campaigns.getColumn(7).isNull())
                campaign.metadata.updatedAtUtc = query_campaigns.getColumn(7).getString();

            campaign.rulesetReference.rulesetId =
                query_campaigns.getColumn(8).isNull() ? std::string{} : query_campaigns.getColumn(8).getString();
            campaign.rulesetReference.version =
                query_campaigns.getColumn(9).isNull() ? std::string{} : query_campaigns.getColumn(9).getString();
            campaign.rulesetReference.overlayId =
                query_campaigns.getColumn(10).isNull() ? std::string{} : query_campaigns.getColumn(10).getString();
            campaign.rulesetReference.overlayVersion =
                query_campaigns.getColumn(11).isNull() ? std::string{} : query_campaigns.getColumn(11).getString();

            campaign.ruleSet = createRuleSetFromName(rulesetName);
            if (noRulesetPins(campaign.rulesetReference) && campaign.ruleSet)
                campaign.rulesetReference = {campaign.ruleSet->getId(), campaign.ruleSet->getVersionId(), {}, {}};
            campaign.characters.resize(static_cast<std::size_t>(std::max(0, query_campaigns.getColumn(12).getInt())));
            campaign.npcs.resize(static_cast<std::size_t>(std::max(0, query_campaigns.getColumn(13).getInt())));
            campaign.customMonsters.resize(
                static_cast<std::size_t>(std::max(0, query_campaigns.getColumn(14).getInt())));
            campaign.maps.resize(static_cast<std::size_t>(std::max(0, query_campaigns.getColumn(15).getInt())));
            campaign.storyThreads.resize(static_cast<std::size_t>(std::max(0, query_campaigns.getColumn(16).getInt())));
            campaign.encounters.resize(static_cast<std::size_t>(std::max(0, query_campaigns.getColumn(17).getInt())));
            campaign.sessions.resize(static_cast<std::size_t>(std::max(0, query_campaigns.getColumn(18).getInt())));
            if (!query_campaigns.getColumn(19).isNull() && !query_campaigns.getColumn(19).getString().empty())
                campaign.contentLanguage = query_campaigns.getColumn(19).getString() == "de" ? "de" : "en";

            SQLite::Statement query_live_session(*m_db, R"(
                SELECT id, stable_id, title, live_mode_active
                FROM sessions
                WHERE campaign_id = ? AND live_mode_active = 1
                ORDER BY COALESCE(started_at_utc, scheduled_at_utc, updated_at, created_at, '') DESC
                LIMIT 1;
            )");
            query_live_session.bind(1, static_cast<int64_t>(campaign.id));
            if (query_live_session.executeStep())
            {
                const int64_t sessionRowId = query_live_session.getColumn(0).getInt64();
                SessionModel session;
                if (!query_live_session.getColumn(1).isNull())
                    session.metadata.id = query_live_session.getColumn(1).getString();
                if (!query_live_session.getColumn(2).isNull())
                    session.title = query_live_session.getColumn(2).getString();
                if (!query_live_session.getColumn(3).isNull())
                    session.liveModeActive = query_live_session.getColumn(3).getInt() != 0;

                SQLite::Statement query_session_notes(*m_db, R"(
                    SELECT body
                    FROM session_notes
                    WHERE session_id = ?
                    ORDER BY id ASC;
                )");
                query_session_notes.bind(1, sessionRowId);
                while (query_session_notes.executeStep())
                {
                    SessionNoteModel note;
                    if (!query_session_notes.getColumn(0).isNull())
                        note.body = query_session_notes.getColumn(0).getString();
                    session.notes.push_back(std::move(note));
                }

                if (campaign.sessions.empty())
                    campaign.sessions.push_back(std::move(session));
                else
                    campaign.sessions.front() = std::move(session);
            }

            SQLite::Statement query_live_encounter(*m_db, R"(
                SELECT stable_id, name, round, active_participant_index, combat_paused, combat_phase, combat_log
                FROM encounters
                WHERE campaign_id = ?
                  AND (combat_paused != 0 OR combat_phase IN ('active_turn', 'movement', 'action_resolution', 'reactions_effects', 'end_turn', 'paused_break', 'wrap_up') OR IFNULL(round, 1) > 1 OR IFNULL(active_participant_index, 0) > 0 OR IFNULL(combat_log, '') <> '')
                ORDER BY combat_paused DESC, COALESCE(updated_at, created_at, '') DESC, name ASC
                LIMIT 1;
            )");
            query_live_encounter.bind(1, static_cast<int64_t>(campaign.id));
            if (query_live_encounter.executeStep())
            {
                EncounterModel encounter;
                if (!query_live_encounter.getColumn(0).isNull())
                    encounter.metadata.id = query_live_encounter.getColumn(0).getString();
                if (!query_live_encounter.getColumn(1).isNull())
                    encounter.name = query_live_encounter.getColumn(1).getString();
                if (!query_live_encounter.getColumn(2).isNull())
                    encounter.round = query_live_encounter.getColumn(2).getInt();
                if (!query_live_encounter.getColumn(3).isNull())
                    encounter.activeParticipantIndex = query_live_encounter.getColumn(3).getInt();
                if (!query_live_encounter.getColumn(4).isNull())
                    encounter.combatPaused = query_live_encounter.getColumn(4).getInt() != 0;
                if (!query_live_encounter.getColumn(6).isNull() &&
                    !query_live_encounter.getColumn(6).getString().empty())
                    encounter.combatLog.push_back(CombatLogEntryModel{});
                if (!query_live_encounter.getColumn(5).isNull() &&
                    !query_live_encounter.getColumn(5).getString().empty())
                    encounter.combatPhase = combatPhaseFromStorageValue(query_live_encounter.getColumn(5).getString());
                else
                    encounter.combatPhase = inferCombatPhaseFromLegacyState(encounter);

                if (campaign.encounters.empty())
                    campaign.encounters.push_back(std::move(encounter));
                else
                    campaign.encounters.front() = std::move(encounter);
            }

            campaigns.push_back(std::move(campaign));
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Fehler beim Laden der Kampagnenuebersicht: " << e.what() << std::endl;
        ProjectLogger::getInstance().error("DatabaseManager",
                                           std::string("Could not load campaign summaries: ") + e.what());
    }
    return campaigns;
}
