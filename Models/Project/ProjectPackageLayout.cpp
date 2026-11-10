#include "ProjectPackageLayout.hpp"

#include "Models/Project/ProjectAssetService.hpp"

#include <array>

namespace ProjectPackageLayout
{
std::filesystem::path findProjectRoot()
{
    return ProjectAssets::findProjectRoot();
}

std::filesystem::path campaignsDirectory()
{
    return findProjectRoot() / "Resources" / "Campaigns";
}

std::filesystem::path campaignPackageDirectory(const EntityId &campaignId)
{
    return campaignsDirectory() / ProjectAssets::sanitizePathSegment(campaignId);
}

std::filesystem::path campaignPackageDirectory(const Campaign &campaign)
{
    return campaignPackageDirectory(campaign.metadata.id);
}

CampaignPackageLayout layoutForCampaign(const Campaign &campaign)
{
    CampaignPackageLayout layout;
    layout.rootDirectory = campaignPackageDirectory(campaign);
    layout.databasePath = layout.rootDirectory / "campaign.db";
    layout.manifestPath = layout.rootDirectory / "project.json";
    layout.assetsDirectory = layout.rootDirectory / "Assets";
    layout.recoveryDirectory = layout.rootDirectory / "Recovery";
    layout.backupsDirectory = layout.rootDirectory / "Backups";
    layout.exportsDirectory = layout.rootDirectory / "Exports";
    return layout;
}

std::vector<std::string> assetFolderNames()
{
    return {"Images", "Portraits", "Tokens", "Maps", "Audio", "Handouts", "Files"};
}

std::filesystem::path assetDirectory(const Campaign &campaign, const std::string &assetFolder)
{
    return layoutForCampaign(campaign).assetsDirectory / ProjectAssets::sanitizePathSegment(assetFolder);
}

void ensureCampaignPackageLayout(const Campaign &campaign)
{
    const CampaignPackageLayout layout = layoutForCampaign(campaign);
    std::filesystem::create_directories(layout.rootDirectory);
    std::filesystem::create_directories(layout.assetsDirectory);
    std::filesystem::create_directories(layout.recoveryDirectory);
    std::filesystem::create_directories(layout.backupsDirectory);
    std::filesystem::create_directories(layout.exportsDirectory);

    for (const std::string &folderName : assetFolderNames())
        std::filesystem::create_directories(assetDirectory(campaign, folderName));
}
}
