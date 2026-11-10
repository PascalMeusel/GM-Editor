#pragma once

#include "Models/Campaign/Campaign.hpp"

#include <string>
#include <vector>

namespace ProjectAssetCollection
{
struct CollectionResult
{
    int totalReferences = 0;
    int collectedAssets = 0;
    int skippedAlreadyPackaged = 0;
    int missingAssets = 0;
    std::vector<std::string> errors;
};

CollectionResult collectAssetsIntoCampaignFolder(Campaign &campaign);
}
