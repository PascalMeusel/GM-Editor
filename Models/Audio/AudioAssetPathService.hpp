#pragma once

#include "Models/Audio/AudioModels.hpp"
#include "Models/Campaign/Campaign.hpp"

#include <filesystem>
#include <string>

namespace AudioAssetPaths
{
std::string sanitizePathSegment(const std::string &value);
std::filesystem::path findProjectRoot();
std::filesystem::path resolveAudioPath(const AudioAssetModel &asset);
std::filesystem::path resolveOriginalSourcePath(const AudioAssetModel &asset);
std::filesystem::path campaignAudioDirectory(const Campaign &campaign);
std::string copyAudioFileToCampaign(const Campaign &campaign, const std::filesystem::path &sourcePath, const EntityId &assetId);
bool isSupportedAudioFile(const std::filesystem::path &sourcePath);
}
