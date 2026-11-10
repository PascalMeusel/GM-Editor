#pragma once

#include "Models/Campaign/Campaign.hpp"
#include "Models/Rulesets/RulesetDefinitionModel.hpp"

#include <string>
#include <vector>

namespace CampaignRepository
{
void saveCampaign(const Campaign &campaign);
void deleteCampaignByStableId(const std::string &campaignStableId);
std::vector<Campaign> loadCampaignSummaries();
std::vector<Campaign> loadAllCampaigns();
Campaign &loadCampaign(const std::string &campaignName);
Campaign &activeCampaign();
void saveInstalledRulesetDefinition(const RulesetDefinitionModel &definition);
void removeInstalledRulesetDefinition(const std::string &stableId);
std::vector<RulesetDefinitionModel> loadInstalledRulesetDefinitions();
void writeSnapshot(const std::string &snapshotPath);
void writeCampaignSnapshot(const Campaign &campaign, const std::string &snapshotPath);
void writeRecoverySnapshot(const std::string &snapshotPath);
bool readCampaignFromSnapshot(const std::string &snapshotPath,
                              const std::string &campaignStableId,
                              Campaign &campaign,
                              std::string &errorMessage);
}
