#include "Models/DataBaseManager.hpp"
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

void DatabaseManager::loadCampaignFactionsAndDeities(Campaign &campaign)
{
    SQLite::Statement query_factions(*m_db, R"(
                    SELECT stable_id, name, faction_type, summary, headquarters, leader_npc_id, attitude_toward_party, goals, resources, gm_notes, influence, threat_level, tags, created_at, updated_at
                    FROM factions
                    WHERE campaign_id = ?
                    ORDER BY name ASC;
                )");
    query_factions.bind(1, static_cast<int64_t>(campaign.id));

    while (query_factions.executeStep())
    {
        FactionModel faction;
        if (!query_factions.getColumn(0).isNull())
            faction.metadata.id = query_factions.getColumn(0).getString();
        faction.name = query_factions.getColumn(1).getString();
        if (!query_factions.getColumn(2).isNull())
            faction.type = query_factions.getColumn(2).getString();
        if (!query_factions.getColumn(3).isNull())
            faction.summary = query_factions.getColumn(3).getString();
        if (!query_factions.getColumn(4).isNull())
            faction.headquarters = query_factions.getColumn(4).getString();
        if (!query_factions.getColumn(5).isNull())
            faction.leaderNpcId = query_factions.getColumn(5).getString();
        if (!query_factions.getColumn(6).isNull())
            faction.attitudeTowardParty = query_factions.getColumn(6).getString();
        if (!query_factions.getColumn(7).isNull())
            faction.goals = query_factions.getColumn(7).getString();
        if (!query_factions.getColumn(8).isNull())
            faction.resources = query_factions.getColumn(8).getString();
        if (!query_factions.getColumn(9).isNull())
            faction.gmNotes = query_factions.getColumn(9).getString();
        if (!query_factions.getColumn(10).isNull())
            faction.influence = query_factions.getColumn(10).getInt();
        if (!query_factions.getColumn(11).isNull())
            faction.threatLevel = query_factions.getColumn(11).getInt();
        if (!query_factions.getColumn(12).isNull())
            faction.metadata.tags = deserializeStringList(query_factions.getColumn(12).getString());
        if (!query_factions.getColumn(13).isNull())
            faction.metadata.createdAtUtc = query_factions.getColumn(13).getString();
        if (!query_factions.getColumn(14).isNull())
            faction.metadata.updatedAtUtc = query_factions.getColumn(14).getString();
        campaign.factions.push_back(faction);
    }

    SQLite::Statement query_deities(*m_db, R"(
                    SELECT stable_id, name, power_type, pantheon, domains, symbol, alignment, tenets, worshipers, relationships, gm_notes, influence, threat_level, tags, created_at, updated_at
                    FROM deities
                    WHERE campaign_id = ?
                    ORDER BY name ASC;
                )");
    query_deities.bind(1, static_cast<int64_t>(campaign.id));

    while (query_deities.executeStep())
    {
        DeityModel deity;
        if (!query_deities.getColumn(0).isNull())
            deity.metadata.id = query_deities.getColumn(0).getString();
        deity.name = query_deities.getColumn(1).getString();
        if (!query_deities.getColumn(2).isNull())
            deity.powerType = query_deities.getColumn(2).getString();
        if (!query_deities.getColumn(3).isNull())
            deity.pantheon = query_deities.getColumn(3).getString();
        if (!query_deities.getColumn(4).isNull())
            deity.domains = query_deities.getColumn(4).getString();
        if (!query_deities.getColumn(5).isNull())
            deity.symbol = query_deities.getColumn(5).getString();
        if (!query_deities.getColumn(6).isNull())
            deity.alignment = query_deities.getColumn(6).getString();
        if (!query_deities.getColumn(7).isNull())
            deity.tenets = query_deities.getColumn(7).getString();
        if (!query_deities.getColumn(8).isNull())
            deity.worshipers = query_deities.getColumn(8).getString();
        if (!query_deities.getColumn(9).isNull())
            deity.relationships = query_deities.getColumn(9).getString();
        if (!query_deities.getColumn(10).isNull())
            deity.gmNotes = query_deities.getColumn(10).getString();
        if (!query_deities.getColumn(11).isNull())
            deity.influence = query_deities.getColumn(11).getInt();
        if (!query_deities.getColumn(12).isNull())
            deity.threatLevel = query_deities.getColumn(12).getInt();
        if (!query_deities.getColumn(13).isNull())
            deity.metadata.tags = deserializeStringList(query_deities.getColumn(13).getString());
        if (!query_deities.getColumn(14).isNull())
            deity.metadata.createdAtUtc = query_deities.getColumn(14).getString();
        if (!query_deities.getColumn(15).isNull())
            deity.metadata.updatedAtUtc = query_deities.getColumn(15).getString();
        campaign.deities.push_back(deity);
    }
}

