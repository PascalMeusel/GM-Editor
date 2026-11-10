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

AudioPlaylistModel *AudioManagerController::getSelectedPlaylist()
{
    if (_selectedPlaylistIndex < 0 || _selectedPlaylistIndex >= static_cast<int>(_currentCampaign.audioPlaylists.size()))
        return nullptr;
    return &_currentCampaign.audioPlaylists[static_cast<size_t>(_selectedPlaylistIndex)];
}

const AudioPlaylistModel *AudioManagerController::getSelectedPlaylist() const
{
    if (_selectedPlaylistIndex < 0 || _selectedPlaylistIndex >= static_cast<int>(_currentCampaign.audioPlaylists.size()))
        return nullptr;
    return &_currentCampaign.audioPlaylists[static_cast<size_t>(_selectedPlaylistIndex)];
}

void AudioManagerController::selectPlaylist(int index)
{
    if (_currentCampaign.audioPlaylists.empty())
    {
        _selectedPlaylistIndex = -1;
        _playlistQueue.clear();
        _queuedAssetIndex = -1;
        _lastQueuedAssetId.clear();
        return;
    }

    const int selectedIndex = std::clamp(index, 0, static_cast<int>(_currentCampaign.audioPlaylists.size()) - 1);
    if (selectedIndex != _selectedPlaylistIndex)
        _lastQueuedAssetId.clear();

    _selectedPlaylistIndex = selectedIndex;
    rebuildPlaylistQueue(false);
}

void AudioManagerController::createPlaylist()
{
    if (_saveFailed) { retrySave(); return; }

    _errorMessage.clear();
    _statusMessage.clear();

    AudioPlaylistModel playlist;
    playlist.name = _newPlaylistName.empty() ? "New Playlist" : _newPlaylistName;
    playlist.audioAssetIds=_newPlaylistAssets;
    playlist.metadata.tags = {"audio"};
    playlist.metadata.touch();
    _currentCampaign.audioPlaylists.push_back(playlist);
    selectPlaylist(static_cast<int>(_currentCampaign.audioPlaylists.size()) - 1);
    if (!saveCampaign()) return;
    _statusMessage = uiText("Created playlist: ", "Wiedergabeliste erstellt: ") + playlist.name;
    _newPlaylistAssets.clear();
}

void AudioManagerController::saveSelectedPlaylist()
{
    AudioPlaylistModel *playlist = getSelectedPlaylist();
    if (!playlist)
        return;

    playlist->metadata.touch();
    if (!saveCampaign()) return;
    rebuildPlaylistQueue(false);
    _statusMessage = uiText("Saved playlist: ", "Wiedergabeliste gespeichert: ") + playlist->name;
    _errorMessage.clear();
}

void AudioManagerController::removeSelectedPlaylist()
{
    if (_saveFailed) { retrySave(); return; }

    AudioPlaylistModel *playlist = getSelectedPlaylist();
    if (!playlist)
        return;

    const std::string removedName = playlist->name;
    _currentCampaign.audioPlaylists.erase(_currentCampaign.audioPlaylists.begin() + _selectedPlaylistIndex);
    if (_currentCampaign.audioPlaylists.empty())
        selectPlaylist(-1);
    else
        selectPlaylist(std::min(_selectedPlaylistIndex, static_cast<int>(_currentCampaign.audioPlaylists.size()) - 1));

    if (!saveCampaign()) return;
    _statusMessage = uiText("Removed playlist: ", "Wiedergabeliste entfernt: ") + removedName;
    _errorMessage.clear();
}

void AudioManagerController::addSelectedAssetToPlaylist()
{
    if (_saveFailed) { retrySave(); return; }

    const AudioAssetModel *asset = getSelectedAsset();
    AudioPlaylistModel *playlist = getSelectedPlaylist();
    if (!asset || !playlist)
        return;

    playlist->audioAssetIds.push_back(asset->metadata.id);
    playlist->metadata.touch();
    rebuildPlaylistQueue(false);
    if (!saveCampaign()) return;
    _statusMessage = uiText("Added ", "Hinzugefügt: ") + asset->name + uiText(" to ", " zu ") + playlist->name;
    _errorMessage.clear();
}

