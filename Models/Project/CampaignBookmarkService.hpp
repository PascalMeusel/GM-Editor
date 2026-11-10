#pragma once

#include "Models/Campaign/Campaign.hpp"
#include "Models/Project/CampaignCommandPaletteService.hpp"

#include <vector>

namespace CampaignBookmarks
{
void recordOpened(Campaign &campaign, const CampaignCommandPaletteItem &item);
void setFavorite(Campaign &campaign, const EntityId &entityId, const std::string &entityType, const std::string &label, bool favorite);
bool isFavorite(const Campaign &campaign, const EntityId &entityId);
std::vector<CampaignEntityBookmarkModel> favorites(const Campaign &campaign);
std::vector<CampaignEntityBookmarkModel> recent(const Campaign &campaign, std::size_t limit = 10);
}