void DatabaseManager::loadCampaignNationsAndPlaces(Campaign &campaign)
{
    SQLite::Statement query_nations(*m_db, R"(
                    SELECT id, stable_id, name, description, government, culture, capital_place_id, relationships, faction_ids, visibility, tags, created_at, updated_at
                    FROM nations
                    WHERE campaign_id = ?
                    ORDER BY name ASC;
                )");
    query_nations.bind(1, static_cast<int64_t>(campaign.id));
    while (query_nations.executeStep())
    {
        NationModel nation;
        const int64_t nationRowId = query_nations.getColumn(0).getInt64();
        if (!query_nations.getColumn(1).isNull())
            nation.metadata.id = query_nations.getColumn(1).getString();
        nation.name = query_nations.getColumn(2).getString();
        if (!query_nations.getColumn(3).isNull())
            nation.description = query_nations.getColumn(3).getString();
        if (!query_nations.getColumn(4).isNull())
            nation.government = query_nations.getColumn(4).getString();
        if (!query_nations.getColumn(5).isNull())
            nation.culture = query_nations.getColumn(5).getString();
        if (!query_nations.getColumn(6).isNull())
            nation.capitalPlaceId = query_nations.getColumn(6).getString();
        if (!query_nations.getColumn(7).isNull())
            nation.relationships = query_nations.getColumn(7).getString();
        if (!query_nations.getColumn(8).isNull())
            nation.factionIds = deserializeStringList(query_nations.getColumn(8).getString());
        if (!query_nations.getColumn(9).isNull())
            nation.visibility = static_cast<WorldObjectVisibility>(
                std::clamp(query_nations.getColumn(9).getInt(), 0, static_cast<int>(WorldObjectVisibility::Public)));
        if (!query_nations.getColumn(10).isNull())
            nation.metadata.tags = deserializeStringList(query_nations.getColumn(10).getString());
        if (!query_nations.getColumn(11).isNull())
            nation.metadata.createdAtUtc = query_nations.getColumn(11).getString();
        if (!query_nations.getColumn(12).isNull())
            nation.metadata.updatedAtUtc = query_nations.getColumn(12).getString();

        SQLite::Statement query_territories(*m_db, R"(
                        SELECT stable_id, name, polygon, tags, created_at, updated_at
                        FROM nation_territories
                        WHERE nation_id = ?
                        ORDER BY id ASC;
                    )");
        query_territories.bind(1, nationRowId);
        while (query_territories.executeStep())
        {
            NationTerritoryModel territory;
            if (!query_territories.getColumn(0).isNull())
                territory.metadata.id = query_territories.getColumn(0).getString();
            if (!query_territories.getColumn(1).isNull())
                territory.name = query_territories.getColumn(1).getString();
            if (!query_territories.getColumn(2).isNull())
                territory.polygon = deserializeNormalizedPolygon(query_territories.getColumn(2).getString());
            if (!query_territories.getColumn(3).isNull())
                territory.metadata.tags = deserializeStringList(query_territories.getColumn(3).getString());
            if (!query_territories.getColumn(4).isNull())
                territory.metadata.createdAtUtc = query_territories.getColumn(4).getString();
            if (!query_territories.getColumn(5).isNull())
                territory.metadata.updatedAtUtc = query_territories.getColumn(5).getString();
            normalizeNationTerritory(territory);
            nation.territories.push_back(std::move(territory));
        }
        campaign.nations.push_back(std::move(nation));
    }

    SQLite::Statement query_world_places(*m_db, R"(
                    SELECT stable_id, place_type, name, description, notes, nation_id, linked_map_id, linked_feature_id, has_map_position, map_position_x, map_position_y, visibility, tags, created_at, updated_at
                    FROM world_places
                    WHERE campaign_id = ?
                    ORDER BY name ASC;
                )");
    query_world_places.bind(1, static_cast<int64_t>(campaign.id));
    while (query_world_places.executeStep())
    {
        WorldPlaceModel place;
        if (!query_world_places.getColumn(0).isNull())
            place.metadata.id = query_world_places.getColumn(0).getString();
        if (!query_world_places.getColumn(1).isNull())
            place.type = static_cast<WorldPlaceType>(
                std::clamp(query_world_places.getColumn(1).getInt(), 0, static_cast<int>(WorldPlaceType::Site)));
        place.name = query_world_places.getColumn(2).getString();
        if (!query_world_places.getColumn(3).isNull())
            place.description = query_world_places.getColumn(3).getString();
        if (!query_world_places.getColumn(4).isNull())
            place.notes = query_world_places.getColumn(4).getString();
        if (!query_world_places.getColumn(5).isNull())
            place.nationId = query_world_places.getColumn(5).getString();
        if (!query_world_places.getColumn(6).isNull())
            place.linkedMapId = query_world_places.getColumn(6).getString();
        if (!query_world_places.getColumn(7).isNull())
            place.linkedFeatureId = query_world_places.getColumn(7).getString();
        if (!query_world_places.getColumn(8).isNull())
            place.hasMapPosition = query_world_places.getColumn(8).getInt() != 0;
        if (!query_world_places.getColumn(9).isNull())
            place.mapPosition.x = query_world_places.getColumn(9).getDouble();
        if (!query_world_places.getColumn(10).isNull())
            place.mapPosition.y = query_world_places.getColumn(10).getDouble();
        if (!query_world_places.getColumn(11).isNull())
            place.visibility = static_cast<WorldObjectVisibility>(std::clamp(
                query_world_places.getColumn(11).getInt(), 0, static_cast<int>(WorldObjectVisibility::Public)));
        if (!query_world_places.getColumn(12).isNull())
            place.metadata.tags = deserializeStringList(query_world_places.getColumn(12).getString());
        if (!query_world_places.getColumn(13).isNull())
            place.metadata.createdAtUtc = query_world_places.getColumn(13).getString();
        if (!query_world_places.getColumn(14).isNull())
            place.metadata.updatedAtUtc = query_world_places.getColumn(14).getString();
        normalizeWorldPlacePosition(place);
        campaign.worldPlaces.push_back(std::move(place));
    }
}
