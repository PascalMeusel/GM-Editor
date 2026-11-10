#include "Models/DataBaseManager.hpp"

#include "Models/Persistence/DatabaseTabletopToolkitIdentityValidation.hpp"
#include "Models/Persistence/PersistenceSerialization.hpp"

#include <cstdint>
#include <vector>

using namespace PersistenceSerialization;

void DatabaseManager::initializeTabletopToolkitSchema()
{
    m_db->exec(R"(
        CREATE TABLE IF NOT EXISTS campaign_dice_macros (
            id INTEGER PRIMARY KEY,
            stable_id TEXT NOT NULL UNIQUE,
            campaign_id INTEGER NOT NULL,
            name TEXT NOT NULL,
            expression TEXT NOT NULL,
            notes TEXT,
            add_to_session_notes INTEGER NOT NULL DEFAULT 1,
            tags TEXT,
            created_at TEXT,
            updated_at TEXT,
            sort_order INTEGER NOT NULL DEFAULT 0,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
        CREATE TABLE IF NOT EXISTS story_objectives (
            id INTEGER PRIMARY KEY,
            stable_id TEXT NOT NULL UNIQUE,
            campaign_id INTEGER NOT NULL,
            title TEXT NOT NULL,
            player_summary TEXT,
            gm_notes TEXT,
            status INTEGER NOT NULL DEFAULT 0,
            player_visible INTEGER NOT NULL DEFAULT 0,
            story_thread_id TEXT,
            story_beat_id TEXT,
            assignee_character_ids TEXT,
            prerequisite_objective_ids TEXT,
            tags TEXT,
            created_at TEXT,
            updated_at TEXT,
            sort_order INTEGER NOT NULL DEFAULT 0,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
        CREATE TABLE IF NOT EXISTS story_clues (
            id INTEGER PRIMARY KEY,
            stable_id TEXT NOT NULL UNIQUE,
            campaign_id INTEGER NOT NULL,
            title TEXT NOT NULL,
            player_text TEXT,
            gm_truth TEXT,
            status INTEGER NOT NULL DEFAULT 0,
            player_visible INTEGER NOT NULL DEFAULT 0,
            source_entity_id TEXT,
            source_entity_type TEXT,
            discovered_at_utc TEXT,
            tags TEXT,
            created_at TEXT,
            updated_at TEXT,
            sort_order INTEGER NOT NULL DEFAULT 0,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
        CREATE TABLE IF NOT EXISTS campaign_safety_profiles (
            campaign_id INTEGER PRIMARY KEY,
            enabled INTEGER NOT NULL DEFAULT 1,
            pause_label TEXT NOT NULL DEFAULT 'Pause',
            pause_protocol TEXT,
            pause_active INTEGER NOT NULL DEFAULT 0,
            pause_requested_at_utc TEXT,
            check_in_active INTEGER NOT NULL DEFAULT 0,
            check_in_prompt TEXT,
            check_in_started_at_utc TEXT,
            updated_at TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
        CREATE TABLE IF NOT EXISTS campaign_safety_boundaries (
            id INTEGER PRIMARY KEY,
            stable_id TEXT NOT NULL UNIQUE,
            campaign_id INTEGER NOT NULL,
            kind INTEGER NOT NULL DEFAULT 0,
            topic TEXT NOT NULL,
            guidance TEXT,
            share_with_players INTEGER NOT NULL DEFAULT 0,
            active INTEGER NOT NULL DEFAULT 1,
            tags TEXT,
            created_at TEXT,
            updated_at TEXT,
            sort_order INTEGER NOT NULL DEFAULT 0,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
        CREATE TABLE IF NOT EXISTS campaign_safety_warnings (
            id INTEGER PRIMARY KEY,
            stable_id TEXT NOT NULL UNIQUE,
            campaign_id INTEGER NOT NULL,
            target_entity_id TEXT,
            target_entity_type TEXT,
            target_label TEXT,
            warning TEXT NOT NULL,
            share_with_players INTEGER NOT NULL DEFAULT 1,
            tags TEXT,
            created_at TEXT,
            updated_at TEXT,
            sort_order INTEGER NOT NULL DEFAULT 0,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
        CREATE INDEX IF NOT EXISTS idx_campaign_dice_macros_campaign ON campaign_dice_macros(campaign_id, sort_order);
        CREATE INDEX IF NOT EXISTS idx_story_objectives_campaign ON story_objectives(campaign_id, status, sort_order);
        CREATE INDEX IF NOT EXISTS idx_story_clues_campaign ON story_clues(campaign_id, status, sort_order);
        CREATE INDEX IF NOT EXISTS idx_safety_boundaries_campaign ON campaign_safety_boundaries(campaign_id, sort_order);
        CREATE INDEX IF NOT EXISTS idx_safety_warnings_target ON campaign_safety_warnings(campaign_id, target_entity_id);
    )");
}

void DatabaseManager::saveCampaignTabletopToolkit(int64_t campaignId, const Campaign &campaign)
{
    // Validate every collection before a reconciliation pass can delete a row.
    // The surrounding campaign transaction then preserves every other table too.
    TabletopToolkitPersistence::validateCampaignTabletopToolkitIdentities(*m_db, campaignId, campaign);
    std::vector<EntityId> macroIds, objectiveIds, clueIds, boundaryIds, warningIds;
    for (const auto &value : campaign.diceMacros) macroIds.push_back(value.metadata.id);
    for (const auto &value : campaign.storyObjectives) objectiveIds.push_back(value.metadata.id);
    for (const auto &value : campaign.storyClues) clueIds.push_back(value.metadata.id);
    for (const auto &value : campaign.safetyProfile.boundaries) boundaryIds.push_back(value.metadata.id);
    for (const auto &value : campaign.safetyProfile.warnings) warningIds.push_back(value.metadata.id);
    deleteRowsMissingStableIds("campaign_dice_macros", campaignId, macroIds);
    deleteRowsMissingStableIds("story_objectives", campaignId, objectiveIds);
    deleteRowsMissingStableIds("story_clues", campaignId, clueIds);
    deleteRowsMissingStableIds("campaign_safety_boundaries", campaignId, boundaryIds);
    deleteRowsMissingStableIds("campaign_safety_warnings", campaignId, warningIds);

    SQLite::Statement macro(*m_db, R"(
        INSERT INTO campaign_dice_macros
        (stable_id, campaign_id, name, expression, notes, add_to_session_notes, tags, created_at, updated_at, sort_order)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(stable_id) DO UPDATE SET campaign_id=excluded.campaign_id, name=excluded.name,
            expression=excluded.expression, notes=excluded.notes, add_to_session_notes=excluded.add_to_session_notes,
            tags=excluded.tags, updated_at=excluded.updated_at, sort_order=excluded.sort_order;
    )");
    for (std::size_t index = 0; index < campaign.diceMacros.size(); ++index)
    {
        const auto &value = campaign.diceMacros[index];
        macro.bind(1, value.metadata.id); macro.bind(2, campaignId); macro.bind(3, value.name);
        macro.bind(4, value.expression); macro.bind(5, value.notes); macro.bind(6, value.addToSessionNotes ? 1 : 0);
        macro.bind(7, serializeStringList(value.metadata.tags)); macro.bind(8, value.metadata.createdAtUtc);
        macro.bind(9, value.metadata.updatedAtUtc); macro.bind(10, static_cast<int>(index)); macro.exec(); macro.reset();
    }

    SQLite::Statement objective(*m_db, R"(
        INSERT INTO story_objectives
        (stable_id, campaign_id, title, player_summary, gm_notes, status, player_visible, story_thread_id,
         story_beat_id, assignee_character_ids, prerequisite_objective_ids, tags, created_at, updated_at, sort_order)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(stable_id) DO UPDATE SET campaign_id=excluded.campaign_id, title=excluded.title,
            player_summary=excluded.player_summary, gm_notes=excluded.gm_notes, status=excluded.status,
            player_visible=excluded.player_visible, story_thread_id=excluded.story_thread_id,
            story_beat_id=excluded.story_beat_id, assignee_character_ids=excluded.assignee_character_ids,
            prerequisite_objective_ids=excluded.prerequisite_objective_ids, tags=excluded.tags,
            updated_at=excluded.updated_at, sort_order=excluded.sort_order;
    )");
    for (std::size_t index = 0; index < campaign.storyObjectives.size(); ++index)
    {
        const auto &value = campaign.storyObjectives[index];
        objective.bind(1, value.metadata.id); objective.bind(2, campaignId); objective.bind(3, value.title);
        objective.bind(4, value.playerSummary); objective.bind(5, value.gmNotes);
        objective.bind(6, static_cast<int>(value.status)); objective.bind(7, value.playerVisible ? 1 : 0);
        objective.bind(8, value.linkedStoryThreadId); objective.bind(9, value.linkedStoryBeatId);
        objective.bind(10, serializeStringList(value.assigneeCharacterIds));
        objective.bind(11, serializeStringList(value.prerequisiteObjectiveIds));
        objective.bind(12, serializeStringList(value.metadata.tags)); objective.bind(13, value.metadata.createdAtUtc);
        objective.bind(14, value.metadata.updatedAtUtc); objective.bind(15, static_cast<int>(index));
        objective.exec(); objective.reset();
    }

    SQLite::Statement clue(*m_db, R"(
        INSERT INTO story_clues
        (stable_id, campaign_id, title, player_text, gm_truth, status, player_visible, source_entity_id,
         source_entity_type, discovered_at_utc, tags, created_at, updated_at, sort_order)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(stable_id) DO UPDATE SET campaign_id=excluded.campaign_id, title=excluded.title,
            player_text=excluded.player_text, gm_truth=excluded.gm_truth, status=excluded.status,
            player_visible=excluded.player_visible, source_entity_id=excluded.source_entity_id,
            source_entity_type=excluded.source_entity_type, discovered_at_utc=excluded.discovered_at_utc,
            tags=excluded.tags, updated_at=excluded.updated_at, sort_order=excluded.sort_order;
    )");
    for (std::size_t index = 0; index < campaign.storyClues.size(); ++index)
    {
        const auto &value = campaign.storyClues[index];
        clue.bind(1, value.metadata.id); clue.bind(2, campaignId); clue.bind(3, value.title);
        clue.bind(4, value.playerText); clue.bind(5, value.gmTruth); clue.bind(6, static_cast<int>(value.status));
        clue.bind(7, value.playerVisible ? 1 : 0); clue.bind(8, value.sourceEntityId);
        clue.bind(9, value.sourceEntityType); clue.bind(10, value.discoveredAtUtc);
        clue.bind(11, serializeStringList(value.metadata.tags)); clue.bind(12, value.metadata.createdAtUtc);
        clue.bind(13, value.metadata.updatedAtUtc); clue.bind(14, static_cast<int>(index)); clue.exec(); clue.reset();
    }

    SQLite::Statement profile(*m_db, R"(
        INSERT INTO campaign_safety_profiles
        (campaign_id, enabled, pause_label, pause_protocol, pause_active, pause_requested_at_utc,
         check_in_active, check_in_prompt, check_in_started_at_utc, updated_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(campaign_id) DO UPDATE SET enabled=excluded.enabled, pause_label=excluded.pause_label,
            pause_protocol=excluded.pause_protocol, pause_active=excluded.pause_active,
            pause_requested_at_utc=excluded.pause_requested_at_utc, check_in_active=excluded.check_in_active,
            check_in_prompt=excluded.check_in_prompt, check_in_started_at_utc=excluded.check_in_started_at_utc,
            updated_at=excluded.updated_at;
    )");
    const auto &safety = campaign.safetyProfile;
    profile.bind(1, campaignId); profile.bind(2, safety.enabled ? 1 : 0); profile.bind(3, safety.pauseLabel);
    profile.bind(4, safety.pauseProtocol); profile.bind(5, safety.pauseActive ? 1 : 0);
    profile.bind(6, safety.pauseRequestedAtUtc); profile.bind(7, safety.checkInActive ? 1 : 0);
    profile.bind(8, safety.checkInPrompt); profile.bind(9, safety.checkInStartedAtUtc);
    profile.bind(10, currentUtcTimestamp()); profile.exec();

    SQLite::Statement boundary(*m_db, R"(
        INSERT INTO campaign_safety_boundaries
        (stable_id, campaign_id, kind, topic, guidance, share_with_players, active, tags, created_at, updated_at, sort_order)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(stable_id) DO UPDATE SET campaign_id=excluded.campaign_id, kind=excluded.kind,
            topic=excluded.topic, guidance=excluded.guidance, share_with_players=excluded.share_with_players,
            active=excluded.active, tags=excluded.tags, updated_at=excluded.updated_at, sort_order=excluded.sort_order;
    )");
    for (std::size_t index = 0; index < safety.boundaries.size(); ++index)
    {
        const auto &value = safety.boundaries[index];
        boundary.bind(1, value.metadata.id); boundary.bind(2, campaignId); boundary.bind(3, static_cast<int>(value.kind));
        boundary.bind(4, value.topic); boundary.bind(5, value.guidance); boundary.bind(6, value.shareWithPlayers ? 1 : 0);
        boundary.bind(7, value.active ? 1 : 0); boundary.bind(8, serializeStringList(value.metadata.tags));
        boundary.bind(9, value.metadata.createdAtUtc); boundary.bind(10, value.metadata.updatedAtUtc);
        boundary.bind(11, static_cast<int>(index)); boundary.exec(); boundary.reset();
    }

    SQLite::Statement warning(*m_db, R"(
        INSERT INTO campaign_safety_warnings
        (stable_id, campaign_id, target_entity_id, target_entity_type, target_label, warning,
         share_with_players, tags, created_at, updated_at, sort_order)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(stable_id) DO UPDATE SET campaign_id=excluded.campaign_id,
            target_entity_id=excluded.target_entity_id, target_entity_type=excluded.target_entity_type,
            target_label=excluded.target_label, warning=excluded.warning,
            share_with_players=excluded.share_with_players, tags=excluded.tags,
            updated_at=excluded.updated_at, sort_order=excluded.sort_order;
    )");
    for (std::size_t index = 0; index < safety.warnings.size(); ++index)
    {
        const auto &value = safety.warnings[index];
        warning.bind(1, value.metadata.id); warning.bind(2, campaignId); warning.bind(3, value.targetEntityId);
        warning.bind(4, value.targetEntityType); warning.bind(5, value.targetLabel); warning.bind(6, value.warning);
        warning.bind(7, value.shareWithPlayers ? 1 : 0); warning.bind(8, serializeStringList(value.metadata.tags));
        warning.bind(9, value.metadata.createdAtUtc); warning.bind(10, value.metadata.updatedAtUtc);
        warning.bind(11, static_cast<int>(index)); warning.exec(); warning.reset();
    }
}

void DatabaseManager::loadCampaignTabletopToolkit(Campaign &campaign)
{
    campaign.diceMacros.clear(); campaign.storyObjectives.clear(); campaign.storyClues.clear();
    campaign.safetyProfile = CampaignSafetyProfileModel{};
    const auto campaignId = static_cast<std::int64_t>(campaign.id);

    SQLite::Statement macro(*m_db, "SELECT stable_id,name,expression,notes,add_to_session_notes,tags,created_at,updated_at FROM campaign_dice_macros WHERE campaign_id=? ORDER BY sort_order,id;");
    macro.bind(1, campaignId);
    while (macro.executeStep())
    {
        CampaignDiceMacroModel value; value.metadata.id=macro.getColumn(0).getString(); value.name=macro.getColumn(1).getString();
        value.expression=macro.getColumn(2).getString(); value.notes=macro.getColumn(3).getString();
        value.addToSessionNotes=macro.getColumn(4).getInt()!=0; value.metadata.tags=deserializeStringList(macro.getColumn(5).getString());
        value.metadata.createdAtUtc=macro.getColumn(6).getString(); value.metadata.updatedAtUtc=macro.getColumn(7).getString();
        campaign.diceMacros.push_back(std::move(value));
    }

    SQLite::Statement objective(*m_db, "SELECT stable_id,title,player_summary,gm_notes,status,player_visible,story_thread_id,story_beat_id,assignee_character_ids,prerequisite_objective_ids,tags,created_at,updated_at FROM story_objectives WHERE campaign_id=? ORDER BY sort_order,id;");
    objective.bind(1, campaignId);
    while (objective.executeStep())
    {
        StoryObjectiveModel value; value.metadata.id=objective.getColumn(0).getString(); value.title=objective.getColumn(1).getString();
        value.playerSummary=objective.getColumn(2).getString(); value.gmNotes=objective.getColumn(3).getString();
        value.status=static_cast<StoryObjectiveStatus>(objective.getColumn(4).getInt()); value.playerVisible=objective.getColumn(5).getInt()!=0;
        value.linkedStoryThreadId=objective.getColumn(6).getString(); value.linkedStoryBeatId=objective.getColumn(7).getString();
        value.assigneeCharacterIds=deserializeStringList(objective.getColumn(8).getString());
        value.prerequisiteObjectiveIds=deserializeStringList(objective.getColumn(9).getString());
        value.metadata.tags=deserializeStringList(objective.getColumn(10).getString());
        value.metadata.createdAtUtc=objective.getColumn(11).getString(); value.metadata.updatedAtUtc=objective.getColumn(12).getString();
        campaign.storyObjectives.push_back(std::move(value));
    }

    SQLite::Statement clue(*m_db, "SELECT stable_id,title,player_text,gm_truth,status,player_visible,source_entity_id,source_entity_type,discovered_at_utc,tags,created_at,updated_at FROM story_clues WHERE campaign_id=? ORDER BY sort_order,id;");
    clue.bind(1, campaignId);
    while (clue.executeStep())
    {
        StoryClueModel value; value.metadata.id=clue.getColumn(0).getString(); value.title=clue.getColumn(1).getString();
        value.playerText=clue.getColumn(2).getString(); value.gmTruth=clue.getColumn(3).getString();
        value.status=static_cast<StoryClueStatus>(clue.getColumn(4).getInt()); value.playerVisible=clue.getColumn(5).getInt()!=0;
        value.sourceEntityId=clue.getColumn(6).getString(); value.sourceEntityType=clue.getColumn(7).getString();
        value.discoveredAtUtc=clue.getColumn(8).getString(); value.metadata.tags=deserializeStringList(clue.getColumn(9).getString());
        value.metadata.createdAtUtc=clue.getColumn(10).getString(); value.metadata.updatedAtUtc=clue.getColumn(11).getString();
        campaign.storyClues.push_back(std::move(value));
    }

    SQLite::Statement profile(*m_db, "SELECT enabled,pause_label,pause_protocol,pause_active,pause_requested_at_utc,check_in_active,check_in_prompt,check_in_started_at_utc FROM campaign_safety_profiles WHERE campaign_id=?;");
    profile.bind(1, campaignId);
    if (profile.executeStep())
    {
        auto &value=campaign.safetyProfile; value.enabled=profile.getColumn(0).getInt()!=0;
        value.pauseLabel=profile.getColumn(1).getString(); value.pauseProtocol=profile.getColumn(2).getString();
        value.pauseActive=profile.getColumn(3).getInt()!=0; value.pauseRequestedAtUtc=profile.getColumn(4).getString();
        value.checkInActive=profile.getColumn(5).getInt()!=0; value.checkInPrompt=profile.getColumn(6).getString();
        value.checkInStartedAtUtc=profile.getColumn(7).getString();
    }

    SQLite::Statement boundary(*m_db, "SELECT stable_id,kind,topic,guidance,share_with_players,active,tags,created_at,updated_at FROM campaign_safety_boundaries WHERE campaign_id=? ORDER BY sort_order,id;");
    boundary.bind(1, campaignId);
    while (boundary.executeStep())
    {
        SafetyBoundaryModel value; value.metadata.id=boundary.getColumn(0).getString(); value.kind=static_cast<SafetyBoundaryKind>(boundary.getColumn(1).getInt());
        value.topic=boundary.getColumn(2).getString(); value.guidance=boundary.getColumn(3).getString();
        value.shareWithPlayers=boundary.getColumn(4).getInt()!=0; value.active=boundary.getColumn(5).getInt()!=0;
        value.metadata.tags=deserializeStringList(boundary.getColumn(6).getString()); value.metadata.createdAtUtc=boundary.getColumn(7).getString();
        value.metadata.updatedAtUtc=boundary.getColumn(8).getString(); campaign.safetyProfile.boundaries.push_back(std::move(value));
    }

    SQLite::Statement warning(*m_db, "SELECT stable_id,target_entity_id,target_entity_type,target_label,warning,share_with_players,tags,created_at,updated_at FROM campaign_safety_warnings WHERE campaign_id=? ORDER BY sort_order,id;");
    warning.bind(1, campaignId);
    while (warning.executeStep())
    {
        SafetyContentWarningModel value; value.metadata.id=warning.getColumn(0).getString(); value.targetEntityId=warning.getColumn(1).getString();
        value.targetEntityType=warning.getColumn(2).getString(); value.targetLabel=warning.getColumn(3).getString();
        value.warning=warning.getColumn(4).getString(); value.shareWithPlayers=warning.getColumn(5).getInt()!=0;
        value.metadata.tags=deserializeStringList(warning.getColumn(6).getString()); value.metadata.createdAtUtc=warning.getColumn(7).getString();
        value.metadata.updatedAtUtc=warning.getColumn(8).getString(); campaign.safetyProfile.warnings.push_back(std::move(value));
    }
}
