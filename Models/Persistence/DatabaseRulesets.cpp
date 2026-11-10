#include "Models/DataBaseManager.hpp"
#include <exception>
#include <memory>
#include <string>
#include "Models/Session/SessionPlanService.hpp"
#include "Models/Persistence/PersistenceSerialization.hpp"
#include "Models/Map/MapAtlasService.hpp"
#include "Models/Project/ApplicationPaths.hpp"
#include "Models/Project/ProjectLogger.hpp"
#include "Models/Rulesets/RulesetDefinitionSerialization.hpp"
#include "Models/Rulesets/DefinitionBackedRuleSet.hpp"
#include "Models/Rulesets/RulesetResolution.hpp"
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

void DatabaseManager::saveCampaignRulesets(int64_t campaignId, const Campaign &campaign)
{
    deleteRowsMissingStableIds("ruleset_definitions", campaignId, rulesetDefinitionIds(campaign.rulesetDefinitions));
    SQLite::Statement query_insert_ruleset_definition(*m_db, R"(
            INSERT INTO ruleset_definitions
            (stable_id, campaign_id, name, version, base_ruleset_id, definition_json, installed, tags, created_at, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(stable_id) DO UPDATE SET
                campaign_id = excluded.campaign_id,
                name = excluded.name,
                version = excluded.version,
                base_ruleset_id = excluded.base_ruleset_id,
                definition_json = excluded.definition_json,
                installed = excluded.installed,
                tags = excluded.tags,
                updated_at = excluded.updated_at;
        )");
    for (const RulesetDefinitionModel &definition : campaign.rulesetDefinitions)
    {
        bindRulesetDefinition(query_insert_ruleset_definition, campaignId, definition, false);
        query_insert_ruleset_definition.exec();
        query_insert_ruleset_definition.reset();
    }

    deleteContentPacksMissingStableIds(*m_db, campaignId, rulesetContentPackIds(campaign.rulesetContentPacks));
    SQLite::Statement query_insert_content_pack(*m_db, R"(
            INSERT INTO homebrew_definitions
            (stable_id, campaign_id, ruleset_id, content_type, name, version, author, license, source, imported_at_utc, enabled, definition_json, tags, created_at, updated_at)
            VALUES (?, ?, ?, 'content_pack', ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(stable_id) DO UPDATE SET
                campaign_id = excluded.campaign_id,
                ruleset_id = excluded.ruleset_id,
                content_type = excluded.content_type,
                name = excluded.name,
                version = excluded.version,
                author = excluded.author,
                license = excluded.license,
                source = excluded.source,
                imported_at_utc = excluded.imported_at_utc,
                enabled = excluded.enabled,
                definition_json = excluded.definition_json,
                tags = excluded.tags,
                updated_at = excluded.updated_at;
        )");
    for (const RulesetContentPackModel &pack : campaign.rulesetContentPacks)
    {
        query_insert_content_pack.bind(1, pack.metadata.id);
        query_insert_content_pack.bind(2, campaignId);
        query_insert_content_pack.bind(3, pack.rulesetId);
        query_insert_content_pack.bind(4, pack.name);
        query_insert_content_pack.bind(5, pack.version);
        query_insert_content_pack.bind(6, pack.author);
        query_insert_content_pack.bind(7, pack.license);
        query_insert_content_pack.bind(8, pack.source);
        query_insert_content_pack.bind(9, pack.importedAtUtc.empty() ? currentUtcTimestamp() : pack.importedAtUtc);
        query_insert_content_pack.bind(10, pack.enabled ? 1 : 0);
        query_insert_content_pack.bind(11, pack.definitionJson);
        query_insert_content_pack.bind(12, serializeStringList(pack.metadata.tags));
        query_insert_content_pack.bind(13, pack.metadata.createdAtUtc);
        query_insert_content_pack.bind(14, currentUtcTimestamp());
        query_insert_content_pack.exec();
        query_insert_content_pack.reset();
    }
}

void DatabaseManager::saveCampaignCampaignPreferences(int64_t campaignId, const Campaign &campaign)
{
    deleteRowsMissingStableIds("house_rules", campaignId, houseRuleIds(campaign.houseRules));
    SQLite::Statement query_insert_house_rule(*m_db, R"(
            INSERT INTO house_rules
            (stable_id, campaign_id, label, description, enabled, change_history, tags, created_at, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(stable_id) DO UPDATE SET
                campaign_id = excluded.campaign_id,
                label = excluded.label,
                description = excluded.description,
                enabled = excluded.enabled,
                change_history = excluded.change_history,
                tags = excluded.tags,
                updated_at = excluded.updated_at;
        )");
    for (const HouseRuleModel &rule : campaign.houseRules)
    {
        query_insert_house_rule.bind(1, rule.metadata.id);
        query_insert_house_rule.bind(2, campaignId);
        query_insert_house_rule.bind(3, rule.label);
        query_insert_house_rule.bind(4, rule.description);
        query_insert_house_rule.bind(5, rule.enabled ? 1 : 0);
        query_insert_house_rule.bind(6, serializeHouseRuleChanges(rule.changeHistory));
        query_insert_house_rule.bind(7, serializeStringList(rule.metadata.tags));
        query_insert_house_rule.bind(8, rule.metadata.createdAtUtc);
        query_insert_house_rule.bind(9, currentUtcTimestamp());
        query_insert_house_rule.exec();
        query_insert_house_rule.reset();
    }

    deleteRowsMissingStableIds("saved_filters", campaignId, savedFilterIds(campaign.savedFilters));
    SQLite::Statement query_insert_saved_filter(*m_db, R"(
            INSERT INTO saved_filters
            (stable_id, campaign_id, name, query, entity_types, required_tags, match_all_tags, smart_collection, tags, created_at, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(stable_id) DO UPDATE SET
                campaign_id = excluded.campaign_id,
                name = excluded.name,
                query = excluded.query,
                entity_types = excluded.entity_types,
                required_tags = excluded.required_tags,
                match_all_tags = excluded.match_all_tags,
                smart_collection = excluded.smart_collection,
                tags = excluded.tags,
                updated_at = excluded.updated_at;
        )");
    for (const CampaignSavedFilterModel &filter : campaign.savedFilters)
    {
        query_insert_saved_filter.bind(1, filter.metadata.id);
        query_insert_saved_filter.bind(2, campaignId);
        query_insert_saved_filter.bind(3, filter.name);
        query_insert_saved_filter.bind(4, filter.query);
        query_insert_saved_filter.bind(5, serializeStringList(filter.entityTypes));
        query_insert_saved_filter.bind(6, serializeStringList(filter.requiredTags));
        query_insert_saved_filter.bind(7, filter.matchAllTags ? 1 : 0);
        query_insert_saved_filter.bind(8, filter.smartCollection ? 1 : 0);
        query_insert_saved_filter.bind(9, serializeStringList(filter.metadata.tags));
        query_insert_saved_filter.bind(10, filter.metadata.createdAtUtc);
        query_insert_saved_filter.bind(11, currentUtcTimestamp());
        query_insert_saved_filter.exec();
        query_insert_saved_filter.reset();
    }

    deleteRowsMissingStableIds("entity_bookmarks", campaignId, entityBookmarkIds(campaign.entityBookmarks));
    SQLite::Statement query_insert_entity_bookmark(*m_db, R"(
            INSERT INTO entity_bookmarks
            (stable_id, campaign_id, entity_id, entity_type, label, favorite, last_opened_at_utc, open_count, tags, created_at, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(stable_id) DO UPDATE SET
                campaign_id = excluded.campaign_id,
                entity_id = excluded.entity_id,
                entity_type = excluded.entity_type,
                label = excluded.label,
                favorite = excluded.favorite,
                last_opened_at_utc = excluded.last_opened_at_utc,
                open_count = excluded.open_count,
                tags = excluded.tags,
                updated_at = excluded.updated_at;
        )");
    for (const CampaignEntityBookmarkModel &bookmark : campaign.entityBookmarks)
    {
        query_insert_entity_bookmark.bind(1, bookmark.metadata.id);
        query_insert_entity_bookmark.bind(2, campaignId);
        query_insert_entity_bookmark.bind(3, bookmark.entityId);
        query_insert_entity_bookmark.bind(4, bookmark.entityType);
        query_insert_entity_bookmark.bind(5, bookmark.label);
        query_insert_entity_bookmark.bind(6, bookmark.favorite ? 1 : 0);
        query_insert_entity_bookmark.bind(7, bookmark.lastOpenedAtUtc);
        query_insert_entity_bookmark.bind(8, bookmark.openCount);
        query_insert_entity_bookmark.bind(9, serializeStringList(bookmark.metadata.tags));
        query_insert_entity_bookmark.bind(10, bookmark.metadata.createdAtUtc);
        query_insert_entity_bookmark.bind(11, currentUtcTimestamp());
        query_insert_entity_bookmark.exec();
        query_insert_entity_bookmark.reset();
    }
}

void DatabaseManager::loadCampaignRulesetsAndPreferences(Campaign &campaign)
{
    SQLite::Statement query_ruleset_definitions(*m_db, R"(
                    SELECT stable_id, name, version, base_ruleset_id, definition_json, installed, tags, created_at, updated_at
                    FROM ruleset_definitions
                    WHERE campaign_id = ?
                    ORDER BY name ASC, version ASC;
                )");
    query_ruleset_definitions.bind(1, static_cast<int64_t>(campaign.id));

    while (query_ruleset_definitions.executeStep())
        campaign.rulesetDefinitions.push_back(readRulesetDefinition(query_ruleset_definitions));

    // Resolve a campaign's immutable base version and optional overlay.
    // Unavailable requested pins remain explicit; their references are never rewritten.
    std::vector<RulesetDefinitionModel> availableDefinitions = campaign.rulesetDefinitions;
    SQLite::Statement query_installed_definitions(*m_db, R"(
                    SELECT stable_id, name, version, base_ruleset_id, definition_json, installed, tags, created_at, updated_at
                    FROM ruleset_definitions
                    WHERE campaign_id IS NULL AND installed = 1;
                )");
    while (query_installed_definitions.executeStep())
        availableDefinitions.push_back(readRulesetDefinition(query_installed_definitions));

    resolveCampaignRuleset(campaign, availableDefinitions);

    SQLite::Statement query_content_packs(*m_db, R"(
                    SELECT stable_id, ruleset_id, name, version, author, license, source, imported_at_utc, enabled, definition_json, tags, created_at, updated_at
                    FROM homebrew_definitions
                    WHERE campaign_id = ? AND content_type = 'content_pack'
                    ORDER BY name ASC, version ASC;
                )");
    query_content_packs.bind(1, static_cast<int64_t>(campaign.id));

    while (query_content_packs.executeStep())
        campaign.rulesetContentPacks.push_back(readRulesetContentPack(query_content_packs));

    SQLite::Statement query_house_rules(*m_db, R"(
                    SELECT stable_id, label, description, enabled, change_history, tags, created_at, updated_at
                    FROM house_rules
                    WHERE campaign_id = ?
                    ORDER BY label ASC;
                )");
    query_house_rules.bind(1, static_cast<int64_t>(campaign.id));

    while (query_house_rules.executeStep())
        campaign.houseRules.push_back(readHouseRule(query_house_rules));

    SQLite::Statement query_saved_filters(*m_db, R"(
                    SELECT stable_id, name, query, entity_types, required_tags, match_all_tags, smart_collection, tags, created_at, updated_at
                    FROM saved_filters
                    WHERE campaign_id = ?
                    ORDER BY name ASC;
                )");
    query_saved_filters.bind(1, static_cast<int64_t>(campaign.id));

    while (query_saved_filters.executeStep())
        campaign.savedFilters.push_back(readSavedFilter(query_saved_filters));

    SQLite::Statement query_entity_bookmarks(*m_db, R"(
                    SELECT stable_id, entity_id, entity_type, label, favorite, last_opened_at_utc, open_count, tags, created_at, updated_at
                    FROM entity_bookmarks
                    WHERE campaign_id = ?
                    ORDER BY favorite DESC, last_opened_at_utc DESC, label ASC;
                )");
    query_entity_bookmarks.bind(1, static_cast<int64_t>(campaign.id));

    while (query_entity_bookmarks.executeStep())
        campaign.entityBookmarks.push_back(readEntityBookmark(query_entity_bookmarks));

    campaign.partyInventory.ownerEntityId = campaign.metadata.id;
    campaign.partyInventory.name = "Party Inventory";
    campaign.partyInventory =
        loadInventoryForOwner(campaign.id, "party", campaign.metadata.id, campaign.partyInventory);

    // Now load the characters for this campaign
}

void DatabaseManager::saveInstalledRulesetDefinition(const RulesetDefinitionModel &definition)
{
    try
    {
        if (definition.published)
        {
            const std::string immutableContent = RulesetDefinitionSerialization::serializeDefinitionContent(definition);
            SQLite::Statement existing_version(
                *m_db, "SELECT definition_json FROM ruleset_definitions WHERE stable_id = ? LIMIT 1;");
            existing_version.bind(1, definition.metadata.id);
            if (existing_version.executeStep())
            {
                const std::string storedContent =
                    existing_version.getColumn(0).isNull() ? std::string{} : existing_version.getColumn(0).getString();
                if (storedContent != immutableContent)
                    throw std::runtime_error(
                        "Published ruleset versions are immutable. Publish the changes under a new version.");
                return;
            }
        }

        SQLite::Statement query_insert_ruleset_definition(*m_db, R"(
            INSERT INTO ruleset_definitions
            (stable_id, campaign_id, name, version, base_ruleset_id, definition_json, installed, tags, created_at, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(stable_id) DO UPDATE SET
                campaign_id = excluded.campaign_id,
                name = excluded.name,
                version = excluded.version,
                base_ruleset_id = excluded.base_ruleset_id,
                definition_json = excluded.definition_json,
                installed = excluded.installed,
                tags = excluded.tags,
                updated_at = excluded.updated_at;
        )");
        bindRulesetDefinition(query_insert_ruleset_definition, -1, definition, true);
        query_insert_ruleset_definition.exec();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Fehler beim Speichern der installierten Ruleset-Definition '" << definition.name
                  << "': " << e.what() << std::endl;
        ProjectLogger::getInstance().error("DatabaseManager",
                                           std::string("Could not save installed ruleset definition '") +
                                               definition.name + "': " + e.what());
        throw;
    }
}

void DatabaseManager::removeInstalledRulesetDefinition(const std::string &stableId)
{
    if (stableId.empty())
        return;
    SQLite::Statement remove_definition(*m_db,
                                        "DELETE FROM ruleset_definitions WHERE stable_id = ? AND campaign_id IS NULL;");
    remove_definition.bind(1, stableId);
    remove_definition.exec();
}

std::vector<RulesetDefinitionModel> DatabaseManager::loadInstalledRulesetDefinitions()
{
    std::vector<RulesetDefinitionModel> definitions;
    try
    {
        SQLite::Statement query_definitions(*m_db, R"(
            SELECT stable_id, name, version, base_ruleset_id, definition_json, installed, tags, created_at, updated_at
            FROM ruleset_definitions
            WHERE installed = 1 AND campaign_id IS NULL
            ORDER BY name ASC, version ASC;
        )");

        while (query_definitions.executeStep())
            definitions.push_back(readRulesetDefinition(query_definitions));
    }
    catch (const std::exception &e)
    {
        std::cerr << "Fehler beim Laden installierter Ruleset-Definitionen: " << e.what() << std::endl;
        ProjectLogger::getInstance().error("DatabaseManager",
                                           std::string("Could not load installed ruleset definitions: ") + e.what());
    }
    return definitions;
}
