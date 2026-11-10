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

void AudioManagerController::addAmbienceLayer()
{
    if (_saveFailed) { retrySave(); return; }

    AudioAmbienceLayerModel layer;
    layer.name = uiText("Ambience Layer ", "Atmosphärenebene ") + std::to_string(_currentCampaign.audioAmbienceLayers.size() + 1);
    layer.sortOrder = static_cast<int>(_currentCampaign.audioAmbienceLayers.size());
    layer.metadata.tags = {"ambience"};
    for (const AudioAssetModel &asset : _currentCampaign.audioAssets)
    {
        if (asset.type == AudioAssetType::Ambience)
        {
            layer.audioAssetId = asset.metadata.id;
            layer.volume = asset.defaultVolume;
            layer.loop = true;
            break;
        }
    }
    if(!_newLayerName.empty())layer.name=_newLayerName;
    if(!_newLayerAssetId.empty())layer.audioAssetId=_newLayerAssetId;
    layer.volume=std::clamp(_newLayerVolume,0.f,1.f);layer.loop=_newLayerLoop;
    layer.active=false;
    layer.metadata.touch();
    _currentCampaign.audioAmbienceLayers.push_back(layer);
    if (!saveCampaign()) return;
    _statusMessage = uiText("Added ambience layer: ", "Atmosphärenebene hinzugefügt: ") + layer.name;
    _errorMessage.clear();
}

void AudioManagerController::saveAmbienceLayer(int index)
{
    if (index < 0 || index >= static_cast<int>(_currentCampaign.audioAmbienceLayers.size()))
        return;

    AudioAmbienceLayerModel &layer = _currentCampaign.audioAmbienceLayers[static_cast<size_t>(index)];
    layer.volume = std::clamp(layer.volume, 0.0f, 1.0f);
    layer.sortOrder = index;
    layer.metadata.touch();
    if (!saveCampaign()) return;
    _statusMessage = uiText("Saved ambience layer: ", "Atmosphärenebene gespeichert: ") + layer.name;
    _errorMessage.clear();
}

void AudioManagerController::removeAmbienceLayer(int index)
{
    if (_saveFailed) { retrySave(); return; }

    if (index < 0 || index >= static_cast<int>(_currentCampaign.audioAmbienceLayers.size()))
        return;

    AudioAmbienceLayerModel &layer = _currentCampaign.audioAmbienceLayers[static_cast<size_t>(index)];
    std::string ignored;
    if (_playbackService->isActive(layer.metadata.id))
        _playbackService->stop(layer.metadata.id, ignored);

    const std::string removedName = layer.name;
    _currentCampaign.audioAmbienceLayers.erase(_currentCampaign.audioAmbienceLayers.begin() + index);
    for (int i = 0; i < static_cast<int>(_currentCampaign.audioAmbienceLayers.size()); ++i)
        _currentCampaign.audioAmbienceLayers[static_cast<size_t>(i)].sortOrder = i;
    if (!saveCampaign()) return;
    _statusMessage = uiText("Removed ambience layer: ", "Atmosphärenebene entfernt: ") + removedName;
    _errorMessage.clear();
}

void AudioManagerController::playAmbienceLayer(int index)
{
    if (index < 0 || index >= static_cast<int>(_currentCampaign.audioAmbienceLayers.size()))
        return;

    AudioAmbienceLayerModel &layer = _currentCampaign.audioAmbienceLayers[static_cast<size_t>(index)];
    AudioAssetModel *asset = findAudioAssetById(layer.audioAssetId);
    if (!asset)
    {
        _errorMessage = uiText("Choose an ambience asset for this layer.", "Wähle eine Audiodatei für diese Atmosphärenebene aus.");
        return;
    }

    _errorMessage.clear();
    if (!ensureSelectedAssetFileAvailable(*asset))
        return;
    if (!saveCampaign()) return;

    AudioAssetModel layerAsset = *asset;
    layerAsset.metadata.id = layer.metadata.id;
    layerAsset.name = layer.name.empty() ? asset->name : layer.name + " - " + asset->name;
    if (_playbackService->play(layerAsset, resolveAudioPath(*asset), AudioMix::effectiveVolume(_currentCampaign, *asset, layer.volume), layer.loop, _errorMessage))
    {
        layer.active = true;
        layer.metadata.touch();
        ProjectSaveService::getInstance().markDirty("Audio layer started");
        _statusMessage = uiText("Playing ambience layer: ", "Atmosphärenebene läuft: ") + layer.name;
    }
}

void AudioManagerController::stopAmbienceLayer(int index)
{
    if (index < 0 || index >= static_cast<int>(_currentCampaign.audioAmbienceLayers.size()))
        return;

    AudioAmbienceLayerModel &layer = _currentCampaign.audioAmbienceLayers[static_cast<size_t>(index)];
    _errorMessage.clear();
    if (_playbackService->stop(layer.metadata.id, _errorMessage))
    {
        layer.active = false;
        layer.metadata.touch();
        saveCampaign(); // Stopping succeeded even if the persistence warning remains visible.
        _statusMessage = uiText("Stopped ambience layer: ", "Atmosphärenebene gestoppt: ") + layer.name;
    }
}

