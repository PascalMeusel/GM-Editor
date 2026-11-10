#include "Models/Rulesets/RulesetResolution.hpp"
#include "DataBaseManager.hpp"
#include "Models/Persistence/DatabaseRows.hpp"
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

using namespace PersistenceSerialization;

std::vector<Campaign> DatabaseManager::loadAllCampaigns()
{
    std::vector<Campaign> campaigns;
    try
    {
        SQLite::Statement query_campaigns(*m_db, R"(
            SELECT id, stable_id, name, ruleset_name, description, tags, created_at, updated_at,
                   party_map_id, party_x, party_y, party_vehicles, party_strongholds, party_renown, party_reputation,
                   travel_route_id, travel_route_name, travel_pace, travel_distance_miles, travel_miles_traveled,
                   travel_watches, travel_rations_used, travel_random_encounter_checks, travel_notes,
                   audio_master_volume, audio_music_volume, audio_ambience_volume, audio_effects_volume, audio_voice_volume,
                   presentation_mode_active, calendar_name, calendar_month_names, calendar_month_lengths,
                   calendar_weekday_names, calendar_hours_per_day, calendar_daylight_hours, calendar_night_hours,
                   calendar_day_starts_at_hour, clock_use_system_time, clock_running, clock_multiplier,
                   clock_anchor_minutes, clock_anchor_utc, ruleset_id, ruleset_version,
                   ruleset_overlay_id, ruleset_overlay_version, content_language
            FROM campaigns
            ORDER BY updated_at DESC, name ASC;
        )");

        while (query_campaigns.executeStep())
        {
            Campaign campaign;
            campaign.id = query_campaigns.getColumn(0).getInt64();
            if (!query_campaigns.getColumn(1).isNull())
                campaign.metadata.id = query_campaigns.getColumn(1).getString();
            campaign.name = query_campaigns.getColumn(2).getString();
            std::string rulesetName = query_campaigns.getColumn(3).getString();
            if (!query_campaigns.getColumn(4).isNull())
                campaign.description = query_campaigns.getColumn(4).getString();
            if (!query_campaigns.getColumn(5).isNull())
                campaign.metadata.tags = deserializeStringList(query_campaigns.getColumn(5).getString());
            if (!query_campaigns.getColumn(6).isNull())
                campaign.metadata.createdAtUtc = query_campaigns.getColumn(6).getString();
            if (!query_campaigns.getColumn(7).isNull())
                campaign.metadata.updatedAtUtc = query_campaigns.getColumn(7).getString();
            if (!query_campaigns.getColumn(8).isNull())
                campaign.partyLocation.mapId = query_campaigns.getColumn(8).getString();
            if (!query_campaigns.getColumn(9).isNull())
                campaign.partyLocation.x = query_campaigns.getColumn(9).getInt();
            if (!query_campaigns.getColumn(10).isNull())
                campaign.partyLocation.y = query_campaigns.getColumn(10).getInt();
            if (!query_campaigns.getColumn(11).isNull())
                campaign.partyVehicles = deserializeStringList(query_campaigns.getColumn(11).getString());
            if (!query_campaigns.getColumn(12).isNull())
                campaign.partyStrongholds = deserializeStringList(query_campaigns.getColumn(12).getString());
            if (!query_campaigns.getColumn(13).isNull())
                campaign.partyRenown = query_campaigns.getColumn(13).getInt();
            if (!query_campaigns.getColumn(14).isNull())
                campaign.partyReputation = query_campaigns.getColumn(14).getString();
            if (!query_campaigns.getColumn(15).isNull())
                campaign.partyTravel.routeId = query_campaigns.getColumn(15).getString();
            if (!query_campaigns.getColumn(16).isNull())
                campaign.partyTravel.routeName = query_campaigns.getColumn(16).getString();
            if (!query_campaigns.getColumn(17).isNull())
                campaign.partyTravel.travelPace = query_campaigns.getColumn(17).getString();
            if (!query_campaigns.getColumn(18).isNull())
                campaign.partyTravel.distanceMiles = static_cast<float>(query_campaigns.getColumn(18).getDouble());
            if (!query_campaigns.getColumn(19).isNull())
                campaign.partyTravel.milesTraveled = static_cast<float>(query_campaigns.getColumn(19).getDouble());
            if (!query_campaigns.getColumn(20).isNull())
                campaign.partyTravel.watches = query_campaigns.getColumn(20).getInt();
            if (!query_campaigns.getColumn(21).isNull())
                campaign.partyTravel.rationsUsed = query_campaigns.getColumn(21).getInt();
            if (!query_campaigns.getColumn(22).isNull())
                campaign.partyTravel.randomEncounterChecks = query_campaigns.getColumn(22).getInt();
            if (!query_campaigns.getColumn(23).isNull())
                campaign.partyTravel.notes = query_campaigns.getColumn(23).getString();
            if (!query_campaigns.getColumn(24).isNull())
                campaign.audioVolumeGroups.master = static_cast<float>(query_campaigns.getColumn(24).getDouble());
            if (!query_campaigns.getColumn(25).isNull())
                campaign.audioVolumeGroups.music = static_cast<float>(query_campaigns.getColumn(25).getDouble());
            if (!query_campaigns.getColumn(26).isNull())
                campaign.audioVolumeGroups.ambience = static_cast<float>(query_campaigns.getColumn(26).getDouble());
            if (!query_campaigns.getColumn(27).isNull())
                campaign.audioVolumeGroups.effects = static_cast<float>(query_campaigns.getColumn(27).getDouble());
            if (!query_campaigns.getColumn(28).isNull())
                campaign.audioVolumeGroups.voice = static_cast<float>(query_campaigns.getColumn(28).getDouble());
            if (!query_campaigns.getColumn(29).isNull())
                campaign.presentationModeActive = query_campaigns.getColumn(29).getInt() != 0;
            if (!query_campaigns.getColumn(30).isNull())
                campaign.clock.calendar.name = query_campaigns.getColumn(30).getString();
            if (!query_campaigns.getColumn(31).isNull() || !query_campaigns.getColumn(32).isNull())
            {
                const std::string names =
                    query_campaigns.getColumn(31).isNull() ? std::string{} : query_campaigns.getColumn(31).getString();
                const std::string lengths =
                    query_campaigns.getColumn(32).isNull() ? std::string{} : query_campaigns.getColumn(32).getString();
                campaign.clock.calendar.months = calendarMonthsFromStorage(names, lengths);
            }
            if (!query_campaigns.getColumn(33).isNull())
                campaign.clock.calendar.weekdays = deserializeStringList(query_campaigns.getColumn(33).getString());
            if (!query_campaigns.getColumn(34).isNull())
                campaign.clock.calendar.hoursPerDay = query_campaigns.getColumn(34).getInt();
            if (!query_campaigns.getColumn(35).isNull())
                campaign.clock.calendar.daylightHours = query_campaigns.getColumn(35).getInt();
            if (!query_campaigns.getColumn(36).isNull())
                campaign.clock.calendar.nightHours = query_campaigns.getColumn(36).getInt();
            if (!query_campaigns.getColumn(37).isNull())
                campaign.clock.calendar.dayStartsAtHour = query_campaigns.getColumn(37).getInt();
            if (!query_campaigns.getColumn(38).isNull())
                campaign.clock.useSystemTime = query_campaigns.getColumn(38).getInt() != 0;
            if (!query_campaigns.getColumn(39).isNull())
                campaign.clock.running = query_campaigns.getColumn(39).getInt() != 0;
            if (!query_campaigns.getColumn(40).isNull())
                campaign.clock.multiplier = query_campaigns.getColumn(40).getDouble();
            if (!query_campaigns.getColumn(41).isNull())
                campaign.clock.anchorAbsoluteMinutes = query_campaigns.getColumn(41).getInt64();
            if (!query_campaigns.getColumn(42).isNull())
                campaign.clock.anchorUtc = query_campaigns.getColumn(42).getString();
            campaign.rulesetReference.rulesetId =
                query_campaigns.getColumn(43).isNull() ? std::string{} : query_campaigns.getColumn(43).getString();
            campaign.rulesetReference.version =
                query_campaigns.getColumn(44).isNull() ? std::string{} : query_campaigns.getColumn(44).getString();
            campaign.rulesetReference.overlayId =
                query_campaigns.getColumn(45).isNull() ? std::string{} : query_campaigns.getColumn(45).getString();
            campaign.rulesetReference.overlayVersion =
                query_campaigns.getColumn(46).isNull() ? std::string{} : query_campaigns.getColumn(46).getString();
            if (!query_campaigns.getColumn(47).isNull() && !query_campaigns.getColumn(47).getString().empty())
                campaign.contentLanguage = query_campaigns.getColumn(47).getString() == "de" ? "de" : "en";
            InWorldTime::normalizeClock(campaign.clock);
            campaign.partyTravel.currentMapId = campaign.partyLocation.mapId;
            campaign.ruleSet = createRuleSetFromName(rulesetName);
            if (noRulesetPins(campaign.rulesetReference) && campaign.ruleSet)
                campaign.rulesetReference = {campaign.ruleSet->getId(), campaign.ruleSet->getVersionId(), {}, {}};

            if (campaign.ruleSet || !campaign.rulesetReference.rulesetId.empty())
            {
                loadCampaignPlayerDisplay(campaign);
                loadCampaignRulesetsAndPreferences(campaign);
                loadCampaignCharacters(campaign);
                loadCampaignFactionsAndDeities(campaign);
                loadCampaignNationsAndPlaces(campaign);
                loadCampaignNpcs(campaign);
                loadCampaignItems(campaign);
                loadCampaignShopsAndLoot(campaign);
                loadCampaignSpellsAndMonsters(campaign);
                loadCampaignMaps(campaign);
                loadCampaignDungeons(campaign);
                loadCampaignHandoutsAndDocuments(campaign);
                loadCampaignSessions(campaign);
                loadCampaignAudio(campaign);
                loadCampaignEncounters(campaign);
                loadCampaignStory(campaign);
                loadCampaignTimeline(campaign);
                loadCampaignRandomTables(campaign);
                loadCampaignTabletopToolkit(campaign);
                loadCampaignEntityProperties(campaign);
                campaigns.push_back(std::move(campaign));
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Fehler beim Laden der Kampagnen: " << e.what() << std::endl;
        ProjectLogger::getInstance().error("DatabaseManager", std::string("Could not load campaigns: ") + e.what());
    }
    return campaigns;
}

Campaign &DatabaseManager::loadCampaign(std::string campaignName)
{
    auto campaigns = loadAllCampaigns();
    for (auto &campaign : campaigns)
    {
        if (campaign.name == campaignName)
        {
            currentCampaign = std::move(campaign);
            return currentCampaign;
        }
    }
    throw std::runtime_error("Campaign not found!");
}
