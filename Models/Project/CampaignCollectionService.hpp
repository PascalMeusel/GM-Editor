#pragma once

#include "Models/Project/CampaignCollectionModels.hpp"
#include "Models/Project/CampaignSearchIndex.hpp"

#include <vector>

namespace CampaignCollections
{
std::vector<CampaignSearchResult> evaluate(const CampaignSearchIndex &index,
                                           const CampaignSavedFilterModel &filter,
                                           std::size_t limit = 100);
}