void AudioManagerController::applyAmbienceLayerVolume(int index)
{
    if (index < 0 || index >= static_cast<int>(_currentCampaign.audioAmbienceLayers.size()))
        return;

    AudioAmbienceLayerModel &layer = _currentCampaign.audioAmbienceLayers[static_cast<size_t>(index)];
    layer.volume = std::clamp(layer.volume, 0.0f, 1.0f);
    if (_playbackService->isActive(layer.metadata.id))
    {
        if (const AudioAssetModel *asset = findAudioAssetById(layer.audioAssetId))
            _playbackService->setVolume(layer.metadata.id, AudioMix::effectiveVolume(_currentCampaign, *asset, layer.volume), _errorMessage);
    }
}

void AudioManagerController::saveVolumeGroups()
{
    AudioVolumeGroupsModel &groups = _currentCampaign.audioVolumeGroups;
    groups.master = std::clamp(groups.master, 0.0f, 1.0f);
    groups.music = std::clamp(groups.music, 0.0f, 1.0f);
    groups.ambience = std::clamp(groups.ambience, 0.0f, 1.0f);
    groups.effects = std::clamp(groups.effects, 0.0f, 1.0f);
    groups.voice = std::clamp(groups.voice, 0.0f, 1.0f);
    if (!saveCampaign()) return;
    _statusMessage = uiText("Saved audio volume groups.", "Audiolautstärkegruppen gespeichert.");
    _errorMessage.clear();
}

void AudioManagerController::addAudioTrigger()
{
    if (_saveFailed) { retrySave(); return; }

    AudioTriggerModel trigger;
    trigger.name = _newTriggerName.empty() ? "Audio Trigger" : _newTriggerName;
    trigger.sourceType = _newTriggerSourceType;
    trigger.eventName = _newTriggerEventName;
    trigger.sourceEntityId=_newTriggerSourceId;
    trigger.volume=std::clamp(_newTriggerVolume,0.f,1.f);
    if (!_currentCampaign.audioAssets.empty())
        trigger.audioAssetId = _currentCampaign.audioAssets.front().metadata.id;
    if(!_newTriggerAssetId.empty())trigger.audioAssetId=_newTriggerAssetId;
    trigger.metadata.tags = {"audio", "trigger"};
    trigger.metadata.touch();
    _currentCampaign.audioTriggers.push_back(trigger);
    if (!saveCampaign()) return;
    _statusMessage = uiText("Added audio trigger: ", "Audioauslöser hinzugefügt: ") + trigger.name;
    _errorMessage.clear();
}

void AudioManagerController::saveAudioTrigger(int index)
{
    if (index < 0 || index >= static_cast<int>(_currentCampaign.audioTriggers.size()))
        return;

    AudioTriggerModel &trigger = _currentCampaign.audioTriggers[static_cast<size_t>(index)];
    trigger.volume = std::clamp(trigger.volume, 0.0f, 1.0f);
    trigger.metadata.touch();
    if (!saveCampaign()) return;
    _statusMessage = uiText("Saved audio trigger: ", "Audioauslöser gespeichert: ") + trigger.name;
    _errorMessage.clear();
}

void AudioManagerController::removeAudioTrigger(int index)
{
    if (_saveFailed) { retrySave(); return; }

    if (index < 0 || index >= static_cast<int>(_currentCampaign.audioTriggers.size()))
        return;

    const std::string removedName = _currentCampaign.audioTriggers[static_cast<size_t>(index)].name;
    _currentCampaign.audioTriggers.erase(_currentCampaign.audioTriggers.begin() + index);
    if (!saveCampaign()) return;
    _statusMessage = uiText("Removed audio trigger: ", "Audioauslöser entfernt: ") + removedName;
    _errorMessage.clear();
}

void AudioManagerController::fireAudioTrigger(int index)
{
    if (index < 0 || index >= static_cast<int>(_currentCampaign.audioTriggers.size()))
        return;

    const AudioTriggerModel &trigger = _currentCampaign.audioTriggers[static_cast<size_t>(index)];
    _errorMessage.clear();
    if (!saveCampaign()) return;
    const int played = AudioTriggers::playMatchingTriggers(_currentCampaign, _playbackService, trigger.sourceType, trigger.sourceEntityId, trigger.eventName, _errorMessage);
    if (played > 0)
    {
        ProjectSaveService::getInstance().markDirty("Audio playback history updated");
        _statusMessage = uiText("Fired audio trigger: ", "Audioauslöser gestartet: ") + trigger.name;
    }
}
