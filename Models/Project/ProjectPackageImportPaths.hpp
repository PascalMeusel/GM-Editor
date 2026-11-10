#pragma once

#include "Models/Project/WorldContextLinks.hpp"
#include "ProjectPackageImportService.hpp"
#include "ProjectDirectoryPublish.hpp"

#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"

#include "SQLiteCpp/SQLiteCpp.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <stdexcept>
#include <unordered_map>

namespace
{
constexpr int kCurrentProjectSchemaVersion = ProjectManifest::CurrentSchemaVersion;

std::string trimmed(std::string value);

std::filesystem::path packageDirectory(const std::filesystem::path &selection)
{
    std::string value = trimmed(selection.string());
    if (value.size() >= 2 &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\'')))
        value = value.substr(1, value.size() - 2);
    const std::filesystem::path normalized = std::filesystem::path(value).lexically_normal();
    return normalized.filename() == "project.json" ? normalized.parent_path() : normalized;
}

bool isSafeRelativePath(const std::filesystem::path &path)
{
    if (path.empty() || path.is_absolute() || path.has_root_path())
        return false;
    for (const auto &component : path.lexically_normal())
    {
        if (component == "..")
            return false;
    }
    return true;
}

void copyDirectoryContents(const std::filesystem::path &source,
                           const std::filesystem::path &destination,
                           std::uintmax_t *fileCount = nullptr,
                           std::uintmax_t *byteCount = nullptr)
{
    std::filesystem::create_directories(destination);
    for (const std::filesystem::directory_entry &entry : std::filesystem::recursive_directory_iterator(source))
    {
        const std::filesystem::path relativePath = std::filesystem::relative(entry.path(), source);
        const std::filesystem::path targetPath = destination / relativePath;
        if (entry.is_directory())
        {
            std::filesystem::create_directories(targetPath);
            continue;
        }
        if (!entry.is_regular_file())
            continue;

        std::filesystem::create_directories(targetPath.parent_path());
        std::filesystem::copy_file(entry.path(), targetPath, std::filesystem::copy_options::overwrite_existing);
        if (fileCount)
            ++*fileCount;
        if (byteCount)
            *byteCount += entry.file_size();
    }
}

bool collectPackageSize(const std::filesystem::path &source,
                        std::uintmax_t &fileCount,
                        std::uintmax_t &byteCount,
                        std::string &errorMessage)
{
    fileCount = 0;
    byteCount = 0;
    for (const std::filesystem::directory_entry &entry : std::filesystem::recursive_directory_iterator(source))
    {
        if (entry.is_symlink())
        {
            errorMessage = "Campaign packages may not contain symbolic links.";
            return false;
        }
        if (!entry.is_regular_file())
            continue;
        ++fileCount;
        byteCount += entry.file_size();
    }
    return true;
}

bool snapshotContainsCampaign(const std::filesystem::path &databasePath,
                              const std::string &campaignId,
                              const std::string &campaignName,
                              std::string &errorMessage)
{
    try
    {
        SQLite::Database database(databasePath.string(), SQLite::OPEN_READONLY);
        SQLite::Statement integrity(database, "PRAGMA integrity_check;");
        if (!integrity.executeStep() || integrity.getColumn(0).getString() != "ok")
        {
            errorMessage = "The package database failed its integrity check.";
            return false;
        }

        SQLite::Statement campaign(database, "SELECT name FROM campaigns WHERE stable_id = ? LIMIT 1;");
        campaign.bind(1, campaignId);
        if (!campaign.executeStep())
        {
            errorMessage = "The package database does not contain the campaign declared by its manifest.";
            return false;
        }
        if (campaignName != campaign.getColumn(0).getString())
        {
            errorMessage = "The campaign name in project.json does not match the package database.";
            return false;
        }
        errorMessage.clear();
        return true;
    }
    catch (const std::exception &e)
    {
        errorMessage = std::string("Could not inspect the package database: ") + e.what();
        return false;
    }
}

std::filesystem::path uniqueSibling(const std::filesystem::path &parent, const std::string &prefix)
{
    return parent / ProjectAssets::sanitizePathSegment(prefix + "-" + createEntityId("import"));
}

std::string trimmed(std::string value)
{
    const auto notSpace = [](unsigned char character) { return !std::isspace(character); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

using IdRemap = std::unordered_map<EntityId, EntityId>;

// A cloned package owns independent copies of these files. Retaining the old
// project-relative prefix would silently make the clone depend on its original.
} // namespace
