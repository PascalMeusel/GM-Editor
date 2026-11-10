#include "Models/Project/ProjectPackageImportCloneRemap.hpp"

namespace ProjectPackageImport
{
ImportPreview previewCampaignPackage(const std::filesystem::path &selectedPath)
{
    ImportPreview preview;
    try
    {
        const std::filesystem::path source = packageDirectory(selectedPath);
        if (source.empty() || !std::filesystem::exists(source) || !std::filesystem::is_directory(source))
        {
            preview.errorMessage = "Choose an existing .ttrpgpkg campaign folder or its project.json file.";
            return preview;
        }
        if (std::filesystem::is_symlink(source))
        {
            preview.errorMessage = "Campaign package folders may not be symbolic links.";
            return preview;
        }

        std::string manifestError;
        preview.manifest = ProjectManifest::loadManifest(source / "project.json", manifestError);
        if (!manifestError.empty())
        {
            preview.errorMessage = manifestError;
            return preview;
        }
        if (preview.manifest.format != "ttrpg-editor.project.v1")
        {
            preview.errorMessage = "This package format is not supported: " + preview.manifest.format;
            return preview;
        }
        if (preview.manifest.schemaVersion <= 0 || preview.manifest.schemaVersion > kCurrentProjectSchemaVersion)
        {
            preview.errorMessage = "This package uses a newer or invalid database schema.";
            return preview;
        }
        if (preview.manifest.campaignId.empty() || preview.manifest.campaignName.empty())
        {
            preview.errorMessage = "Project manifest is missing campaignId or campaignName.";
            return preview;
        }

        const std::filesystem::path databaseRelativePath(preview.manifest.databasePath);
        if (!isSafeRelativePath(databaseRelativePath))
        {
            preview.errorMessage = "Project manifest contains an unsafe database path.";
            return preview;
        }
        preview.sourceDatabasePath = source / databaseRelativePath;
        if (!std::filesystem::exists(preview.sourceDatabasePath) || !std::filesystem::is_regular_file(preview.sourceDatabasePath))
        {
            preview.errorMessage = "The package database snapshot is missing.";
            return preview;
        }
        if (!snapshotContainsCampaign(preview.sourceDatabasePath,
                                      preview.manifest.campaignId,
                                      preview.manifest.campaignName,
                                      preview.errorMessage))
            return preview;

        preview.targetPackagePath = ProjectPackageLayout::campaignPackageDirectory(preview.manifest.campaignId);
        preview.packageConflict = std::filesystem::exists(preview.targetPackagePath);
        for (const Campaign &campaign : CampaignRepository::loadCampaignSummaries())
        {
            if (campaign.metadata.id == preview.manifest.campaignId || campaign.name == preview.manifest.campaignName)
            {
                preview.repositoryConflict = true;
                break;
            }
        }
        if (!collectPackageSize(source, preview.fileCount, preview.byteCount, preview.errorMessage))
            return preview;
        if (source.extension() != ".ttrpgpkg")
            preview.warnings.push_back("The selected folder does not use the .ttrpgpkg extension, but its contents are valid.");
        preview.valid = true;
    }
    catch (const std::exception &e)
    {
        preview.errorMessage = e.what();
    }
    return preview;
}

ImportResult importCampaignPackage(const ImportRequest &request)
{
    ImportResult result;
    const ImportPreview preview = previewCampaignPackage(request.packagePath);
    result.manifest = preview.manifest;
    if (!preview.valid)
    {
        result.errorMessage = preview.errorMessage;
        return result;
    }

    const bool hasConflict = preview.packageConflict || preview.repositoryConflict;
    const bool replaceExisting = request.conflictResolution == ConflictResolution::ReplaceExisting || request.overwriteExisting;
    const bool cloneAsCopy = request.conflictResolution == ConflictResolution::CloneAsCopy;
    if (hasConflict && !replaceExisting && !cloneAsCopy)
    {
        result.errorMessage = "A campaign with this ID or name already exists. Review the conflict and explicitly choose Replace Existing.";
        return result;
    }

    const std::string cloneName = trimmed(request.cloneCampaignName);
    if (cloneAsCopy)
    {
        if (cloneName.empty())
        {
            result.errorMessage = "Enter a name for the imported copy.";
            return result;
        }
        for (const Campaign &campaign : CampaignRepository::loadCampaignSummaries())
        {
            if (campaign.name == cloneName)
            {
                result.errorMessage = "A campaign with the copy name already exists. Choose another name.";
                return result;
            }
        }
    }

    const std::filesystem::path source = packageDirectory(request.packagePath);
    const std::filesystem::path campaignsDirectory = ProjectPackageLayout::campaignsDirectory();
    const std::filesystem::path stagingPath = uniqueSibling(campaignsDirectory, ".import-staging");
    const std::filesystem::path displacedPath = uniqueSibling(campaignsDirectory, ".import-replaced");
    std::filesystem::path targetPackagePath = preview.targetPackagePath;
    bool displacedExistingPackage = false;
    bool clonedCampaignSaved = false;

    try
    {
        std::filesystem::create_directories(campaignsDirectory);
        copyDirectoryContents(source, stagingPath, &result.fileCount, &result.byteCount);

        Campaign importedCampaign;
        std::string campaignError;
        const std::filesystem::path stagedDatabase = stagingPath / preview.manifest.databasePath;
        if (!CampaignRepository::readCampaignFromSnapshot(stagedDatabase.string(),
                                                          preview.manifest.campaignId,
                                                          importedCampaign,
                                                          campaignError))
            throw std::runtime_error(campaignError);

        if (cloneAsCopy)
        {
            remapCampaignAsClone(importedCampaign, cloneName);
            targetPackagePath = ProjectPackageLayout::campaignPackageDirectory(importedCampaign.metadata.id);
            while (std::filesystem::exists(targetPackagePath))
            {
                importedCampaign.metadata.id = createEntityId("campaign");
                targetPackagePath = ProjectPackageLayout::campaignPackageDirectory(importedCampaign.metadata.id);
            }
            rebaseClonedAssets(importedCampaign, preview.manifest.campaignId);
            result.manifest.campaignId = importedCampaign.metadata.id;
            result.manifest.campaignName = importedCampaign.name;
            result.importedAsCopy = true;
        }
        result.importedPackagePath = targetPackagePath;

        Campaign previousCampaign;
        bool previousCampaignLoaded = false;
        if (preview.repositoryConflict)
        {
            for (const Campaign &campaign : CampaignRepository::loadAllCampaigns())
            {
                if (campaign.metadata.id == preview.manifest.campaignId || campaign.name == preview.manifest.campaignName)
                {
                    previousCampaign = campaign;
                    previousCampaignLoaded = true;
                    break;
                }
            }
        }

        if (hasConflict && replaceExisting)
        {
            result.backupPath = uniqueSibling(campaignsDirectory / "ImportBackups", preview.manifest.campaignId);
            std::filesystem::create_directories(result.backupPath);
            CampaignRepository::writeSnapshot((result.backupPath / "repository-before-import.db").string());
            if (preview.packageConflict)
                copyDirectoryContents(preview.targetPackagePath, result.backupPath / "package-before-import.ttrpgpkg");
        }

        if (preview.packageConflict && replaceExisting)
        {
            ProjectDirectoryPublish::rename(preview.targetPackagePath, displacedPath);
            displacedExistingPackage = true;
        }
        ProjectDirectoryPublish::rename(stagingPath, targetPackagePath);

        try
        {
            CampaignRepository::saveCampaign(importedCampaign);
            clonedCampaignSaved = cloneAsCopy;
            if (cloneAsCopy)
            {
                const std::filesystem::path clonedDatabase = targetPackagePath / result.manifest.databasePath;
                std::error_code removeError;
                std::filesystem::remove(clonedDatabase, removeError);
                CampaignRepository::writeCampaignSnapshot(importedCampaign, clonedDatabase.u8string());
                std::string manifestError;
                if (!ProjectManifest::writeManifest(importedCampaign, preview.manifest.recovery, manifestError))
                    throw std::runtime_error("Could not finalize the copied package manifest: " + manifestError);
            }
        }
        catch (...)
        {
            std::error_code cleanupError;
            if (std::filesystem::exists(targetPackagePath))
                std::filesystem::remove_all(targetPackagePath, cleanupError);
            if (displacedExistingPackage && std::filesystem::exists(displacedPath))
                ProjectDirectoryPublish::rename(displacedPath, targetPackagePath);
            if (previousCampaignLoaded)
                CampaignRepository::saveCampaign(previousCampaign);
            if (clonedCampaignSaved)
                CampaignRepository::deleteCampaignByStableId(importedCampaign.metadata.id);
            throw;
        }

        std::error_code cleanupError;
        if (displacedExistingPackage && std::filesystem::exists(displacedPath))
            std::filesystem::remove_all(displacedPath, cleanupError);
        result.importedCampaignName = importedCampaign.name;
        result.restoredToRepository = true;
        result.success = true;
    }
    catch (const std::exception &e)
    {
        result.errorMessage = e.what();
        std::error_code cleanupError;
        if (std::filesystem::exists(stagingPath))
            std::filesystem::remove_all(stagingPath, cleanupError);
        if (displacedExistingPackage && std::filesystem::exists(displacedPath) && !std::filesystem::exists(targetPackagePath))
            ProjectDirectoryPublish::rename(displacedPath, targetPackagePath);
    }
    return result;
}
}
