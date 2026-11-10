#include "Controllers/Editor/Widgets/LiveSessionDashboard/LiveSessionDashboardControllerSupport.hpp"

int LiveSessionDashboardController::rollDie(int sides)
{
    std::uniform_int_distribution<int> die(1, std::max(1, sides));
    return die(_randomGenerator);
}

AudioAssetModel *LiveSessionDashboardController::findAudioAsset(const EntityId &assetId)
{
    auto found = std::find_if(_currentCampaign.audioAssets.begin(), _currentCampaign.audioAssets.end(), [&](const AudioAssetModel &asset) {
        return asset.metadata.id == assetId;
    });
    return found == _currentCampaign.audioAssets.end() ? nullptr : &*found;
}

const AudioAssetModel *LiveSessionDashboardController::findAudioAsset(const EntityId &assetId) const
{
    auto found = std::find_if(_currentCampaign.audioAssets.begin(), _currentCampaign.audioAssets.end(), [&](const AudioAssetModel &asset) {
        return asset.metadata.id == assetId;
    });
    return found == _currentCampaign.audioAssets.end() ? nullptr : &*found;
}

bool LiveSessionDashboardController::playAudioAsset(const EntityId &assetId, float volume, bool loop, const std::string &contextLabel)
{
    AudioAssetModel *asset = findAudioAsset(assetId);
    if (!asset)
        return false;
    if (!ensureSelectedAudioFileAvailable(*asset))
        return false;
    const bool played = _playbackService->play(*asset, ProjectAssets::resolveAudioPath(*asset), AudioMix::effectiveVolume(_currentCampaign, *asset, volume), loop, _audioErrorMessage);
    if (played)
        saveCampaign("play " + contextLabel);
    return played;
}

bool LiveSessionDashboardController::ensureSelectedAudioFileAvailable(AudioAssetModel &asset)
{
    if (std::filesystem::exists(ProjectAssets::resolveAudioPath(asset)))
        return true;

    if (!asset.originalSourcePath.empty())
    {
        const std::filesystem::path sourcePath = ProjectAssets::resolveOriginalSourcePath(asset);
        if (std::filesystem::exists(sourcePath) && std::filesystem::is_regular_file(sourcePath))
        {
            if (ProjectAssets::repairAudioAssetFromSource(_currentCampaign, asset, sourcePath, _audioErrorMessage))
            {
                saveCampaign("restore missing campaign audio copy");
                _audioStatusMessage = gm::localization::tr(gm::localization::TextId::LiveRestoredAudio);
                return true;
            }
            return false;
        }
    }

    _audioErrorMessage = gm::localization::tr(gm::localization::TextId::LiveMissingAudio);
    return false;
}

void LiveSessionDashboardController::saveCampaign(const std::string &reason)
{
    _currentCampaign.metadata.touch();
    ProjectSaveService::getInstance().saveCampaign(_currentCampaign, reason);
}

void LiveSessionDashboardController::onShow()
{
    configure();
    _view->show();
    _view->requestDockOnNextUpdate();
}
