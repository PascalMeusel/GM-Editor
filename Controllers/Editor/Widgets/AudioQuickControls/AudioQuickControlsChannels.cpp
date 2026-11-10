#include "AudioQuickControlsController.hpp"

#include "Models/Audio/AudioMixService.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include "Models/Project/ProjectSaveService.hpp"

#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

namespace
{
std::string uiText(const char *english, const char *german)
{
    return gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German ? german : english;
}

std::string audioDisplayName(const AudioAssetModel &asset)
{
    if (!asset.name.empty())
        return asset.name;
    if (!asset.originalFileName.empty())
        return asset.originalFileName;
    return gm::localization::trUi("Untitled audio");
}

void appendChannelName(std::string &summary, const std::string &name)
{
    if (summary.empty())
        summary = name;
    else
        summary += ", " + name;
}

std::string summaryText(int count, const std::string &names)
{
    if (count <= 0)
        return gm::localization::trUi("None");
    return std::to_string(count) + ": " + names;
}
}

void AudioQuickControlsController::pauseAllForBreak()
{
    const AudioPlaybackBatchResult result = _playbackService->pausePlaying(_errorMessage);

    if (result.changed > 0)
        _statusMessage = gm::localization::LocalizationService::instance().format(
            gm::localization::TextId::LiveAudioPausedForBreak, {std::to_string(result.changed)});
    else if (result.considered == 0)
        _statusMessage = gm::localization::trUi("No active audio channels to pause for break.");
    else
        _statusMessage = gm::localization::trUi("All active audio channels are already paused.");
}

void AudioQuickControlsController::resumePausedAudio()
{
    if (!saveCampaign())
        return;
    const AudioPlaybackBatchResult result = _playbackService->resumePaused(_errorMessage);

    if (result.changed > 0)
        _statusMessage = gm::localization::LocalizationService::instance().format(
            gm::localization::TextId::LiveAudioResumedAfterBreak, {std::to_string(result.changed)});
    else if (result.considered == 0)
        _statusMessage = gm::localization::trUi("No active audio channels to resume after break.");
    else
        _statusMessage = gm::localization::trUi("No paused audio channels to resume.");
}

void AudioQuickControlsController::pauseChannel(const EntityId &channelId)
{
    if (!_playbackService->isActive(channelId))
    {
        _statusMessage = gm::localization::trUi("Audio channel is not active.");
        return;
    }

    _errorMessage.clear();
    if (_playbackService->pause(channelId, _errorMessage))
        _statusMessage = gm::localization::LocalizationService::instance().format(
            gm::localization::TextId::LivePausedAsset, {channelDisplayName(channelId)});
}

void AudioQuickControlsController::resumeChannel(const EntityId &channelId)
{
    if (!_playbackService->isActive(channelId))
    {
        _statusMessage = gm::localization::trUi("Audio channel is not active.");
        return;
    }
    if (!_playbackService->isPaused(channelId))
    {
        _statusMessage = gm::localization::trUi("Audio channel is already playing.");
        return;
    }

    _errorMessage.clear();
    if (!saveCampaign())
        return;
    if (_playbackService->resume(channelId, _errorMessage))
        _statusMessage = gm::localization::LocalizationService::instance().format(
            gm::localization::TextId::LiveResumedAsset, {channelDisplayName(channelId)});
}

void AudioQuickControlsController::stopChannel(const EntityId &channelId)
{
    if (!_playbackService->isActive(channelId))
    {
        _statusMessage = gm::localization::trUi("Audio channel is not active.");
        return;
    }

    _errorMessage.clear();
    if (_playbackService->stop(channelId, _errorMessage))
        _statusMessage = gm::localization::LocalizationService::instance().format(
            gm::localization::TextId::LiveStoppedAsset, {channelDisplayName(channelId)});
}

void AudioQuickControlsController::applySelectedVolume()
{
    const AudioAssetModel *asset = getSelectedAsset();
    if (!asset || !_playbackService->isActive(asset->metadata.id))
        return;

    _errorMessage.clear();
    _playbackService->setVolume(asset->metadata.id, AudioMix::effectiveVolume(_currentCampaign, *asset, _volume), _errorMessage);
}

bool AudioQuickControlsController::isSelectedAssetActive() const
{
    const AudioAssetModel *asset = getSelectedAsset();
    return asset && _playbackService->isActive(asset->metadata.id);
}

bool AudioQuickControlsController::isSelectedAssetPaused() const
{
    const AudioAssetModel *asset = getSelectedAsset();
    return asset && _playbackService->isPaused(asset->metadata.id);
}

const AudioAssetModel *AudioQuickControlsController::findAudioAssetById(const EntityId &assetId) const
{
    const auto found = std::find_if(_currentCampaign.audioAssets.begin(), _currentCampaign.audioAssets.end(), [&](const AudioAssetModel &asset) {
        return asset.metadata.id == assetId;
    });
    return found == _currentCampaign.audioAssets.end() ? nullptr : &(*found);
}

int AudioQuickControlsController::firstAssetIndexOfType(AudioAssetType type) const
{
    const auto found = std::find_if(_currentCampaign.audioAssets.begin(), _currentCampaign.audioAssets.end(), [&](const AudioAssetModel &asset) {
        return asset.type == type;
    });
    return found == _currentCampaign.audioAssets.end() ? -1 : static_cast<int>(std::distance(_currentCampaign.audioAssets.begin(), found));
}

const AudioAmbienceLayerModel *AudioQuickControlsController::findAmbienceLayerById(const EntityId &layerId) const
{
    const auto found = std::find_if(_currentCampaign.audioAmbienceLayers.begin(), _currentCampaign.audioAmbienceLayers.end(), [&](const AudioAmbienceLayerModel &layer) {
        return layer.metadata.id == layerId;
    });
    return found == _currentCampaign.audioAmbienceLayers.end() ? nullptr : &(*found);
}

std::string AudioQuickControlsController::channelDisplayName(const EntityId &channelId) const
{
    if (const AudioAssetModel *asset = findAudioAssetById(channelId))
        return audioDisplayName(*asset);
    if (const AudioAmbienceLayerModel *layer = findAmbienceLayerById(channelId))
        return layer->name.empty() ? std::string(gm::localization::trUi("Ambience layer")) : layer->name;
    return channelId;
}
