#include "Models/DataBaseManager.hpp"
#include <cstddef>
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

std::string serializeHouseRuleChanges(const std::vector<HouseRuleChangeModel> &changes)
{
    constexpr char fieldSeparator = '\x1f';
    constexpr char entrySeparator = '\x1e';

    std::stringstream stream;
    for (const HouseRuleChangeModel &change : changes)
        stream << change.changedAtUtc << fieldSeparator << change.summary << entrySeparator;
    return stream.str();
}

std::vector<HouseRuleChangeModel> deserializeHouseRuleChanges(const std::string &serializedChanges)
{
    constexpr char fieldSeparator = '\x1f';
    constexpr char entrySeparator = '\x1e';

    std::vector<HouseRuleChangeModel> changes;
    std::stringstream entryStream(serializedChanges);
    std::string entryValue;
    while (std::getline(entryStream, entryValue, entrySeparator))
    {
        if (entryValue.empty())
            continue;

        std::stringstream fieldStream(entryValue);
        HouseRuleChangeModel change;
        std::getline(fieldStream, change.changedAtUtc, fieldSeparator);
        std::getline(fieldStream, change.summary, fieldSeparator);
        if (!change.summary.empty())
            changes.push_back(change);
    }
    return changes;
}

void deleteContentPacksMissingStableIds(SQLite::Database &database, int64_t campaignId,
                                        const std::vector<EntityId> &stableIds)
{
    if (stableIds.empty())
    {
        SQLite::Statement deleteAll(
            database, "DELETE FROM homebrew_definitions WHERE campaign_id = ? AND content_type = 'content_pack';");
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

    SQLite::Statement deleteMissing(
        database,
        "DELETE FROM homebrew_definitions WHERE campaign_id = ? AND content_type = 'content_pack' AND (stable_id IS NULL OR stable_id NOT IN (" +
            placeholders + "));");
    deleteMissing.bind(1, campaignId);
    for (std::size_t index = 0; index < stableIds.size(); ++index)
        deleteMissing.bind(static_cast<int>(index + 2), stableIds[index]);
    deleteMissing.exec();
}

RulesetDefinitionModel readRulesetDefinition(SQLite::Statement &query)
{
    RulesetDefinitionModel definition;
    if (!query.getColumn(0).isNull())
        definition.metadata.id = query.getColumn(0).getString();
    definition.name = query.getColumn(1).getString();
    if (!query.getColumn(2).isNull())
        definition.version = query.getColumn(2).getString();
    if (!query.getColumn(3).isNull())
        definition.baseRulesetId = query.getColumn(3).getString();
    if (!query.getColumn(4).isNull())
        definition.definitionJson = query.getColumn(4).getString();
    if (!query.getColumn(5).isNull())
        definition.installedGlobally = query.getColumn(5).getInt() != 0;
    if (!query.getColumn(6).isNull())
        definition.metadata.tags = deserializeStringList(query.getColumn(6).getString());
    if (!query.getColumn(7).isNull())
        definition.metadata.createdAtUtc = query.getColumn(7).getString();
    if (!query.getColumn(8).isNull())
        definition.metadata.updatedAtUtc = query.getColumn(8).getString();
    RulesetDefinitionSerialization::hydrateDefinitionContent(definition);
    return definition;
}

void bindRulesetDefinition(SQLite::Statement &statement, int64_t campaignId, const RulesetDefinitionModel &definition,
                           bool installedGlobally)
{
    statement.bind(1, definition.metadata.id);
    if (campaignId >= 0)
        statement.bind(2, campaignId);
    else
        statement.bind(2);
    statement.bind(3, definition.name);
    statement.bind(4, definition.version);
    statement.bind(5, definition.baseRulesetId);
    statement.bind(6, RulesetDefinitionSerialization::serializeDefinitionContent(definition));
    statement.bind(7, installedGlobally ? 1 : 0);
    statement.bind(8, serializeStringList(definition.metadata.tags));
    statement.bind(9, definition.metadata.createdAtUtc);
    statement.bind(10, currentUtcTimestamp());
}

RulesetContentPackModel readRulesetContentPack(SQLite::Statement &query)
{
    RulesetContentPackModel pack;
    if (!query.getColumn(0).isNull())
        pack.metadata.id = query.getColumn(0).getString();
    if (!query.getColumn(1).isNull())
        pack.rulesetId = query.getColumn(1).getString();
    pack.name = query.getColumn(2).getString();
    if (!query.getColumn(3).isNull())
        pack.version = query.getColumn(3).getString();
    if (!query.getColumn(4).isNull())
        pack.author = query.getColumn(4).getString();
    if (!query.getColumn(5).isNull())
        pack.license = query.getColumn(5).getString();
    if (!query.getColumn(6).isNull())
        pack.source = query.getColumn(6).getString();
    if (!query.getColumn(7).isNull())
        pack.importedAtUtc = query.getColumn(7).getString();
    if (!query.getColumn(8).isNull())
        pack.enabled = query.getColumn(8).getInt() != 0;
    if (!query.getColumn(9).isNull())
        pack.definitionJson = query.getColumn(9).getString();
    if (!query.getColumn(10).isNull())
        pack.metadata.tags = deserializeStringList(query.getColumn(10).getString());
    if (!query.getColumn(11).isNull())
        pack.metadata.createdAtUtc = query.getColumn(11).getString();
    if (!query.getColumn(12).isNull())
        pack.metadata.updatedAtUtc = query.getColumn(12).getString();
    return pack;
}

HouseRuleModel readHouseRule(SQLite::Statement &query)
{
    HouseRuleModel rule;
    if (!query.getColumn(0).isNull())
        rule.metadata.id = query.getColumn(0).getString();
    rule.label = query.getColumn(1).getString();
    if (!query.getColumn(2).isNull())
        rule.description = query.getColumn(2).getString();
    if (!query.getColumn(3).isNull())
        rule.enabled = query.getColumn(3).getInt() != 0;
    if (!query.getColumn(4).isNull())
        rule.changeHistory = deserializeHouseRuleChanges(query.getColumn(4).getString());
    if (!query.getColumn(5).isNull())
        rule.metadata.tags = deserializeStringList(query.getColumn(5).getString());
    if (!query.getColumn(6).isNull())
        rule.metadata.createdAtUtc = query.getColumn(6).getString();
    if (!query.getColumn(7).isNull())
        rule.metadata.updatedAtUtc = query.getColumn(7).getString();
    return rule;
}

CampaignSavedFilterModel readSavedFilter(SQLite::Statement &query)
{
    CampaignSavedFilterModel filter;
    if (!query.getColumn(0).isNull())
        filter.metadata.id = query.getColumn(0).getString();
    filter.name = query.getColumn(1).getString();
    if (!query.getColumn(2).isNull())
        filter.query = query.getColumn(2).getString();
    if (!query.getColumn(3).isNull())
        filter.entityTypes = deserializeStringList(query.getColumn(3).getString());
    if (!query.getColumn(4).isNull())
        filter.requiredTags = deserializeStringList(query.getColumn(4).getString());
    if (!query.getColumn(5).isNull())
        filter.matchAllTags = query.getColumn(5).getInt() != 0;
    if (!query.getColumn(6).isNull())
        filter.smartCollection = query.getColumn(6).getInt() != 0;
    if (!query.getColumn(7).isNull())
        filter.metadata.tags = deserializeStringList(query.getColumn(7).getString());
    if (!query.getColumn(8).isNull())
        filter.metadata.createdAtUtc = query.getColumn(8).getString();
    if (!query.getColumn(9).isNull())
        filter.metadata.updatedAtUtc = query.getColumn(9).getString();
    return filter;
}

CampaignEntityBookmarkModel readEntityBookmark(SQLite::Statement &query)
{
    CampaignEntityBookmarkModel bookmark;
    if (!query.getColumn(0).isNull())
        bookmark.metadata.id = query.getColumn(0).getString();
    if (!query.getColumn(1).isNull())
        bookmark.entityId = query.getColumn(1).getString();
    if (!query.getColumn(2).isNull())
        bookmark.entityType = query.getColumn(2).getString();
    if (!query.getColumn(3).isNull())
        bookmark.label = query.getColumn(3).getString();
    if (!query.getColumn(4).isNull())
        bookmark.favorite = query.getColumn(4).getInt() != 0;
    if (!query.getColumn(5).isNull())
        bookmark.lastOpenedAtUtc = query.getColumn(5).getString();
    if (!query.getColumn(6).isNull())
        bookmark.openCount = query.getColumn(6).getInt();
    if (!query.getColumn(7).isNull())
        bookmark.metadata.tags = deserializeStringList(query.getColumn(7).getString());
    if (!query.getColumn(8).isNull())
        bookmark.metadata.createdAtUtc = query.getColumn(8).getString();
    if (!query.getColumn(9).isNull())
        bookmark.metadata.updatedAtUtc = query.getColumn(9).getString();
    return bookmark;
}
