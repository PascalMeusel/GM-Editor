#pragma once

#include "Models/Campaign/Campaign.hpp"

#include <SQLiteCpp/SQLiteCpp.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace TabletopToolkitPersistence
{
template <typename Records>
inline void validateOwnedStableIds(SQLite::Database &database,
                                   std::int64_t campaignId,
                                   const char *table,
                                   const Records &records)
{
    std::unordered_set<std::string> seen;
    SQLite::Statement owner(database, std::string("SELECT campaign_id FROM ") + table + " WHERE stable_id = ?;");
    for (const auto &record : records)
    {
        const std::string &stableId = record.metadata.id;
        if (stableId.empty() || !seen.insert(stableId).second)
            throw std::runtime_error(std::string("Empty or duplicate tabletop toolkit identity in ") + table);
        owner.bind(1, stableId);
        if (owner.executeStep() && owner.getColumn(0).getInt64() != campaignId)
            throw std::runtime_error(std::string("Tabletop toolkit identity belongs to another campaign in ") + table + ".");
        owner.reset();
    }
}

inline void validateCampaignTabletopToolkitIdentities(SQLite::Database &database,
                                                       std::int64_t campaignId,
                                                       const Campaign &campaign)
{
    validateOwnedStableIds(database, campaignId, "campaign_dice_macros", campaign.diceMacros);
    validateOwnedStableIds(database, campaignId, "story_objectives", campaign.storyObjectives);
    validateOwnedStableIds(database, campaignId, "story_clues", campaign.storyClues);
    validateOwnedStableIds(database, campaignId, "campaign_safety_boundaries", campaign.safetyProfile.boundaries);
    validateOwnedStableIds(database, campaignId, "campaign_safety_warnings", campaign.safetyProfile.warnings);
}
} // namespace TabletopToolkitPersistence
