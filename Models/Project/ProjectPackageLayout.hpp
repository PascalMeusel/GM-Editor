#pragma once

#include "Models/Campaign/Campaign.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace ProjectPackageLayout
{
struct CampaignPackageLayout
{
    std::filesystem::path rootDirectory;
    std::filesystem::path databasePath;
    std::filesystem::path manifestPath;
    std::filesystem::path assetsDirectory;
    std::filesystem::path recoveryDirectory;
    std::filesystem::path backupsDirectory;
    std::filesystem::path exportsDirectory;
};

std::filesystem::path findProjectRoot();
std::filesystem::path campaignsDirectory();
std::filesystem::path campaignPackageDirectory(const EntityId &campaignId);
std::filesystem::path campaignPackageDirectory(const Campaign &campaign);
CampaignPackageLayout layoutForCampaign(const Campaign &campaign);
std::vector<std::string> assetFolderNames();
std::filesystem::path assetDirectory(const Campaign &campaign, const std::string &assetFolder);
void ensureCampaignPackageLayout(const Campaign &campaign);
}
