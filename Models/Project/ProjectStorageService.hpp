#pragma once

#include "Models/Campaign/Campaign.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace ProjectStorage
{
struct StorageBucket
{
    std::string name;
    std::filesystem::path path;
    std::uintmax_t fileCount = 0;
    std::uintmax_t byteCount = 0;
};

struct StorageUsageReport
{
    std::vector<StorageBucket> buckets;
    std::uintmax_t totalFiles = 0;
    std::uintmax_t totalBytes = 0;
};

struct CleanupOptions
{
    bool removeEmptyDirectories = true;
    bool removeZeroByteFiles = false;
};

struct CleanupResult
{
    std::uintmax_t removedFiles = 0;
    std::uintmax_t removedDirectories = 0;
    std::uintmax_t reclaimedBytes = 0;
    std::vector<std::filesystem::path> removedPaths;
};

StorageUsageReport measureCampaignStorage(const Campaign &campaign);
CleanupResult cleanupCampaignStorage(const Campaign &campaign, const CleanupOptions &options = CleanupOptions{});
}
