#include "CampaignBookmarkService.hpp"

#include <algorithm>

namespace
{
CampaignEntityBookmarkModel *findBookmark(Campaign &campaign, const EntityId &entityId)
{
    for (CampaignEntityBookmarkModel &bookmark : campaign.entityBookmarks)
    {
        if (bookmark.entityId == entityId)
            return &bookmark;
    }
    return nullptr;
}

const CampaignEntityBookmarkModel *findBookmark(const Campaign &campaign, const EntityId &entityId)
{
    for (const CampaignEntityBookmarkModel &bookmark : campaign.entityBookmarks)
    {
        if (bookmark.entityId == entityId)
            return &bookmark;
    }
    return nullptr;
}

CampaignEntityBookmarkModel &upsertBookmark(Campaign &campaign, const EntityId &entityId, const std::string &entityType, const std::string &label)
{
    if (CampaignEntityBookmarkModel *existing = findBookmark(campaign, entityId))
    {
        existing->entityType = entityType;
        existing->label = label;
        return *existing;
    }

    CampaignEntityBookmarkModel bookmark;
    bookmark.entityId = entityId;
    bookmark.entityType = entityType;
    bookmark.label = label;
    campaign.entityBookmarks.push_back(bookmark);
    return campaign.entityBookmarks.back();
}
}

namespace CampaignBookmarks
{
void recordOpened(Campaign &campaign, const CampaignCommandPaletteItem &item)
{
    if (item.entityId.empty())
        return;

    CampaignEntityBookmarkModel &bookmark = upsertBookmark(campaign, item.entityId, item.entityType, item.label);
    bookmark.openCount += 1;
    bookmark.lastOpenedAtUtc = currentUtcTimestamp();
    bookmark.metadata.touch();
}

void setFavorite(Campaign &campaign, const EntityId &entityId, const std::string &entityType, const std::string &label, bool favorite)
{
    if (entityId.empty())
        return;

    CampaignEntityBookmarkModel &bookmark = upsertBookmark(campaign, entityId, entityType, label);
    bookmark.favorite = favorite;
    bookmark.metadata.touch();
}

bool isFavorite(const Campaign &campaign, const EntityId &entityId)
{
    const CampaignEntityBookmarkModel *bookmark = findBookmark(campaign, entityId);
    return bookmark != nullptr && bookmark->favorite;
}

std::vector<CampaignEntityBookmarkModel> favorites(const Campaign &campaign)
{
    std::vector<CampaignEntityBookmarkModel> result;
    for (const CampaignEntityBookmarkModel &bookmark : campaign.entityBookmarks)
    {
        if (bookmark.favorite)
            result.push_back(bookmark);
    }
    std::sort(result.begin(), result.end(), [](const CampaignEntityBookmarkModel &left, const CampaignEntityBookmarkModel &right) {
        return left.label < right.label;
    });
    return result;
}

std::vector<CampaignEntityBookmarkModel> recent(const Campaign &campaign, std::size_t limit)
{
    std::vector<CampaignEntityBookmarkModel> result;
    for (const CampaignEntityBookmarkModel &bookmark : campaign.entityBookmarks)
    {
        if (!bookmark.lastOpenedAtUtc.empty())
            result.push_back(bookmark);
    }
    std::sort(result.begin(), result.end(), [](const CampaignEntityBookmarkModel &left, const CampaignEntityBookmarkModel &right) {
        if (left.lastOpenedAtUtc == right.lastOpenedAtUtc)
            return left.openCount > right.openCount;
        return left.lastOpenedAtUtc > right.lastOpenedAtUtc;
    });
    if (result.size() > limit)
        result.resize(limit);
    return result;
}
}
