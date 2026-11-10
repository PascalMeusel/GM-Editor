#include "CampaignRepository.hpp"

#include "Models/DataBaseManager.hpp"

namespace CampaignRepository
{
void saveCampaign(const Campaign &campaign)
{
    DatabaseManager::getInstance()->saveCampaign(campaign);
}

void deleteCampaignByStableId(const std::string &campaignStableId)
{
    DatabaseManager::getInstance()->deleteCampaignByStableId(campaignStableId);
}

std::vector<Campaign> loadAllCampaigns()
{
    return DatabaseManager::getInstance()->loadAllCampaigns();
}

std::vector<Campaign> loadCampaignSummaries()
{
    return DatabaseManager::getInstance()->loadCampaignSummaries();
}

Campaign &loadCampaign(const std::string &campaignName)
{
    return DatabaseManager::getInstance()->loadCampaign(campaignName);
}

Campaign &activeCampaign()
{
    return DatabaseManager::getInstance()->getActiveCampaign();
}

void saveInstalledRulesetDefinition(const RulesetDefinitionModel &definition)
{
    DatabaseManager::getInstance()->saveInstalledRulesetDefinition(definition);
}

void removeInstalledRulesetDefinition(const std::string &stableId)
{
    DatabaseManager::getInstance()->removeInstalledRulesetDefinition(stableId);
}

std::vector<RulesetDefinitionModel> loadInstalledRulesetDefinitions()
{
    return DatabaseManager::getInstance()->loadInstalledRulesetDefinitions();
}

void writeSnapshot(const std::string &snapshotPath)
{
    DatabaseManager::getInstance()->writeSnapshot(snapshotPath);
}

void writeCampaignSnapshot(const Campaign &campaign, const std::string &snapshotPath)
{
    DatabaseManager::writeCampaignSnapshot(campaign, snapshotPath);
}

void writeRecoverySnapshot(const std::string &snapshotPath)
{
    DatabaseManager::getInstance()->writeRecoverySnapshot(snapshotPath);
}

bool readCampaignFromSnapshot(const std::string &snapshotPath,
                              const std::string &campaignStableId,
                              Campaign &campaign,
                              std::string &errorMessage)
{
    return DatabaseManager::readCampaignFromSnapshot(snapshotPath, campaignStableId, campaign, errorMessage);
}
}
