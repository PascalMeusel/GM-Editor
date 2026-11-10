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

void AudioManagerController::loadBundledCatalog()
{
    if (_catalogLoaded)
        return;

    _catalogLoaded = true;
    std::string catalogError;
    // Bundled resources live beside the application (or in the development
    // checkout), while imported campaign files live in writable user storage.
    const auto manifest = ProjectAssets::resolveProjectPath("Resources/Audio/catalog.psv");
    _catalogEntries = loadBundledAudioCatalog(manifest.parent_path().parent_path().parent_path(), catalogError);
    if (!catalogError.empty() && _errorMessage.empty())
        _errorMessage = catalogError;
}

bool AudioManagerController::catalogEntryMatchesFilter(const AudioCatalogEntry &entry) const
{
    if (_catalogSearch.empty())
        return true;

    const std::string needle = lowerCopy(_catalogSearch);
    const std::string haystack = lowerCopy(entry.name + " " + entry.theme + " " + audioAssetTypeLabel(entry.type) + " " + entry.sourceTitle + " " + entry.author);
    return haystack.find(needle) != std::string::npos;
}

bool AudioManagerController::ensureSelectedAssetFileAvailable(AudioAssetModel &asset)
{
    if (std::filesystem::exists(resolveAudioPath(asset)))
        return true;

    if (!asset.originalSourcePath.empty())
    {
        std::filesystem::path sourcePath = ProjectAssets::resolveOriginalSourcePath(asset);
        if (std::filesystem::exists(sourcePath) && std::filesystem::is_regular_file(sourcePath))
            return repairSelectedAssetFromPath(asset, sourcePath);
    }

    _errorMessage = uiText("Audio file is missing. Use Relink Missing File in the Audio Controller to restore it.",
                           "Die Audiodatei fehlt. Verwende »Fehlende Datei neu verknüpfen« in der Audiosteuerung, um sie wiederherzustellen.");
    return false;
}

bool AudioManagerController::repairSelectedAssetFromPath(AudioAssetModel &asset, const std::filesystem::path &sourcePath)
{
    _errorMessage.clear();
    _statusMessage.clear();

    if (ProjectAssets::repairAudioAssetFromSource(_currentCampaign, asset, sourcePath, _errorMessage))
    {
        if (!saveCampaign()) return false;
        _statusMessage = uiText("Repaired audio file: ", "Audiodatei repariert: ") + asset.name;
        return true;
    }

    return false;
}

void AudioManagerController::rebuildPlaylistQueue(bool oneShotOverride)
{
    const AudioPlaylistModel *playlist = getSelectedPlaylist();
    _playlistQueue.clear();
    _queuedAssetIndex = -1;
    if (!playlist)
        return;

    for (const EntityId &assetId : playlist->audioAssetIds)
    {
        if (findAudioAssetById(assetId))
            _playlistQueue.push_back(assetId);
    }

    if (playlist->shuffle && _playlistQueue.size() > 1)
    {
        std::random_device device;
        std::mt19937 generator(device());
        std::shuffle(_playlistQueue.begin(), _playlistQueue.end(), generator);
    }

    if (oneShotOverride && _playlistQueue.size() > 1)
        _playlistQueue.resize(1);

    if (!_lastQueuedAssetId.empty() && std::find(_playlistQueue.begin(), _playlistQueue.end(), _lastQueuedAssetId) == _playlistQueue.end())
        _lastQueuedAssetId.clear();
}

