#pragma once

#include "Models/Campaign/Campaign.hpp"

#include <string>
#include <vector>

namespace CampaignTags
{
std::vector<std::string> normalize(std::vector<std::string> tags);
EntityMetadata *findEntityMetadata(Campaign &campaign, const EntityId &entityId);
const EntityMetadata *findEntityMetadata(const Campaign &campaign, const EntityId &entityId);
std::vector<std::string> tagsForEntity(const Campaign &campaign, const EntityId &entityId);
bool setTagsForEntity(Campaign &campaign, const EntityId &entityId, const std::vector<std::string> &tags);
bool isEntityArchived(const Campaign &campaign, const EntityId &entityId);
bool setEntityArchived(Campaign &campaign, const EntityId &entityId, bool archived);
std::vector<std::string> collectAllTags(const Campaign &campaign);
}
