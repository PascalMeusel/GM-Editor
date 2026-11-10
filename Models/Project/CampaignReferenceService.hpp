#pragma once

#include "Models/Campaign/Campaign.hpp"

#include <string>
#include <vector>

struct CampaignBacklink
{
    EntityId sourceEntityId;
    std::string sourceType;
    std::string sourceTitle;
    std::string relationship;
};

struct CampaignBrokenReference
{
    EntityId sourceEntityId;
    std::string sourceType;
    std::string sourceTitle;
    std::string relationship;
    EntityId missingEntityId;
};

namespace CampaignReferences
{
std::vector<CampaignBacklink> backlinksTo(const Campaign &campaign, const EntityId &targetEntityId);
std::vector<CampaignBrokenReference> brokenReferences(const Campaign &campaign);
}
