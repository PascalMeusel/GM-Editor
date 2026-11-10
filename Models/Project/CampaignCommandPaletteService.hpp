#pragma once

#include "Models/Project/CampaignSearchIndex.hpp"

#include <string>
#include <vector>

struct CampaignCommandPaletteItem
{
    std::string id;
    std::string label;
    std::string category;
    EntityId entityId;
    std::string entityType;
};

namespace CampaignCommandPalette
{
std::vector<CampaignCommandPaletteItem> buildItems(const CampaignSearchIndex &index);
std::vector<CampaignCommandPaletteItem> filterItems(const std::vector<CampaignCommandPaletteItem> &items, const std::string &query, std::size_t limit = 50);
}
