#pragma once

#include "Models/Campaign/Campaign.hpp"
#include "Models/Project/ProjectAssetImportService.hpp"

#include <filesystem>
#include <string>

namespace ProjectAssetRepair
{
struct AssetRepairRequest
{
    ProjectAssetImport::AssetKind kind = ProjectAssetImport::AssetKind::GenericFile;
    EntityId assetId;
    std::filesystem::path replacementPath;
    std::string displayName;
    std::string sourceReference;
};

struct AssetRepairResult
{
    bool success = false;
    std::string errorMessage;
    ProjectAssetImport::AssetImportResult importedAsset;
};

bool isProjectAssetMissing(const std::string &projectPath);
AssetRepairResult repairAsset(const Campaign &campaign, const AssetRepairRequest &request);
}