void AudioManagerController::publishPlaylistPlaybackState(const AudioPlaylistModel &playlist, bool transitionActive, const std::string &transitionLabel, bool oneShotActive)
{
    AudioPlaylistPlaybackState state;
    state.playlistId = playlist.metadata.id;
    state.playlistName = playlist.name.empty() ? "Untitled playlist" : playlist.name;
    state.queueAssetIds = _playlistQueue;
    state.currentIndex = _queuedAssetIndex;
    state.shuffle = playlist.shuffle;
    state.repeat = playlist.repeat;
    state.oneShot = playlist.oneShot || oneShotActive;
    state.transitionActive = transitionActive;
    state.transitionLabel = transitionLabel;

    const AudioAssetModel *current = getCurrentQueuedAsset();
    const AudioAssetModel *previous = getPreviousQueuedAsset();
    const AudioAssetModel *last = getLastQueuedAsset();
    const AudioAssetModel *next = getNextQueuedAsset();
    if (current)
        state.currentAssetId = current->metadata.id;
    if (previous)
        state.previousAssetId = previous->metadata.id;
    if (last)
        state.lastAssetId = last->metadata.id;
    if (next)
        state.nextAssetId = next->metadata.id;

    _playbackService->setPlaylistPlaybackState(state);
}

bool AudioManagerController::playQueuedAssetAt(int queueIndex, bool oneShotOverride)
{
    const AudioPlaylistModel *playlist = getSelectedPlaylist();
    if (!playlist)
        return false;

    if (_playlistQueue.empty())
    {
        _errorMessage = uiText("Playlist has no playable audio assets.", "Die Wiedergabeliste enthält keine abspielbaren Audiodateien.");
        return false;
    }

    if (queueIndex < 0 || queueIndex >= static_cast<int>(_playlistQueue.size()))
        return false;

    AudioAssetModel *asset = findAudioAssetById(_playlistQueue[static_cast<size_t>(queueIndex)]);
    if (!asset)
    {
        _errorMessage = uiText("Playlist item is missing from campaign audio.", "Der Eintrag der Wiedergabeliste fehlt im Kampagnenaudio.");
        return false;
    }

    _errorMessage.clear();
    if (!ensureSelectedAssetFileAvailable(*asset))
        return false;
    if (!saveCampaign()) return false;

    const bool playAsOneShot = oneShotOverride || playlist->oneShot || asset->type == AudioAssetType::Effect;
    if (_playbackService->play(*asset, resolveAudioPath(*asset), AudioMix::effectiveVolume(_currentCampaign, *asset, asset->defaultVolume), playAsOneShot ? false : asset->loop, _errorMessage))
    {
        _queuedAssetIndex = queueIndex;
        _lastQueuedAssetId = asset->metadata.id;
        publishPlaylistPlaybackState(*playlist, false, oneShotOverride ? "One-shot queue playback" : "Queue playback", playAsOneShot);
        ProjectSaveService::getInstance().markDirty("Audio playback history updated");
        _statusMessage = uiText("Playlist ", "Wiedergabeliste ") + playlist->name + ": " + asset->name;
        return true;
    }

    return false;
}

bool AudioManagerController::saveCampaign()
{
    _currentCampaign.metadata.touch();
    auto &service = ProjectSaveService::getInstance();
    _saveFailed = !service.saveCampaign(_currentCampaign, "audio edits");
    if (_saveFailed)
    {
        _saveFailureCampaignId = _currentCampaign.metadata.id;
        _statusMessage.clear();
        _saveErrorMessage = uiText("Not saved. Your audio changes remain here. Retry Save before closing. ",
                                   "Nicht gespeichert. Deine Audioänderungen bleiben hier. Vor dem Schließen erneut speichern. ") + service.state().lastError;
        return false;
    }
    _saveErrorMessage.clear();
    if (_importPendingSave)
    {
        _importSourcePath.clear();
        _importName.clear();
        _importPendingSave = false;
    }
    return true;
}

void AudioManagerController::retrySave()
{
    if (saveCampaign())
    {
        _statusMessage = uiText("Audio changes saved.", "Audioänderungen gespeichert.");
        // Retry persists the retained asset/playlist identities; it does not
        // repeat the mutation or start playback.
    }
}

void AudioManagerController::syncSelectedControlsFromAsset()
{
    const AudioAssetModel *asset = getSelectedAsset();
    if (!asset)
        return;
    _selectedVolume = std::clamp(asset->defaultVolume, 0.0f, 1.0f);
    _selectedLoop = asset->loop;
}

void AudioManagerController::onShow()
{
    configure();
    _view->show();
    _view->requestDockOnNextUpdate();
}