void AudioManagerController::removeAssetFromSelectedPlaylist(int index)
{
    if (_saveFailed) { retrySave(); return; }

    AudioPlaylistModel *playlist = getSelectedPlaylist();
    if (!playlist || index < 0 || index >= static_cast<int>(playlist->audioAssetIds.size()))
        return;

    playlist->audioAssetIds.erase(playlist->audioAssetIds.begin() + index);
    playlist->metadata.touch();
    rebuildPlaylistQueue(false);
    if (!saveCampaign()) return;
    _statusMessage = uiText("Removed playlist item.", "Eintrag aus der Wiedergabeliste entfernt.");
    _errorMessage.clear();
}

void AudioManagerController::moveSelectedPlaylistAsset(int index, int direction)
{
    if (_saveFailed) { retrySave(); return; }

    AudioPlaylistModel *playlist = getSelectedPlaylist();
    if (!playlist)
        return;

    const int targetIndex = index + direction;
    if (index < 0 || targetIndex < 0 || index >= static_cast<int>(playlist->audioAssetIds.size()) || targetIndex >= static_cast<int>(playlist->audioAssetIds.size()))
        return;

    std::swap(playlist->audioAssetIds[static_cast<size_t>(index)], playlist->audioAssetIds[static_cast<size_t>(targetIndex)]);
    playlist->metadata.touch();
    rebuildPlaylistQueue(false);
    if (!saveCampaign()) return;
}

void AudioManagerController::playSelectedPlaylist()
{
    rebuildPlaylistQueue(false);
    playQueuedAssetAt(0, false);
}

void AudioManagerController::playSelectedPlaylistOneShot()
{
    rebuildPlaylistQueue(true);
    playQueuedAssetAt(0, true);
}

void AudioManagerController::playPreviousQueuedAsset()
{
    const AudioPlaylistModel *playlist = getSelectedPlaylist();
    if (!playlist)
        return;

    if (_playlistQueue.empty())
        rebuildPlaylistQueue(false);

    int previousIndex = _queuedAssetIndex - 1;
    if (_queuedAssetIndex < 0 && !_lastQueuedAssetId.empty())
    {
        const auto lastIt = std::find(_playlistQueue.begin(), _playlistQueue.end(), _lastQueuedAssetId);
        if (lastIt != _playlistQueue.end())
            previousIndex = static_cast<int>(std::distance(_playlistQueue.begin(), lastIt)) - 1;
    }

    if (previousIndex < 0)
    {
        if (!playlist->repeat || _playlistQueue.empty())
        {
            _statusMessage = uiText("Queue is at the start: ", "Warteschlange ist am Anfang: ") + playlist->name;
            return;
        }
        previousIndex = static_cast<int>(_playlistQueue.size()) - 1;
    }

    playQueuedAssetAt(previousIndex, playlist->oneShot);
}

void AudioManagerController::playNextQueuedAsset()
{
    const AudioPlaylistModel *playlist = getSelectedPlaylist();
    if (!playlist)
        return;

    if (_playlistQueue.empty())
        rebuildPlaylistQueue(false);

    int nextIndex = _queuedAssetIndex + 1;
    if (nextIndex >= static_cast<int>(_playlistQueue.size()))
    {
        if (!playlist->repeat)
        {
            _statusMessage = uiText("Playlist queue ended: ", "Wiedergabeliste beendet: ") + playlist->name;
            _queuedAssetIndex = -1;
            _playbackService->clearPlaylistPlaybackState();
            return;
        }
        rebuildPlaylistQueue(false);
        nextIndex = 0;
    }

    playQueuedAssetAt(nextIndex, playlist->oneShot);
}

void AudioManagerController::stopPlaylistQueue()
{
    for (const EntityId &assetId : _playlistQueue)
    {
        std::string ignored;
        if (_playbackService->isActive(assetId))
            _playbackService->stop(assetId, ignored);
    }
    _queuedAssetIndex = -1;
    _playbackService->clearPlaylistPlaybackState();
    _statusMessage = uiText("Stopped playlist queue.", "Wiedergabeliste gestoppt.");
    _errorMessage.clear();
}
