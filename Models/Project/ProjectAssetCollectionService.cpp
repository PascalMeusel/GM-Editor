#include "ProjectAssetCollectionService.hpp"

#include "Models/Project/ProjectAssetImportService.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"

namespace
{
bool isInsideDirectory(const std::filesystem::path &path, const std::filesystem::path &directory)
{
    const std::filesystem::path normalizedPath = std::filesystem::weakly_canonical(path);
    const std::filesystem::path normalizedDirectory = std::filesystem::weakly_canonical(directory);
    auto pathIt = normalizedPath.begin();
    for (auto directoryIt = normalizedDirectory.begin(); directoryIt != normalizedDirectory.end(); ++directoryIt, ++pathIt)
    {
        if (pathIt == normalizedPath.end() || *pathIt != *directoryIt)
            return false;
    }
    return true;
}

bool collectPath(Campaign &campaign,
                 ProjectAssetImport::AssetKind kind,
                 const EntityId &assetId,
                 const std::string &displayName,
                 std::string &storedPath,
                 ProjectAssetCollection::CollectionResult &result)
{
    if (storedPath.empty())
        return false;

    ++result.totalReferences;
    const std::filesystem::path sourcePath = ProjectAssets::resolveProjectPath(storedPath);
    if (!std::filesystem::exists(sourcePath) || !std::filesystem::is_regular_file(sourcePath))
    {
        ++result.missingAssets;
        result.errors.push_back("Missing asset: " + storedPath);
        return false;
    }

    if (isInsideDirectory(sourcePath, ProjectPackageLayout::layoutForCampaign(campaign).assetsDirectory))
    {
        ++result.skippedAlreadyPackaged;
        return false;
    }

    ProjectAssetImport::AssetImportRequest request;
    request.kind = kind;
    request.sourcePath = sourcePath;
    request.assetId = assetId;
    request.displayName = displayName;
    request.sourceReference = storedPath;

    const ProjectAssetImport::AssetImportResult imported = ProjectAssetImport::importAsset(campaign, request);
    if (!imported.success)
    {
        result.errors.push_back(imported.errorMessage);
        return false;
    }

    storedPath = imported.projectPath;
    ++result.collectedAssets;
    return true;
}
}

namespace ProjectAssetCollection
{
CollectionResult collectAssetsIntoCampaignFolder(Campaign &campaign)
{
    CollectionResult result;
    ProjectPackageLayout::ensureCampaignPackageLayout(campaign);

    for (CharacterModel &character : campaign.characters)
    {
        if (collectPath(campaign, ProjectAssetImport::AssetKind::Portrait, character.metadata.id, character.nameBuffer, character.portraitAssetId, result))
            character.metadata.touch();
    }

    for (MapModel &map : campaign.maps)
    {
        if (collectPath(campaign, ProjectAssetImport::AssetKind::Map, map.metadata.id + "_background", map.name, map.backgroundImagePath, result))
            map.metadata.touch();
    }

    for(ItemModel &item:campaign.customItems) {
        const auto image=item.metadata.customProperties.find("item.image");
        if(image!=item.metadata.customProperties.end() &&
           collectPath(campaign,ProjectAssetImport::AssetKind::Image,item.metadata.id+"_image",item.name,image->second,result))item.metadata.touch();
    }

    for (AudioAssetModel &asset : campaign.audioAssets)
    {
        const std::string previousPath = asset.filePath;
        if (collectPath(campaign, ProjectAssetImport::AssetKind::Audio, asset.metadata.id, asset.name, asset.filePath, result))
        {
            asset.originalSourcePath = asset.originalSourcePath.empty() ? previousPath : asset.originalSourcePath;
            asset.originalFileName = std::filesystem::path(previousPath).filename().string();
            asset.metadata.touch();
        }
    }

    for (HandoutModel &handout : campaign.handouts)
    {
        if (collectPath(campaign, ProjectAssetImport::AssetKind::Handout, handout.metadata.id, handout.title, handout.filePath, result))
            handout.metadata.touch();
    }

    return result;
}
}
