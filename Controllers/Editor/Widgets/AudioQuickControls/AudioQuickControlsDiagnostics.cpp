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

AudioQuickChannelSummary AudioQuickControlsController::channelSummary() const
{
    AudioQuickChannelSummary summary;
    for (const EntityId &channelId : _playbackService->activeAssetIds())
    {
        if (_playbackService->isPaused(channelId))
            ++summary.paused;
        else
            ++summary.playing;

        if (const AudioAmbienceLayerModel *layer = findAmbienceLayerById(channelId))
        {
            ++summary.ambience;
            appendChannelName(summary.ambienceText, layer->name.empty() ? std::string(gm::localization::trUi("Ambience layer")) : layer->name);
            continue;
        }

        const AudioAssetModel *asset = findAudioAssetById(channelId);
        const std::string name = asset ? audioDisplayName(*asset) : channelId;
        const AudioAssetType type = asset ? asset->type : AudioAssetType::Other;
        switch (type)
        {
        case AudioAssetType::Music:
            ++summary.music;
            appendChannelName(summary.musicText, name);
            break;
        case AudioAssetType::Ambience:
            ++summary.ambience;
            appendChannelName(summary.ambienceText, name);
            break;
        case AudioAssetType::Effect:
            ++summary.effects;
            appendChannelName(summary.effectsText, name);
            break;
        case AudioAssetType::Voice:
        case AudioAssetType::Other:
            ++summary.other;
            break;
        }
    }

    summary.musicText = summaryText(summary.music, summary.musicText);
    summary.ambienceText = summaryText(summary.ambience, summary.ambienceText);
    summary.effectsText = summaryText(summary.effects, summary.effectsText);
    return summary;
}

AudioQuickFileDiagnostics AudioQuickControlsController::fileDiagnostics() const
{
    AudioQuickFileDiagnostics diagnostics;
    const AudioAssetModel *selectedAsset = getSelectedAsset();
    for (const AudioAssetModel &asset : _currentCampaign.audioAssets)
    {
        if (std::filesystem::exists(ProjectAssets::resolveAudioPath(asset)))
            continue;

        ++diagnostics.missingAssets;
        bool repairable = false;
        if (!asset.originalSourcePath.empty())
        {
            const std::filesystem::path sourcePath = ProjectAssets::resolveOriginalSourcePath(asset);
            repairable = std::filesystem::exists(sourcePath) && std::filesystem::is_regular_file(sourcePath);
        }
        if (repairable)
            ++diagnostics.repairableAssets;

        if (selectedAsset && selectedAsset->metadata.id == asset.metadata.id)
        {
            diagnostics.selectedMissing = true;
            diagnostics.selectedRepairable = repairable;
            diagnostics.selectedMessage = repairable
                                              ? gm::localization::trUi("Selected file is missing from the campaign copy. Play will try to restore it from the original source.")
                                              : gm::localization::trUi("Selected file is missing. Play will ask you to relink it from the Audio Library.");
        }
    }
    return diagnostics;
}

AudioQuickPlaylistSummary AudioQuickControlsController::playlistSummary() const
{
    AudioQuickPlaylistSummary summary;
    const AudioPlaylistPlaybackState state = _playbackService->playlistPlaybackState();
    if (!state.active)
        return summary;

    const auto assetText = [&](const EntityId &assetId, const char *fallback) {
        if (assetId.empty())
            return std::string(fallback);
        const AudioAssetModel *asset = findAudioAssetById(assetId);
        return asset ? audioDisplayName(*asset) : assetId;
    };

    summary.active = true;
    summary.playlistName = state.playlistName.empty() ? gm::localization::trUi("Untitled playlist") : state.playlistName;
    summary.currentText = assetText(state.currentAssetId, gm::localization::trUi("None"));
    summary.previousText = assetText(state.previousAssetId, gm::localization::trUi("Start of queue"));
    summary.lastText = assetText(state.lastAssetId, gm::localization::trUi("None"));
    summary.nextText = assetText(state.nextAssetId, gm::localization::trUi("End of queue"));
    summary.activeQueuedCount = state.activeQueuedCount;
    summary.queueSize = static_cast<int>(state.queueAssetIds.size());
    summary.transitionActive = state.transitionActive;
    summary.transitionText = state.transitionLabel;

    if (state.shuffle)
        appendChannelName(summary.flagsText, gm::localization::trUi("Shuffle"));
    if (state.repeat)
        appendChannelName(summary.flagsText, gm::localization::trUi("Repeat"));
    if (state.oneShot)
        appendChannelName(summary.flagsText, gm::localization::trUi("One-shot"));
    if (summary.flagsText.empty())
        summary.flagsText = gm::localization::trUi("Ordered");

    return summary;
}

bool AudioQuickControlsController::ensureSelectedAssetFileAvailable(AudioAssetModel &asset)
{
    if (std::filesystem::exists(ProjectAssets::resolveAudioPath(asset)))
        return true;

    if (!asset.originalSourcePath.empty())
    {
        std::filesystem::path sourcePath = ProjectAssets::resolveOriginalSourcePath(asset);
        if (std::filesystem::exists(sourcePath) && std::filesystem::is_regular_file(sourcePath))
        {
            if (ProjectAssets::repairAudioAssetFromSource(_currentCampaign, asset, sourcePath, _errorMessage))
            {
                if (!saveCampaign())
                    return false;
                _statusMessage = gm::localization::tr(gm::localization::TextId::LiveRestoredAudio);
                return true;
            }
            return false;
        }
    }

    _errorMessage = gm::localization::tr(gm::localization::TextId::LiveMissingAudio);
    return false;
}

bool AudioQuickControlsController::saveCampaign()
{
    _currentCampaign.metadata.touch();
    auto &service = ProjectSaveService::getInstance();
    _saveFailed = !service.saveCampaign(_currentCampaign, "quick audio edits");
    if (_saveFailed)
    {
        _saveFailureCampaignId = _currentCampaign.metadata.id;
        _statusMessage.clear();
        _saveErrorMessage = uiText("Not saved. Your audio changes remain here. Retry Save before closing. ",
                                   "Nicht gespeichert. Deine Audioänderungen bleiben hier. Vor dem Schließen erneut speichern. ") + service.state().lastError;
        return false;
    }
    _saveErrorMessage.clear();
    return true;
}

void AudioQuickControlsController::retrySave()
{
    if (saveCampaign())
        _statusMessage = uiText("Audio changes saved. Press Play or Resume when ready.",
                               "Audioänderungen gespeichert. Bei Bedarf Wiedergabe starten oder fortsetzen.");
    // Retry only saves retained identities; it never repeats a sound command.
}

void AudioQuickControlsController::syncControlsFromSelection()
{
    const AudioAssetModel *asset = getSelectedAsset();
    if (!asset)
        return;

    _volume = std::clamp(asset->defaultVolume, 0.0f, 1.0f);
    _loop = asset->loop;
}

void AudioQuickControlsController::onShow()
{
    configure();
    _view->show();
    _view->requestDockOnNextUpdate();
}
