#include "AudioTriggerService.hpp"

#include "Models/Audio/AudioMixService.hpp"
#include "Models/Project/ProjectAssetService.hpp"

#include <algorithm>

namespace
{
AudioAssetModel *findAudioAsset(Campaign &campaign, const EntityId &assetId)
{
    auto found = std::find_if(campaign.audioAssets.begin(), campaign.audioAssets.end(), [&](const AudioAssetModel &asset) {
        return asset.metadata.id == assetId;
    });
    return found == campaign.audioAssets.end() ? nullptr : &(*found);
}
}

namespace AudioTriggers
{
int playMatchingTriggers(Campaign &campaign,
                         const std::shared_ptr<AudioPlaybackService> &playbackService,
                         const std::string &sourceType,
                         const EntityId &sourceEntityId,
                         const std::string &eventName,
                         std::string &errorMessage)
{
    if (!playbackService)
        return 0;

    int played = 0;
    for (AudioTriggerModel &trigger : campaign.audioTriggers)
    {
        if (!trigger.enabled || trigger.sourceType != sourceType || trigger.eventName != eventName)
            continue;
        if (!trigger.sourceEntityId.empty() && trigger.sourceEntityId != sourceEntityId)
            continue;

        AudioAssetModel *asset = findAudioAsset(campaign, trigger.audioAssetId);
        if (!asset)
        {
            errorMessage = "Audio trigger asset is missing: " + trigger.name;
            continue;
        }

        if (playbackService->play(*asset, ProjectAssets::resolveAudioPath(*asset), AudioMix::effectiveVolume(campaign, *asset, trigger.volume), trigger.loop, errorMessage))
        {
            trigger.metadata.touch();
            ++played;
        }
    }

    return played;
}
}
