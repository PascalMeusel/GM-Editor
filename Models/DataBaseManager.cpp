#include "Models/Rulesets/RulesetResolution.hpp"
#include "DataBaseManager.hpp"
#include "Models/Persistence/DatabaseRows.hpp"
#include "Models/Persistence/DatabaseSnapshotReadCopy.hpp"
#include "Models/Persistence/DatabaseCampaignDelete.hpp"
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
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>
#include "Rulesets/Clockwork/ClockworkBloodline.hpp"
#include "Rulesets/DND/DND5e.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

std::shared_ptr<DatabaseManager> DatabaseManager::instance = nullptr;

using namespace PersistenceSerialization;

namespace
{

std::filesystem::path executableDirectoryForPersistence()
{
#if defined(_WIN32)
    std::wstring buffer(MAX_PATH, L'\0');
    DWORD size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    while (size == buffer.size())
    {
        buffer.resize(buffer.size() * 2);
        size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    }

    if (size > 0)
    {
        buffer.resize(size);
        return std::filesystem::path(buffer).parent_path();
    }
#else
    std::vector<char> buffer(4096);
    ssize_t size = readlink("/proc/self/exe", buffer.data(), buffer.size());
    while (size == static_cast<ssize_t>(buffer.size()))
    {
        buffer.resize(buffer.size() * 2);
        size = readlink("/proc/self/exe", buffer.data(), buffer.size());
    }

    if (size > 0)
        return std::filesystem::path(std::string(buffer.data(), static_cast<std::size_t>(size))).parent_path();
#endif
    return {};
}

std::filesystem::path defaultDatabasePath()
{
    return ApplicationPaths::databasePath();
}

}

void DatabaseManager::deleteRowsMissingStableIds(const std::string &tableName, int64_t campaignId,
                                                 const std::vector<EntityId> &stableIds)
{
    if (stableIds.empty())
    {
        SQLite::Statement deleteAll(*m_db, "DELETE FROM " + tableName + " WHERE campaign_id = ?;");
        deleteAll.bind(1, campaignId);
        deleteAll.exec();
        return;
    }

    std::string placeholders;
    for (std::size_t index = 0; index < stableIds.size(); ++index)
    {
        if (index > 0)
            placeholders += ",";
        placeholders += "?";
    }

    SQLite::Statement deleteMissing(*m_db, "DELETE FROM " + tableName +
                                               " WHERE campaign_id = ? AND (stable_id IS NULL OR stable_id NOT IN (" +
                                               placeholders + "));");
    deleteMissing.bind(1, campaignId);
    for (std::size_t index = 0; index < stableIds.size(); ++index)
        deleteMissing.bind(static_cast<int>(index + 2), stableIds[index]);
    deleteMissing.exec();
}

std::shared_ptr<DatabaseManager> DatabaseManager::getInstance()
{
    if (instance == nullptr)
        instance = std::shared_ptr<DatabaseManager>(new DatabaseManager(defaultDatabasePath().u8string()));

    return instance;
}

bool DatabaseManager::readCampaignFromSnapshot(const std::string &snapshotPath, const std::string &campaignStableId,
                                               Campaign &campaign, std::string &errorMessage)
{
    try
    {
        DatabaseSnapshotReadCopy readCopy(std::filesystem::u8path(snapshotPath));
        DatabaseManager snapshot(readCopy.path().u8string(), true);
        std::vector<Campaign> campaigns = snapshot.loadAllCampaigns();
        const auto found = std::find_if(campaigns.begin(), campaigns.end(), [&](const Campaign &candidate) {
            return candidate.metadata.id == campaignStableId;
        });
        if (found == campaigns.end())
        {
            errorMessage = "The package database does not contain the campaign declared by its manifest.";
            return false;
        }

        Campaign loadedCampaign = *found;
        const std::vector<RulesetDefinitionModel> installedDefinitions = snapshot.loadInstalledRulesetDefinitions();
        auto availableDefinitions = loadedCampaign.rulesetDefinitions;
        availableDefinitions.insert(availableDefinitions.end(), installedDefinitions.begin(), installedDefinitions.end());
        const auto copyReferencedDefinition = [&](const std::string &rulesetId, const std::string &version, bool overlay) {
            if (rulesetId.empty()) return;
            SpellcastingValues::Reason reason;
            const auto *definition = findPinnedRulesetDefinition(availableDefinitions, rulesetId, version, overlay, reason);
            if (!definition)
            {
                if (reason == SpellcastingValues::Reason::AmbiguousBase || reason == SpellcastingValues::Reason::AmbiguousOverlay ||
                    reason == SpellcastingValues::Reason::InvalidDefinition)
                    throw std::runtime_error("The backup contains ambiguous or invalid pinned rules. Resolve that dependency before restoring: " + rulesetId + " " + version);
                return; // Preserve a missing pin as unresolved; never invent a version.
            }
            if (findPinnedRulesetDefinition(loadedCampaign.rulesetDefinitions, rulesetId, version, overlay, reason)) return;
            if (std::any_of(loadedCampaign.rulesetDefinitions.begin(), loadedCampaign.rulesetDefinitions.end(),
                [&](const auto &candidate) { return candidate.metadata.id == definition->metadata.id; }))
                throw std::runtime_error("The backup contains a conflicting rules definition identity: " + definition->metadata.id);
            loadedCampaign.rulesetDefinitions.push_back(*definition);
        };
        copyReferencedDefinition(loadedCampaign.rulesetReference.rulesetId, loadedCampaign.rulesetReference.version, false);
        copyReferencedDefinition(loadedCampaign.rulesetReference.overlayId, loadedCampaign.rulesetReference.overlayVersion, true);
        campaign = std::move(loadedCampaign);
        errorMessage.clear();
        return true;
    }
    catch (const std::exception &e)
    {
        errorMessage = e.what();
        return false;
    }
}

