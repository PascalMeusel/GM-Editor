#include "Models/DataBaseManager.hpp"
#include "Models/Project/CampaignTagService.hpp"

void DatabaseManager::saveCampaignEntityProperties(int64_t campaignId, const Campaign &campaign)
{
    SQLite::Statement remove(*m_db, "DELETE FROM campaign_entity_properties WHERE campaign_id = ?;");
    remove.bind(1, campaignId); remove.exec();
    SQLite::Statement insert(*m_db, "INSERT INTO campaign_entity_properties (campaign_id, entity_id, property_key, property_value) VALUES (?, ?, ?, ?);");
    const auto append = [&](const EntityMetadata &metadata) {
        for (const auto &property : metadata.customProperties)
        {
            insert.bind(1, campaignId); insert.bind(2, metadata.id);
            insert.bind(3, property.first); insert.bind(4, property.second);
            insert.exec(); insert.reset();
        }
    };
    append(campaign.metadata);
    const auto collection = [&](const auto &entries) { for (const auto &entry : entries) append(entry.metadata); };
    collection(campaign.characters); collection(campaign.npcs); collection(campaign.factions);
    collection(campaign.deities); collection(campaign.nations); collection(campaign.worldPlaces);
    collection(campaign.customItems); collection(campaign.customSpells); collection(campaign.customMonsters);
    collection(campaign.maps); collection(campaign.encounters); collection(campaign.shops);
    collection(campaign.dungeons); collection(campaign.handouts); collection(campaign.sessions);
    collection(campaign.storyThreads); collection(campaign.timelineEvents);
}

void DatabaseManager::loadCampaignEntityProperties(Campaign &campaign)
{
    SQLite::Statement query(*m_db, "SELECT entity_id, property_key, property_value FROM campaign_entity_properties WHERE campaign_id = ?;");
    query.bind(1, static_cast<int64_t>(campaign.id));
    while (query.executeStep())
        if (auto *metadata = CampaignTags::findEntityMetadata(campaign, query.getColumn(0).getString()))
            metadata->customProperties[query.getColumn(1).getString()] = query.getColumn(2).getString();
}
