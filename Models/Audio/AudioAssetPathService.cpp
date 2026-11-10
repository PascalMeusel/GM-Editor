#include "AudioAssetPathService.hpp"

#include "Models/Project/ProjectAssetService.hpp"

namespace AudioAssetPaths
{
std::string sanitizePathSegment(const std::string &value)
{
    return ProjectAssets::sanitizePathSegment(value);
}

std::filesystem::path findProjectRoot()
{
    return ProjectAssets::findProjectRoot();
}

std::filesystem::path resolveAudioPath(const AudioAssetModel &asset)
{
    return ProjectAssets::resolveAudioPath(asset);
}

std::filesystem::path resolveOriginalSourcePath(const AudioAssetModel &asset)
{
    return ProjectAssets::resolveOriginalSourcePath(asset);
}

std::filesystem::path campaignAudioDirectory(const Campaign &campaign)
{
    return ProjectAssets::campaignAudioDirectory(campaign);
}

std::string copyAudioFileToCampaign(const Campaign &campaign, const std::filesystem::path &sourcePath, const EntityId &assetId)
{
    return ProjectAssets::copyAudioFileToCampaign(campaign, sourcePath, assetId);
}

bool isSupportedAudioFile(const std::filesystem::path &sourcePath)
{
    return ProjectAssets::isSupportedAudioFile(sourcePath);
}
}
