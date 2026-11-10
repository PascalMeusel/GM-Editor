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

void DatabaseManager::saveCampaignFactionsAndDeities(int64_t campaignId, const Campaign &campaign)
{
    deleteRowsMissingStableIds("factions", campaignId, factionIds(campaign.factions));

    SQLite::Statement query_insert_faction(*m_db, R"(
            INSERT INTO factions
            (stable_id, campaign_id, name, faction_type, summary, headquarters, leader_npc_id, attitude_toward_party, goals, resources, gm_notes, influence, threat_level, tags, created_at, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(stable_id) DO UPDATE SET
                campaign_id = excluded.campaign_id,
                name = excluded.name,
                faction_type = excluded.faction_type,
                summary = excluded.summary,
                headquarters = excluded.headquarters,
                leader_npc_id = excluded.leader_npc_id,
                attitude_toward_party = excluded.attitude_toward_party,
                goals = excluded.goals,
                resources = excluded.resources,
                gm_notes = excluded.gm_notes,
                influence = excluded.influence,
                threat_level = excluded.threat_level,
                tags = excluded.tags,
                updated_at = excluded.updated_at;
        )");
    for (const auto &faction : campaign.factions)
    {
        query_insert_faction.bind(1, faction.metadata.id);
        query_insert_faction.bind(2, campaignId);
        query_insert_faction.bind(3, faction.name);
        query_insert_faction.bind(4, faction.type);
        query_insert_faction.bind(5, faction.summary);
        query_insert_faction.bind(6, faction.headquarters);
        query_insert_faction.bind(7, faction.leaderNpcId);
        query_insert_faction.bind(8, faction.attitudeTowardParty);
        query_insert_faction.bind(9, faction.goals);
        query_insert_faction.bind(10, faction.resources);
        query_insert_faction.bind(11, faction.gmNotes);
        query_insert_faction.bind(12, faction.influence);
        query_insert_faction.bind(13, faction.threatLevel);
        query_insert_faction.bind(14, serializeStringList(faction.metadata.tags));
        query_insert_faction.bind(15, faction.metadata.createdAtUtc);
        query_insert_faction.bind(16, currentUtcTimestamp());
        query_insert_faction.exec();
        query_insert_faction.reset();
    }

    deleteRowsMissingStableIds("deities", campaignId, deityIds(campaign.deities));

    SQLite::Statement query_insert_deity(*m_db, R"(
            INSERT INTO deities
            (stable_id, campaign_id, name, power_type, pantheon, domains, symbol, alignment, tenets, worshipers, relationships, gm_notes, influence, threat_level, tags, created_at, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(stable_id) DO UPDATE SET
                campaign_id = excluded.campaign_id,
                name = excluded.name,
                power_type = excluded.power_type,
                pantheon = excluded.pantheon,
                domains = excluded.domains,
                symbol = excluded.symbol,
                alignment = excluded.alignment,
                tenets = excluded.tenets,
                worshipers = excluded.worshipers,
                relationships = excluded.relationships,
                gm_notes = excluded.gm_notes,
                influence = excluded.influence,
                threat_level = excluded.threat_level,
                tags = excluded.tags,
                updated_at = excluded.updated_at;
        )");
    for (const auto &deity : campaign.deities)
    {
        query_insert_deity.bind(1, deity.metadata.id);
        query_insert_deity.bind(2, campaignId);
        query_insert_deity.bind(3, deity.name);
        query_insert_deity.bind(4, deity.powerType);
        query_insert_deity.bind(5, deity.pantheon);
        query_insert_deity.bind(6, deity.domains);
        query_insert_deity.bind(7, deity.symbol);
        query_insert_deity.bind(8, deity.alignment);
        query_insert_deity.bind(9, deity.tenets);
        query_insert_deity.bind(10, deity.worshipers);
        query_insert_deity.bind(11, deity.relationships);
        query_insert_deity.bind(12, deity.gmNotes);
        query_insert_deity.bind(13, deity.influence);
        query_insert_deity.bind(14, deity.threatLevel);
        query_insert_deity.bind(15, serializeStringList(deity.metadata.tags));
        query_insert_deity.bind(16, deity.metadata.createdAtUtc);
        query_insert_deity.bind(17, currentUtcTimestamp());
        query_insert_deity.exec();
        query_insert_deity.reset();
    }
}

void DatabaseManager::saveCampaignNationsAndPlaces(int64_t campaignId, const Campaign &campaign)
{
    SQLite::Statement query_delete_nation_territories(*m_db, R"(
            DELETE FROM nation_territories
            WHERE nation_id IN (SELECT id FROM nations WHERE campaign_id = ?);
        )");
    query_delete_nation_territories.bind(1, campaignId);
    query_delete_nation_territories.exec();
    deleteRowsMissingStableIds("nations", campaignId, nationIds(campaign.nations));

    SQLite::Statement query_insert_nation(*m_db, R"(
            INSERT INTO nations
            (stable_id, campaign_id, name, description, government, culture, capital_place_id, relationships, faction_ids, visibility, tags, created_at, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(stable_id) DO UPDATE SET
                campaign_id = excluded.campaign_id,
                name = excluded.name,
                description = excluded.description,
                government = excluded.government,
                culture = excluded.culture,
                capital_place_id = excluded.capital_place_id,
                relationships = excluded.relationships,
                faction_ids = excluded.faction_ids,
                visibility = excluded.visibility,
                tags = excluded.tags,
                updated_at = excluded.updated_at;
        )");
    SQLite::Statement query_nation_id(*m_db, "SELECT id FROM nations WHERE stable_id = ? LIMIT 1;");
    SQLite::Statement query_insert_territory(*m_db, R"(
            INSERT INTO nation_territories
            (stable_id, nation_id, name, polygon, tags, created_at, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?);
        )");
    for (const NationModel &nation : campaign.nations)
    {
        query_insert_nation.bind(1, nation.metadata.id);
        query_insert_nation.bind(2, campaignId);
        query_insert_nation.bind(3, nation.name.empty() ? "Unnamed Nation" : nation.name);
        query_insert_nation.bind(4, nation.description);
        query_insert_nation.bind(5, nation.government);
        query_insert_nation.bind(6, nation.culture);
        query_insert_nation.bind(7, nation.capitalPlaceId);
        query_insert_nation.bind(8, nation.relationships);
        query_insert_nation.bind(9, serializeStringList(nation.factionIds));
        query_insert_nation.bind(10, static_cast<int>(nation.visibility));
        query_insert_nation.bind(11, serializeStringList(nation.metadata.tags));
        query_insert_nation.bind(12, nation.metadata.createdAtUtc);
        query_insert_nation.bind(13, currentUtcTimestamp());
        query_insert_nation.exec();
        query_insert_nation.reset();

        query_nation_id.bind(1, nation.metadata.id);
        if (query_nation_id.executeStep())
        {
            const int64_t nationRowId = query_nation_id.getColumn(0).getInt64();
            for (const NationTerritoryModel &territory : nation.territories)
            {
                query_insert_territory.bind(1, territory.metadata.id);
                query_insert_territory.bind(2, nationRowId);
                query_insert_territory.bind(3, territory.name);
                query_insert_territory.bind(4, serializeNormalizedPolygon(territory.polygon));
                query_insert_territory.bind(5, serializeStringList(territory.metadata.tags));
                query_insert_territory.bind(6, territory.metadata.createdAtUtc);
                query_insert_territory.bind(7, currentUtcTimestamp());
                query_insert_territory.exec();
                query_insert_territory.reset();
            }
        }
        query_nation_id.reset();
    }

    deleteRowsMissingStableIds("world_places", campaignId, worldPlaceIds(campaign.worldPlaces));
    SQLite::Statement query_insert_world_place(*m_db, R"(
            INSERT INTO world_places
            (stable_id, campaign_id, place_type, name, description, notes, nation_id, linked_map_id, linked_feature_id, has_map_position, map_position_x, map_position_y, visibility, tags, created_at, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(stable_id) DO UPDATE SET
                campaign_id = excluded.campaign_id,
                place_type = excluded.place_type,
                name = excluded.name,
                description = excluded.description,
                notes = excluded.notes,
                nation_id = excluded.nation_id,
                linked_map_id = excluded.linked_map_id,
                linked_feature_id = excluded.linked_feature_id,
                has_map_position = excluded.has_map_position,
                map_position_x = excluded.map_position_x,
                map_position_y = excluded.map_position_y,
                visibility = excluded.visibility,
                tags = excluded.tags,
                updated_at = excluded.updated_at;
        )");
    for (const WorldPlaceModel &place : campaign.worldPlaces)
    {
        query_insert_world_place.bind(1, place.metadata.id);
        query_insert_world_place.bind(2, campaignId);
        query_insert_world_place.bind(3, static_cast<int>(place.type));
        query_insert_world_place.bind(4, place.name.empty() ? "Unnamed Place" : place.name);
        query_insert_world_place.bind(5, place.description);
        query_insert_world_place.bind(6, place.notes);
        query_insert_world_place.bind(7, place.nationId);
        query_insert_world_place.bind(8, place.linkedMapId);
        query_insert_world_place.bind(9, place.linkedFeatureId);
        query_insert_world_place.bind(10, place.hasMapPosition ? 1 : 0);
        query_insert_world_place.bind(11, place.mapPosition.x);
        query_insert_world_place.bind(12, place.mapPosition.y);
        query_insert_world_place.bind(13, static_cast<int>(place.visibility));
        query_insert_world_place.bind(14, serializeStringList(place.metadata.tags));
        query_insert_world_place.bind(15, place.metadata.createdAtUtc);
        query_insert_world_place.bind(16, currentUtcTimestamp());
        query_insert_world_place.exec();
        query_insert_world_place.reset();
    }
}
