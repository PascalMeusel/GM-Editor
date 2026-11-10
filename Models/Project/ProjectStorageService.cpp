#include "ProjectStorageService.hpp"

#include "Models/Project/ProjectPackageLayout.hpp"

#include <algorithm>

namespace
{
ProjectStorage::StorageBucket measurePath(const std::string &name, const std::filesystem::path &path)
{
    ProjectStorage::StorageBucket bucket;
    bucket.name = name;
    bucket.path = path;

    if (!std::filesystem::exists(path))
        return bucket;

    if (std::filesystem::is_regular_file(path))
    {
        bucket.fileCount = 1;
        bucket.byteCount = std::filesystem::file_size(path);
        return bucket;
    }

    for (const std::filesystem::directory_entry &entry : std::filesystem::recursive_directory_iterator(path))
    {
        if (!entry.is_regular_file())
            continue;
        ++bucket.fileCount;
        bucket.byteCount += entry.file_size();
    }

    return bucket;
}

bool directoryIsEmpty(const std::filesystem::path &path)
{
    return std::filesystem::exists(path) && std::filesystem::is_directory(path) && std::filesystem::directory_iterator(path) == std::filesystem::directory_iterator{};
}
}

namespace ProjectStorage
{
StorageUsageReport measureCampaignStorage(const Campaign &campaign)
{
    const ProjectPackageLayout::CampaignPackageLayout layout = ProjectPackageLayout::layoutForCampaign(campaign);
    StorageUsageReport report;
    report.buckets.push_back(measurePath("Database", layout.databasePath));
    report.buckets.push_back(measurePath("Manifest", layout.manifestPath));
    report.buckets.push_back(measurePath("Assets", layout.assetsDirectory));
    report.buckets.push_back(measurePath("Recovery", layout.recoveryDirectory));
    report.buckets.push_back(measurePath("Backups", layout.backupsDirectory));
    report.buckets.push_back(measurePath("Exports", layout.exportsDirectory));

    for (const StorageBucket &bucket : report.buckets)
    {
        report.totalFiles += bucket.fileCount;
        report.totalBytes += bucket.byteCount;
    }

    return report;
}

CleanupResult cleanupCampaignStorage(const Campaign &campaign, const CleanupOptions &options)
{
    CleanupResult result;
    const ProjectPackageLayout::CampaignPackageLayout layout = ProjectPackageLayout::layoutForCampaign(campaign);
    if (!std::filesystem::exists(layout.rootDirectory))
        return result;

    std::vector<std::filesystem::path> directories;
    for (const std::filesystem::directory_entry &entry : std::filesystem::recursive_directory_iterator(layout.rootDirectory))
    {
        if (entry.is_directory())
        {
            directories.push_back(entry.path());
            continue;
        }

        if (options.removeZeroByteFiles && entry.is_regular_file() && entry.file_size() == 0)
        {
            result.removedPaths.push_back(entry.path());
            std::filesystem::remove(entry.path());
            ++result.removedFiles;
        }
    }

    if (options.removeEmptyDirectories)
    {
        std::sort(directories.begin(), directories.end(), [](const auto &left, const auto &right) {
            return left.generic_string().size() > right.generic_string().size();
        });

        for (const std::filesystem::path &directory : directories)
        {
            if (directoryIsEmpty(directory))
            {
                result.removedPaths.push_back(directory);
                std::filesystem::remove(directory);
                ++result.removedDirectories;
            }
        }
    }

    return result;
}
}