void DatabaseManager::saveCampaign(const Campaign &campaign)
{
    try
    {
        if (!campaign.ruleSet)
            throw std::runtime_error("Cannot save campaign without a ruleset.");

        // Campaign rows and their replacement/deletion passes form one durable
        // unit. Helpers below use this connection without opening transactions;
        // any statement failure unwinds them before this guard rolls back.
        SQLite::Transaction transaction(*m_db, SQLite::TransactionBehavior::IMMEDIATE);

        RulesetReference savedRuleset = campaign.rulesetReference;
        if (noRulesetPins(savedRuleset))
            savedRuleset = {campaign.ruleSet->getId(), campaign.ruleSet->getVersionId(), {}, {}};

        int64_t campaignId = -1;
        CampaignClockModel savedClock = campaign.clock;
        InWorldTime::normalizeClock(savedClock);
        SQLite::Statement find_campaign(*m_db, "SELECT id FROM campaigns WHERE stable_id = ? OR name = ? LIMIT 1;");
        find_campaign.bind(1, campaign.metadata.id);
        find_campaign.bind(2, campaign.name);
        if (find_campaign.executeStep())
            campaignId = find_campaign.getColumn(0).getInt64();

        if (campaignId >= 0)
        {
            SQLite::Statement update_campaign(*m_db, R"(
                UPDATE campaigns
                SET stable_id = ?, name = ?, ruleset_name = ?, description = ?, tags = ?, created_at = ?, updated_at = ?, party_map_id = ?, party_x = ?, party_y = ?, party_vehicles = ?, party_strongholds = ?, party_renown = ?, party_reputation = ?, travel_route_id = ?, travel_route_name = ?, travel_pace = ?, travel_distance_miles = ?, travel_miles_traveled = ?, travel_watches = ?, travel_rations_used = ?, travel_random_encounter_checks = ?, travel_notes = ?, audio_master_volume = ?, audio_music_volume = ?, audio_ambience_volume = ?, audio_effects_volume = ?, audio_voice_volume = ?, presentation_mode_active = ?, calendar_name = ?, calendar_month_names = ?, calendar_month_lengths = ?, calendar_weekday_names = ?, calendar_hours_per_day = ?, calendar_daylight_hours = ?, calendar_night_hours = ?, calendar_day_starts_at_hour = ?, clock_use_system_time = ?, clock_running = ?, clock_multiplier = ?, clock_anchor_minutes = ?, clock_anchor_utc = ?, ruleset_id = ?, ruleset_version = ?, ruleset_overlay_id = ?, ruleset_overlay_version = ?, content_language = ?, last_modified = CURRENT_TIMESTAMP
                WHERE id = ?;
            )");
            update_campaign.bind(1, campaign.metadata.id);
            update_campaign.bind(2, campaign.name);
            update_campaign.bind(3, campaign.ruleSet->getVersionId());
            update_campaign.bind(4, campaign.description);
            update_campaign.bind(5, serializeStringList(campaign.metadata.tags));
            update_campaign.bind(6, campaign.metadata.createdAtUtc);
            update_campaign.bind(7, currentUtcTimestamp());
            update_campaign.bind(8, campaign.partyLocation.mapId);
            update_campaign.bind(9, campaign.partyLocation.x);
            update_campaign.bind(10, campaign.partyLocation.y);
            update_campaign.bind(11, serializeStringList(campaign.partyVehicles));
            update_campaign.bind(12, serializeStringList(campaign.partyStrongholds));
            update_campaign.bind(13, campaign.partyRenown);
            update_campaign.bind(14, campaign.partyReputation);
            update_campaign.bind(15, campaign.partyTravel.routeId);
            update_campaign.bind(16, campaign.partyTravel.routeName);
            update_campaign.bind(17, campaign.partyTravel.travelPace);
            update_campaign.bind(18, campaign.partyTravel.distanceMiles);
            update_campaign.bind(19, campaign.partyTravel.milesTraveled);
            update_campaign.bind(20, campaign.partyTravel.watches);
            update_campaign.bind(21, campaign.partyTravel.rationsUsed);
            update_campaign.bind(22, campaign.partyTravel.randomEncounterChecks);
            update_campaign.bind(23, campaign.partyTravel.notes);
            update_campaign.bind(24, campaign.audioVolumeGroups.master);
            update_campaign.bind(25, campaign.audioVolumeGroups.music);
            update_campaign.bind(26, campaign.audioVolumeGroups.ambience);
            update_campaign.bind(27, campaign.audioVolumeGroups.effects);
            update_campaign.bind(28, campaign.audioVolumeGroups.voice);
            update_campaign.bind(29, campaign.presentationModeActive ? 1 : 0);
            update_campaign.bind(30, savedClock.calendar.name);
            update_campaign.bind(31, serializeStringList(monthNames(savedClock.calendar)));
            update_campaign.bind(32, serializeStringList(monthLengths(savedClock.calendar)));
            update_campaign.bind(33, serializeStringList(savedClock.calendar.weekdays));
            update_campaign.bind(34, savedClock.calendar.hoursPerDay);
            update_campaign.bind(35, savedClock.calendar.daylightHours);
            update_campaign.bind(36, savedClock.calendar.nightHours);
            update_campaign.bind(37, savedClock.calendar.dayStartsAtHour);
            update_campaign.bind(38, savedClock.useSystemTime ? 1 : 0);
            update_campaign.bind(39, savedClock.running ? 1 : 0);
            update_campaign.bind(40, savedClock.multiplier);
            update_campaign.bind(41, static_cast<std::int64_t>(savedClock.anchorAbsoluteMinutes));
            update_campaign.bind(42, savedClock.anchorUtc);
            update_campaign.bind(43, savedRuleset.rulesetId);
            update_campaign.bind(44, savedRuleset.version);
            update_campaign.bind(45, savedRuleset.overlayId);
            update_campaign.bind(46, savedRuleset.overlayVersion);
            update_campaign.bind(47, campaign.contentLanguage == "de" ? "de" : "en");
            update_campaign.bind(48, campaignId);
            update_campaign.exec();
        }
        else
        {
            SQLite::Statement insert_campaign(*m_db, R"(
                INSERT INTO campaigns (stable_id, name, ruleset_name, description, tags, created_at, updated_at, party_map_id, party_x, party_y, party_vehicles, party_strongholds, party_renown, party_reputation, travel_route_id, travel_route_name, travel_pace, travel_distance_miles, travel_miles_traveled, travel_watches, travel_rations_used, travel_random_encounter_checks, travel_notes, audio_master_volume, audio_music_volume, audio_ambience_volume, audio_effects_volume, audio_voice_volume, presentation_mode_active, calendar_name, calendar_month_names, calendar_month_lengths, calendar_weekday_names, calendar_hours_per_day, calendar_daylight_hours, calendar_night_hours, calendar_day_starts_at_hour, clock_use_system_time, clock_running, clock_multiplier, clock_anchor_minutes, clock_anchor_utc, ruleset_id, ruleset_version, ruleset_overlay_id, ruleset_overlay_version, content_language)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
            )");
            insert_campaign.bind(1, campaign.metadata.id);
            insert_campaign.bind(2, campaign.name);
            insert_campaign.bind(3, campaign.ruleSet->getVersionId());
            insert_campaign.bind(4, campaign.description);
            insert_campaign.bind(5, serializeStringList(campaign.metadata.tags));
            insert_campaign.bind(6, campaign.metadata.createdAtUtc);
            insert_campaign.bind(7, campaign.metadata.updatedAtUtc);
            insert_campaign.bind(8, campaign.partyLocation.mapId);
            insert_campaign.bind(9, campaign.partyLocation.x);
            insert_campaign.bind(10, campaign.partyLocation.y);
            insert_campaign.bind(11, serializeStringList(campaign.partyVehicles));
            insert_campaign.bind(12, serializeStringList(campaign.partyStrongholds));
            insert_campaign.bind(13, campaign.partyRenown);
            insert_campaign.bind(14, campaign.partyReputation);
            insert_campaign.bind(15, campaign.partyTravel.routeId);
            insert_campaign.bind(16, campaign.partyTravel.routeName);
            insert_campaign.bind(17, campaign.partyTravel.travelPace);
            insert_campaign.bind(18, campaign.partyTravel.distanceMiles);
            insert_campaign.bind(19, campaign.partyTravel.milesTraveled);
            insert_campaign.bind(20, campaign.partyTravel.watches);
            insert_campaign.bind(21, campaign.partyTravel.rationsUsed);
            insert_campaign.bind(22, campaign.partyTravel.randomEncounterChecks);
            insert_campaign.bind(23, campaign.partyTravel.notes);
            insert_campaign.bind(24, campaign.audioVolumeGroups.master);
            insert_campaign.bind(25, campaign.audioVolumeGroups.music);
            insert_campaign.bind(26, campaign.audioVolumeGroups.ambience);
            insert_campaign.bind(27, campaign.audioVolumeGroups.effects);
            insert_campaign.bind(28, campaign.audioVolumeGroups.voice);
            insert_campaign.bind(29, campaign.presentationModeActive ? 1 : 0);
            insert_campaign.bind(30, savedClock.calendar.name);
            insert_campaign.bind(31, serializeStringList(monthNames(savedClock.calendar)));
            insert_campaign.bind(32, serializeStringList(monthLengths(savedClock.calendar)));
            insert_campaign.bind(33, serializeStringList(savedClock.calendar.weekdays));
            insert_campaign.bind(34, savedClock.calendar.hoursPerDay);
            insert_campaign.bind(35, savedClock.calendar.daylightHours);
            insert_campaign.bind(36, savedClock.calendar.nightHours);
            insert_campaign.bind(37, savedClock.calendar.dayStartsAtHour);
            insert_campaign.bind(38, savedClock.useSystemTime ? 1 : 0);
            insert_campaign.bind(39, savedClock.running ? 1 : 0);
            insert_campaign.bind(40, savedClock.multiplier);
            insert_campaign.bind(41, static_cast<std::int64_t>(savedClock.anchorAbsoluteMinutes));
            insert_campaign.bind(42, savedClock.anchorUtc);
            insert_campaign.bind(43, savedRuleset.rulesetId);
            insert_campaign.bind(44, savedRuleset.version);
            insert_campaign.bind(45, savedRuleset.overlayId);
            insert_campaign.bind(46, savedRuleset.overlayVersion);
            insert_campaign.bind(47, campaign.contentLanguage == "de" ? "de" : "en");
            insert_campaign.exec();
            campaignId = m_db->getLastInsertRowid();
        }

        saveCampaignPlayerDisplay(campaignId, campaign);
        saveCampaignRulesets(campaignId, campaign);
        saveCampaignCampaignPreferences(campaignId, campaign);
        saveCampaignPartyInventory(campaignId, campaign);
        saveCampaignCharacters(campaignId, campaign);
        saveCampaignFactionsAndDeities(campaignId, campaign);
        saveCampaignNationsAndPlaces(campaignId, campaign);
        saveCampaignNpcs(campaignId, campaign);
        saveCampaignItems(campaignId, campaign);
        saveCampaignShopsAndLoot(campaignId, campaign);
        saveCampaignDungeons(campaignId, campaign);
        saveCampaignHandouts(campaignId, campaign);
        saveCampaignSessions(campaignId, campaign);
        saveCampaignSpellsAndMonsters(campaignId, campaign);
        saveCampaignMaps(campaignId, campaign);
        saveCampaignAudio(campaignId, campaign);
        saveCampaignEncounters(campaignId, campaign);
        saveCampaignStory(campaignId, campaign);
        saveCampaignTimeline(campaignId, campaign);
        saveCampaignRandomTables(campaignId, campaign);
        saveCampaignTabletopToolkit(campaignId, campaign);
        saveCampaignEntityProperties(campaignId, campaign);
        // ProjectSaveService creates recovery snapshots and emits saved events
        // only after this method returns, when the database is fully committed.
        transaction.commit();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Fehler beim Speichern der Kampagne '" << campaign.name << "': " << e.what() << std::endl;
        ProjectLogger::getInstance().error("DatabaseManager",
                                           std::string("Could not save campaign '") + campaign.name + "': " + e.what());
        throw;
    }
}

void DatabaseManager::deleteCampaignByStableId(const std::string &campaignStableId)
{
    if (campaignStableId.empty())
        return;
    CampaignDeletion::deleteCampaign(*m_db, campaignStableId);
    if (currentCampaign.metadata.id == campaignStableId)
        currentCampaign = Campaign{};
}

void DatabaseManager::writeSnapshot(const std::string &snapshotPath)
{
    std::string escapedPath;
    escapedPath.reserve(snapshotPath.size());
    for (char character : snapshotPath)
    {
        if (character == '\'')
            escapedPath += "''";
        else
            escapedPath += character;
    }

    m_db->exec("VACUUM main INTO '" + escapedPath + "';");
}
