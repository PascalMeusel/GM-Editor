#pragma once

#include "Models/Audio/AudioModels.hpp"
#include "Models/Campaign/Campaign.hpp"

#include <algorithm>

namespace AudioMix
{
inline float groupMultiplier(const AudioVolumeGroupsModel &groups, AudioAssetType type)
{
    switch (type)
    {
    case AudioAssetType::Music:
        return groups.music;
    case AudioAssetType::Ambience:
        return groups.ambience;
    case AudioAssetType::Effect:
        return groups.effects;
    case AudioAssetType::Voice:
        return groups.voice;
    case AudioAssetType::Other:
        return 1.0f;
    }
    return 1.0f;
}

inline float effectiveVolume(const Campaign &campaign, const AudioAssetModel &asset, float sourceVolume)
{
    return std::clamp(sourceVolume, 0.0f, 1.0f) *
           std::clamp(campaign.audioVolumeGroups.master, 0.0f, 1.0f) *
           std::clamp(groupMultiplier(campaign.audioVolumeGroups, asset.type), 0.0f, 1.0f);
}
}
