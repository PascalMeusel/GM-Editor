#pragma once

#include "Models/Project/ProjectManifestService.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace ProjectPackageImport
{
enum class ConflictResolution
{
    Cancel,
    ReplaceExisting,
    CloneAsCopy
};

struct ImportRequest
{
    std::filesystem::path packagePath;
    ConflictResolution conflictResolution = ConflictResolution::Cancel;
    // Required for CloneAsCopy. The imported campaign receives a new stable ID
    // and all campaign-owned entity IDs/references are remapped.
    std::string cloneCampaignName;
    // Kept for source compatibility with callers created before conflict previews.
    bool overwriteExisting = false;
};

struct ImportPreview
{
    bool valid = false;
    std::string errorMessage;
    ProjectManifest::ProjectManifestModel manifest;
    std::filesystem::path sourceDatabasePath;
    std::filesystem::path targetPackagePath;
    bool packageConflict = false;
    bool repositoryConflict = false;
    std::uintmax_t fileCount = 0;
    std::uintmax_t byteCount = 0;
    std::vector<std::string> warnings;
};

struct ImportResult
{
    bool success = false;
    std::string errorMessage;
    std::filesystem::path importedPackagePath;
    std::filesystem::path backupPath;
    ProjectManifest::ProjectManifestModel manifest;
    std::string importedCampaignName;
    bool importedAsCopy = false;
    bool restoredToRepository = false;
    std::uintmax_t fileCount = 0;
    std::uintmax_t byteCount = 0;
};

ImportPreview previewCampaignPackage(const std::filesystem::path &packagePath);
ImportResult importCampaignPackage(const ImportRequest &request);
}
