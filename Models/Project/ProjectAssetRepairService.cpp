#include "ProjectAssetRepairService.hpp"

#include "Models/Project/ProjectAssetService.hpp"

namespace ProjectAssetRepair
{
bool isProjectAssetMissing(const std::string &projectPath)
{
    return projectPath.empty() || !std::filesystem::exists(ProjectAssets::resolveProjectPath(projectPath));
}

AssetRepairResult repairAsset(const Campaign &campaign, const AssetRepairRequest &request)
{
    AssetRepairResult result;
    ProjectAssetImport::AssetImportRequest importRequest;
    importRequest.kind = request.kind;
    importRequest.sourcePath = request.replacementPath;
    importRequest.assetId = request.assetId;
    importRequest.displayName = request.displayName;
    importRequest.sourceReference = request.sourceReference;
    importRequest.deduplicateByHash = false;

    result.importedAsset = ProjectAssetImport::importAsset(campaign, importRequest);
    result.success = result.importedAsset.success;
    result.errorMessage = result.importedAsset.errorMessage;
    return result;
}
}
