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

void AudioManagerController::playSelectedAsset()
{
    AudioAssetModel *asset = getSelectedAsset();
    if (!asset)
        return;

    _errorMessage.clear();
    if (!ensureSelectedAssetFileAvailable(*asset))
        return;
    if (!saveCampaign()) return;

    if (_playbackService->play(*asset, resolveAudioPath(*asset), AudioMix::effectiveVolume(_currentCampaign, *asset, _selectedVolume), _selectedLoop, _errorMessage))
    {
        ProjectSaveService::getInstance().markDirty("Audio playback history updated");
        _statusMessage = uiText("Playing: ", "Wiedergabe: ") + asset->name;
    }
}

void AudioManagerController::fadeInSelectedAsset()
{
    AudioAssetModel *asset = getSelectedAsset();
    if (!asset)
        return;

    _errorMessage.clear();
    if (!ensureSelectedAssetFileAvailable(*asset))
        return;
    if (!saveCampaign()) return;

    if (_playbackService->fadeIn(*asset, resolveAudioPath(*asset), AudioMix::effectiveVolume(_currentCampaign, *asset, _selectedVolume), _selectedLoop, _fadeSeconds, _errorMessage))
    {
        ProjectSaveService::getInstance().markDirty("Audio playback history updated");
        _statusMessage = uiText("Fading in: ", "Einblenden: ") + asset->name;
    }
}

void AudioManagerController::fadeOutSelectedAsset()
{
    const AudioAssetModel *asset = getSelectedAsset();
    if (!asset || !_playbackService->isActive(asset->metadata.id))
        return;

    _errorMessage.clear();
    if (_playbackService->fadeOut(asset->metadata.id, _fadeSeconds, true, _errorMessage))
        _statusMessage = uiText("Fading out: ", "Ausblenden: ") + asset->name;
}

void AudioManagerController::crossfadeToSelectedAsset()
{
    AudioAssetModel *asset = getSelectedAsset();
    if (!asset)
        return;

    _errorMessage.clear();
    if (!ensureSelectedAssetFileAvailable(*asset))
        return;
    if (!saveCampaign()) return;

    EntityId fromAssetId;
    for (const EntityId &activeId : _playbackService->activeAssetIds())
    {
        if (activeId != asset->metadata.id)
        {
            fromAssetId = activeId;
            break;
        }
    }

    if (fromAssetId.empty())
    {
        fadeInSelectedAsset();
        return;
    }

    if (_playbackService->crossfade(fromAssetId, *asset, resolveAudioPath(*asset), AudioMix::effectiveVolume(_currentCampaign, *asset, _selectedVolume), _selectedLoop, _fadeSeconds, _errorMessage))
    {
        ProjectSaveService::getInstance().markDirty("Audio playback history updated");
        _statusMessage = uiText("Crossfading to: ", "Überblenden zu: ") + asset->name;
    }
}

void AudioManagerController::pauseActiveAsset()
{
    const AudioAssetModel *asset = getSelectedAsset();
    if (!asset || !_playbackService->isActive(asset->metadata.id))
        return;

    _errorMessage.clear();
    if (_playbackService->pause(asset->metadata.id, _errorMessage))
    {
        _statusMessage = uiText("Paused: ", "Pausiert: ") + asset->name;
    }
}

void AudioManagerController::resumeActiveAsset()
{
    const AudioAssetModel *asset = getSelectedAsset();
    if (!asset || !_playbackService->isActive(asset->metadata.id))
        return;

    _errorMessage.clear();
    if (_playbackService->resume(asset->metadata.id, _errorMessage))
    {
        _statusMessage = uiText("Resumed: ", "Fortgesetzt: ") + asset->name;
    }
}

void AudioManagerController::stopActiveAsset()
{
    const AudioAssetModel *asset = getSelectedAsset();
    if (!asset || !_playbackService->isActive(asset->metadata.id))
        return;

    _errorMessage.clear();
    if (_playbackService->stop(asset->metadata.id, _errorMessage))
    {
        _statusMessage = uiText("Stopped: ", "Gestoppt: ") + asset->name;
    }
}

void AudioManagerController::stopActiveChannel(const EntityId &assetId)
{
    if (assetId.empty() || !_playbackService->isActive(assetId))
        return;

    _errorMessage.clear();
    if (!_playbackService->stop(assetId, _errorMessage))
        return;

    for (AudioAmbienceLayerModel &layer : _currentCampaign.audioAmbienceLayers)
    {
        if (layer.metadata.id != assetId)
            continue;
        layer.active = false;
        layer.metadata.touch();
        saveCampaign();
        _statusMessage = uiText("Stopped ambience layer: ", "Atmosphärenebene gestoppt: ") + layer.name;
        return;
    }

    const AudioAssetModel *asset = findAudioAssetById(assetId);
    _statusMessage = asset ? uiText("Stopped: ", "Gestoppt: ") + asset->name
                           : uiText("Stopped audio channel.", "Audiokanal gestoppt.");
}

