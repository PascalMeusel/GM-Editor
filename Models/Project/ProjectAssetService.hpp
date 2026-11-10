#pragma once

#include "Models/Audio/AudioModels.hpp"
#include "Models/Campaign/Campaign.hpp"

#include <filesystem>
#include <string>

namespace ProjectAssets
{
std::string sanitizePathSegment(const std::string &value);
std::filesystem::path findProjectRoot();
std::filesystem::path resolveProjectPath(const std::string &storedPath);
std::filesystem::path campaignAssetDirectory(const Campaign &campaign, const std::string &assetFolder);
std::string copyFileToCampaignAssetFolder(const Campaign &campaign, const std::filesystem::path &sourcePath, const EntityId &assetId, const std::string &assetFolder, const std::string &fallbackExtension);

bool isSupportedAudioFile(const std::filesystem::path &sourcePath);
std::filesystem::path campaignAudioDirectory(const Campaign &campaign);
std::string copyAudioFileToCampaign(const Campaign &campaign, const std::filesystem::path &sourcePath, const EntityId &assetId);
std::filesystem::path resolveAudioPath(const AudioAssetModel &asset);
std::filesystem::path resolveOriginalSourcePath(const AudioAssetModel &asset);
bool repairAudioAssetFromSource(Campaign &campaign, AudioAssetModel &asset, const std::filesystem::path &sourcePath, std::string &errorMessage);
}
