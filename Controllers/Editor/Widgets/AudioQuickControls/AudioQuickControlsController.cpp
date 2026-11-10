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

AudioQuickControlsController::AudioQuickControlsController(Campaign &currentCampaign, std::shared_ptr<AudioPlaybackService> playbackService)
    : _currentCampaign(currentCampaign), _playbackService(std::move(playbackService))
{
    _view = std::make_unique<AudioQuickControlsView>(*this);
}

void AudioQuickControlsController::configure()
{
    if (_saveFailed && _saveFailureCampaignId != _currentCampaign.metadata.id)
    {
        _saveFailed = false;
        _saveErrorMessage.clear();
        _statusMessage.clear();
        _errorMessage.clear();
    }
    if (_selectedAssetIndex < 0 && !_currentCampaign.audioAssets.empty())
        selectAsset(0);
    if (_selectedAssetIndex >= static_cast<int>(_currentCampaign.audioAssets.size()))
        selectAsset(static_cast<int>(_currentCampaign.audioAssets.size()) - 1);
}

void AudioQuickControlsController::update(std::shared_ptr<GLFWwindow> window)
{
    (void)window;
    if (!m_isOpen)
        return;

    configure();
    _view->update();
    if (!_view->isVisible())
        m_isOpen = false;
}

const AudioAssetModel *AudioQuickControlsController::getSelectedAsset() const
{
    if (_selectedAssetIndex < 0 || _selectedAssetIndex >= static_cast<int>(_currentCampaign.audioAssets.size()))
        return nullptr;
    return &_currentCampaign.audioAssets[static_cast<size_t>(_selectedAssetIndex)];
}

AudioAssetModel *AudioQuickControlsController::getSelectedAsset()
{
    if (_selectedAssetIndex < 0 || _selectedAssetIndex >= static_cast<int>(_currentCampaign.audioAssets.size()))
        return nullptr;
    return &_currentCampaign.audioAssets[static_cast<size_t>(_selectedAssetIndex)];
}

void AudioQuickControlsController::selectAsset(int index)
{
    if (_currentCampaign.audioAssets.empty())
    {
        _selectedAssetIndex = -1;
        return;
    }

    _selectedAssetIndex = std::clamp(index, 0, static_cast<int>(_currentCampaign.audioAssets.size()) - 1);
    syncControlsFromSelection();
}

void AudioQuickControlsController::playSelectedAsset()
{
    AudioAssetModel *asset = getSelectedAsset();
    if (!asset)
        return;

    _errorMessage.clear();
    if (!ensureSelectedAssetFileAvailable(*asset))
        return;
    if (!saveCampaign())
        return;

    if (_playbackService->play(*asset, ProjectAssets::resolveAudioPath(*asset), AudioMix::effectiveVolume(_currentCampaign, *asset, _volume), _loop, _errorMessage))
    {
        // Playback history changes only after the intended configuration is durable.
        ProjectSaveService::getInstance().markDirty("Audio playback history updated");
        _statusMessage = gm::localization::LocalizationService::instance().format(
            gm::localization::TextId::LivePlayingAsset, {audioDisplayName(*asset)});
    }
}

void AudioQuickControlsController::pauseSelectedAsset()
{
    const AudioAssetModel *asset = getSelectedAsset();
    if (!asset || !_playbackService->isActive(asset->metadata.id))
    {
        _statusMessage = gm::localization::trUi("Selected audio is not playing.");
        return;
    }

    _errorMessage.clear();
    if (_playbackService->pause(asset->metadata.id, _errorMessage))
        _statusMessage = gm::localization::LocalizationService::instance().format(
            gm::localization::TextId::LivePausedAsset, {audioDisplayName(*asset)});
}

void AudioQuickControlsController::resumeSelectedAsset()
{
    const AudioAssetModel *asset = getSelectedAsset();
    if (!asset || !_playbackService->isActive(asset->metadata.id))
    {
        _statusMessage = gm::localization::trUi("Selected audio is not active.");
        return;
    }
    if (!_playbackService->isPaused(asset->metadata.id))
    {
        _statusMessage = gm::localization::trUi("Selected audio is already playing.");
        return;
    }

    _errorMessage.clear();
    if (!saveCampaign())
        return;
    if (_playbackService->resume(asset->metadata.id, _errorMessage))
        _statusMessage = gm::localization::LocalizationService::instance().format(
            gm::localization::TextId::LiveResumedAsset, {audioDisplayName(*asset)});
}

void AudioQuickControlsController::stopSelectedAsset()
{
    const AudioAssetModel *asset = getSelectedAsset();
    if (!asset || !_playbackService->isActive(asset->metadata.id))
    {
        _statusMessage = gm::localization::trUi("Selected audio is not active.");
        return;
    }

    _errorMessage.clear();
    if (_playbackService->stop(asset->metadata.id, _errorMessage))
        _statusMessage = gm::localization::LocalizationService::instance().format(
            gm::localization::TextId::LiveStoppedAsset, {audioDisplayName(*asset)});
}

void AudioQuickControlsController::stopAllAudio()
{
    const std::vector<EntityId> activeIds = _playbackService->activeAssetIds();
    _playbackService->stopAll();
    _errorMessage.clear();
    _statusMessage = activeIds.empty()
                         ? gm::localization::trUi("No active audio channels to stop.")
                         : gm::localization::tr(gm::localization::TextId::LiveStoppedAllAudio);
}

void AudioQuickControlsController::playFirstAssetOfType(AudioAssetType type, bool loop)
{
    const int assetIndex = firstAssetIndexOfType(type);
    if (assetIndex < 0)
    {
        _statusMessage = std::string(gm::localization::trUi("No assets of this type are available.")) +
                         " (" + gm::localization::trUi(audioAssetTypeLabel(type)) + ")";
        return;
    }

    selectAsset(assetIndex);
    _loop = loop;
    playSelectedAsset();
}

bool AudioQuickControlsController::hasAssetOfType(AudioAssetType type) const
{
    return firstAssetIndexOfType(type) >= 0;
}