void AudioManagerController::stopAllActiveAudio()
{
    const std::vector<EntityId> activeIds = _playbackService->activeAssetIds();
    if (activeIds.empty())
        return;

    _playbackService->stopAll();
    bool ambienceChanged = false;
    for (AudioAmbienceLayerModel &layer : _currentCampaign.audioAmbienceLayers)
    {
        if (std::find(activeIds.begin(), activeIds.end(), layer.metadata.id) == activeIds.end())
            continue;
        layer.active = false;
        layer.metadata.touch();
        ambienceChanged = true;
    }
    if (ambienceChanged)
        saveCampaign();
    _errorMessage.clear();
    _statusMessage = uiText("Stopped all audio channels.", "Alle Audiokanäle gestoppt.");
}

void AudioManagerController::applySelectedVolume()
{
    const AudioAssetModel *asset = getSelectedAsset();
    if (!asset || !_playbackService->isActive(asset->metadata.id))
        return;

    _errorMessage.clear();
    _playbackService->setVolume(asset->metadata.id, AudioMix::effectiveVolume(_currentCampaign, *asset, _selectedVolume), _errorMessage);
}

void AudioManagerController::addCatalogAsset(int index)
{
    if (_saveFailed) { retrySave(); return; }

    _errorMessage.clear();
    _statusMessage.clear();

    if (index < 0 || index >= static_cast<int>(_catalogEntries.size()))
        return;

    const AudioCatalogEntry &entry = _catalogEntries[static_cast<size_t>(index)];
    const std::string catalogSourcePath = entry.relativePath.generic_string();
    if (catalogAssetAlreadyAdded(index))
    {
        const auto existing = std::find_if(_currentCampaign.audioAssets.begin(), _currentCampaign.audioAssets.end(), [&](const AudioAssetModel &asset) {
            return lowerCopy(asset.originalSourcePath) == lowerCopy(catalogSourcePath);
        });
        if (existing != _currentCampaign.audioAssets.end())
        {
            selectAsset(static_cast<int>(std::distance(_currentCampaign.audioAssets.begin(), existing)));
            _statusMessage = uiText("Bundled audio is already in this campaign: ", "Mitgeliefertes Audio ist bereits in dieser Kampagne: ") + existing->name;
        }
        return;
    }

    const std::filesystem::path sourcePath = ProjectAssets::resolveProjectPath(entry.relativePath.generic_string());
    AudioAssetModel asset;
    ProjectAssetImport::AssetImportRequest request;
    request.kind = ProjectAssetImport::AssetKind::Audio;
    request.sourcePath = sourcePath;
    request.assetId = asset.metadata.id;
    request.displayName = entry.name;
    request.sourceReference = catalogSourcePath;
    const ProjectAssetImport::AssetImportResult importedAsset = ProjectAssetImport::importAsset(_currentCampaign, request);
    if (!importedAsset.success)
    {
        _errorMessage = importedAsset.errorMessage;
        ProjectLogger::getInstance().warning("Bundled Audio Import", _errorMessage);
        return;
    }

    asset.name = importedAsset.displayName;
    asset.originalFileName = importedAsset.originalFileName;
    asset.originalSourcePath = importedAsset.originalSourcePath;
    asset.type = entry.type;
    asset.defaultVolume = std::clamp(entry.defaultVolume, 0.0f, 1.0f);
    asset.loop = entry.loop;
    asset.metadata.tags = {"bundled-audio", lowerCopy(entry.theme)};
    asset.filePath = importedAsset.projectPath;

    asset.metadata.touch();
    _currentCampaign.audioAssets.push_back(asset);
    selectAsset(static_cast<int>(_currentCampaign.audioAssets.size()) - 1);
    if (!saveCampaign()) return;
    _statusMessage = uiText("Added bundled audio: ", "Mitgeliefertes Audio hinzugefügt: ") + asset.name;
    ProjectEventBus::getInstance().publish(ProjectEvent{ProjectEventType::AudioChanged,
                                                        _currentCampaign.metadata.id,
                                                        asset.metadata.id,
                                                        "audio",
                                                        _statusMessage});
}

bool AudioManagerController::catalogAssetAlreadyAdded(int index) const
{
    if (index < 0 || index >= static_cast<int>(_catalogEntries.size()))
        return false;
    const std::string catalogSourcePath = lowerCopy(_catalogEntries[static_cast<size_t>(index)].relativePath.generic_string());
    return std::any_of(_currentCampaign.audioAssets.begin(), _currentCampaign.audioAssets.end(), [&](const AudioAssetModel &asset) {
        return lowerCopy(asset.originalSourcePath) == catalogSourcePath;
    });
}

std::filesystem::path AudioManagerController::resolveAudioPath(const AudioAssetModel &asset) const
{
    return ProjectAssets::resolveAudioPath(asset);
}
