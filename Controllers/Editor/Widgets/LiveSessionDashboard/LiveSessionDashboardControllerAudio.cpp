#include "Controllers/Editor/Widgets/LiveSessionDashboard/LiveSessionDashboardControllerSupport.hpp"

void LiveSessionDashboardController::selectAudioAsset(int index)
{
    if (_currentCampaign.audioAssets.empty())
    {
        _selectedAudioAssetIndex = -1;
        return;
    }

    _selectedAudioAssetIndex = std::clamp(index, 0, static_cast<int>(_currentCampaign.audioAssets.size()) - 1);
    const AudioAssetModel *asset = selectedAudioAsset();
    if (asset)
    {
        _audioVolume = std::clamp(asset->defaultVolume, 0.0f, 1.0f);
        _audioLoop = asset->loop;
    }
}

void LiveSessionDashboardController::playSelectedAudio()
{
    AudioAssetModel *asset = selectedAudioAsset();
    if (!asset)
        return;

    _audioErrorMessage.clear();
    if (!ensureSelectedAudioFileAvailable(*asset))
        return;

    if (_playbackService->play(*asset, ProjectAssets::resolveAudioPath(*asset), AudioMix::effectiveVolume(_currentCampaign, *asset, _audioVolume), _audioLoop, _audioErrorMessage))
    {
        saveCampaign("play selected audio");
        _audioStatusMessage = gm::localization::LocalizationService::instance().format(
            gm::localization::TextId::LivePlayingAsset,
            {asset->name.empty() ? asset->originalFileName : asset->name});
    }
}

void LiveSessionDashboardController::pauseSelectedAudio()
{
    const AudioAssetModel *asset = selectedAudioAsset();
    if (asset && _playbackService->pause(asset->metadata.id, _audioErrorMessage))
        _audioStatusMessage = gm::localization::LocalizationService::instance().format(
            gm::localization::TextId::LivePausedAsset,
            {asset->name.empty() ? asset->originalFileName : asset->name});
}

void LiveSessionDashboardController::resumeSelectedAudio()
{
    const AudioAssetModel *asset = selectedAudioAsset();
    if (asset && _playbackService->resume(asset->metadata.id, _audioErrorMessage))
        _audioStatusMessage = gm::localization::LocalizationService::instance().format(
            gm::localization::TextId::LiveResumedAsset,
            {asset->name.empty() ? asset->originalFileName : asset->name});
}

void LiveSessionDashboardController::stopSelectedAudio()
{
    const AudioAssetModel *asset = selectedAudioAsset();
    if (asset && _playbackService->stop(asset->metadata.id, _audioErrorMessage))
        _audioStatusMessage = gm::localization::LocalizationService::instance().format(
            gm::localization::TextId::LiveStoppedAsset,
            {asset->name.empty() ? asset->originalFileName : asset->name});
}

void LiveSessionDashboardController::stopAllAudio()
{
    _playbackService->stopAll();
    _audioErrorMessage.clear();
    _audioStatusMessage = gm::localization::tr(gm::localization::TextId::LiveStoppedAllAudio);
}

void LiveSessionDashboardController::applySelectedAudioVolume()
{
    const AudioAssetModel *asset = selectedAudioAsset();
    if (asset && _playbackService->isActive(asset->metadata.id))
        _playbackService->setVolume(asset->metadata.id, AudioMix::effectiveVolume(_currentCampaign, *asset, _audioVolume), _audioErrorMessage);
}

bool LiveSessionDashboardController::isSelectedAudioActive() const
{
    const AudioAssetModel *asset = selectedAudioAsset();
    return asset && _playbackService->isActive(asset->metadata.id);
}

bool LiveSessionDashboardController::isSelectedAudioPaused() const
{
    const AudioAssetModel *asset = selectedAudioAsset();
    return asset && _playbackService->isPaused(asset->metadata.id);
}

void LiveSessionDashboardController::openAudioMini()
{
    if (_openAudioMiniHandler)
        _openAudioMiniHandler();
}

void LiveSessionDashboardController::openAudioLibrary()
{
    if (_openAudioLibraryHandler)
        _openAudioLibraryHandler();
}
