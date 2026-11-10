#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#endif

#include "AudioManagerController.hpp"

#include "Models/Audio/AudioMixService.hpp"
#include "Models/Audio/AudioTriggerService.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Project/ProjectAssetImportService.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include "Models/Project/ProjectEventBus.hpp"
#include "Models/Project/ProjectLogger.hpp"
#include "Models/Project/ProjectSaveService.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <iterator>
#include <random>
#include <utility>

namespace
{
std::string uiText(const char *english, const char *german)
{
    return gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German ? german : english;
}

std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}
}

void AudioManagerController::sceneTransitionToSelectedPlaylist()
{
    const AudioPlaylistModel *playlist = getSelectedPlaylist();
    if (!playlist)
        return;

    rebuildPlaylistQueue(false);
    if (_playlistQueue.empty())
    {
        _errorMessage = uiText("Playlist has no playable audio assets.", "Die Wiedergabeliste enthält keine abspielbaren Audiodateien.");
        return;
    }

    AudioAssetModel *nextAsset = findAudioAssetById(_playlistQueue.front());
    if (!nextAsset)
    {
        _errorMessage = uiText("Playlist item is missing from campaign audio.", "Der Eintrag der Wiedergabeliste fehlt im Kampagnenaudio.");
        return;
    }

    _errorMessage.clear();
    if (!ensureSelectedAssetFileAvailable(*nextAsset))
        return;
    if (!saveCampaign()) return;

    const std::vector<EntityId> activeIds = _playbackService->activeAssetIds();
    const bool playAsOneShot = playlist->oneShot || nextAsset->type == AudioAssetType::Effect;
    bool transitioned = false;
    for (const EntityId &activeId : activeIds)
    {
        if (activeId == nextAsset->metadata.id)
            continue;

        transitioned = _playbackService->crossfade(activeId, *nextAsset, resolveAudioPath(*nextAsset), AudioMix::effectiveVolume(_currentCampaign, *nextAsset, nextAsset->defaultVolume), playAsOneShot ? false : nextAsset->loop, _fadeSeconds, _errorMessage);
        break;
    }

    if (!transitioned)
        transitioned = _playbackService->fadeIn(*nextAsset, resolveAudioPath(*nextAsset), AudioMix::effectiveVolume(_currentCampaign, *nextAsset, nextAsset->defaultVolume), playAsOneShot ? false : nextAsset->loop, _fadeSeconds, _errorMessage);

    if (!transitioned)
        return;

    for (const EntityId &activeId : activeIds)
    {
        std::string ignored;
        if (activeId != nextAsset->metadata.id && _playbackService->isActive(activeId) && !_playbackService->isFading(activeId))
            _playbackService->stop(activeId, ignored);
    }

    _queuedAssetIndex = 0;
    _lastQueuedAssetId = nextAsset->metadata.id;
    publishPlaylistPlaybackState(*playlist, true, "Scene transition / crossfade", playAsOneShot);
    ProjectSaveService::getInstance().markDirty("Audio playback history updated");
    _statusMessage = uiText("Scene transition: ", "Szenenübergang: ") + playlist->name + " -> " + nextAsset->name;
}

const AudioAssetModel *AudioManagerController::getCurrentQueuedAsset() const
{
    if (_queuedAssetIndex >= 0 && _queuedAssetIndex < static_cast<int>(_playlistQueue.size()))
        return findAudioAssetById(_playlistQueue[static_cast<size_t>(_queuedAssetIndex)]);

    if (!_lastQueuedAssetId.empty() && _playbackService->isActive(_lastQueuedAssetId))
        return findAudioAssetById(_lastQueuedAssetId);

    return nullptr;
}

const AudioAssetModel *AudioManagerController::getPreviousQueuedAsset() const
{
    const AudioPlaylistModel *playlist = getSelectedPlaylist();
    if (!playlist || _playlistQueue.empty())
        return nullptr;

    int previousIndex = _queuedAssetIndex - 1;
    if (_queuedAssetIndex < 0 && !_lastQueuedAssetId.empty())
    {
        const auto lastIt = std::find(_playlistQueue.begin(), _playlistQueue.end(), _lastQueuedAssetId);
        if (lastIt != _playlistQueue.end())
            previousIndex = static_cast<int>(std::distance(_playlistQueue.begin(), lastIt)) - 1;
    }

    if (previousIndex < 0)
    {
        if (!playlist->repeat)
            return nullptr;
        previousIndex = static_cast<int>(_playlistQueue.size()) - 1;
    }

    return findAudioAssetById(_playlistQueue[static_cast<size_t>(previousIndex)]);
}

const AudioAssetModel *AudioManagerController::getNextQueuedAsset() const
{
    const AudioPlaylistModel *playlist = getSelectedPlaylist();
    if (!playlist || _playlistQueue.empty())
        return nullptr;

    int nextIndex = _queuedAssetIndex + 1;
    if (_queuedAssetIndex < 0 && !_lastQueuedAssetId.empty())
    {
        const auto lastIt = std::find(_playlistQueue.begin(), _playlistQueue.end(), _lastQueuedAssetId);
        if (lastIt != _playlistQueue.end())
            nextIndex = static_cast<int>(std::distance(_playlistQueue.begin(), lastIt)) + 1;
    }

    if (nextIndex >= static_cast<int>(_playlistQueue.size()))
    {
        if (!playlist->repeat)
            return nullptr;
        nextIndex = 0;
    }

    return findAudioAssetById(_playlistQueue[static_cast<size_t>(nextIndex)]);
}

const AudioAssetModel *AudioManagerController::getLastQueuedAsset() const
{
    return _lastQueuedAssetId.empty() ? nullptr : findAudioAssetById(_lastQueuedAssetId);
}

int AudioManagerController::activePlaylistQueueCount() const
{
    int activeCount = 0;
    for (const EntityId &assetId : _playlistQueue)
    {
        if (_playbackService->isActive(assetId))
            ++activeCount;
    }
    return activeCount;
}

const AudioAssetModel *AudioManagerController::findAudioAssetById(const EntityId &assetId) const
{
    const auto it = std::find_if(_currentCampaign.audioAssets.begin(), _currentCampaign.audioAssets.end(), [&](const AudioAssetModel &asset) {
        return asset.metadata.id == assetId;
    });
    return it == _currentCampaign.audioAssets.end() ? nullptr : &(*it);
}

AudioAssetModel *AudioManagerController::findAudioAssetById(const EntityId &assetId)
{
    auto it = std::find_if(_currentCampaign.audioAssets.begin(), _currentCampaign.audioAssets.end(), [&](const AudioAssetModel &asset) {
        return asset.metadata.id == assetId;
    });
    return it == _currentCampaign.audioAssets.end() ? nullptr : &(*it);
}

const AudioAmbienceLayerModel *AudioManagerController::findAmbienceLayerById(const EntityId &layerId) const
{
    const auto it = std::find_if(_currentCampaign.audioAmbienceLayers.begin(), _currentCampaign.audioAmbienceLayers.end(), [&](const AudioAmbienceLayerModel &layer) {
        return layer.metadata.id == layerId;
    });
    return it == _currentCampaign.audioAmbienceLayers.end() ? nullptr : &(*it);
}
