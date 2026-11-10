#include "Controllers/Editor/Widgets/GMScreen/GMScreenControllerSupport.hpp"

void GMScreenController::touchLocalPlayerDisplay()
{
    _currentCampaign.localPlayerDisplay.metadata.touch();
    _currentCampaign.metadata.touch();
}

const MapModel *GMScreenController::reviewedPublicMap() const
{
    return PlayerDisplayReview::publicMap(_currentCampaign);
}

void GMScreenController::resetPlayerDisplayReview(bool clearRestore) { _view->resetPlayerReview(clearRestore); }

bool GMScreenController::armLocalPlayerDisplayFromReview(const PlayerDisplayReviewSnapshot &review)
{
    if(_view->rejectUnreadyPlayerReview(review))return false;
    const auto heldMemo=_view->compileMemoFor(review);
    gm::map_render::MapRenderCompileMemo localMemo;
    if (!PlayerDisplayReview::arm(_currentCampaign, review, heldMemo?heldMemo.get():&localMemo))
    { _view->invalidatePlayerReview(review);
      ::blackoutLocalPlayerDisplay(_currentCampaign.localPlayerDisplay); return false; }
    touchLocalPlayerDisplay();
    return true;
}

bool GMScreenController::publishLocalPlayerDisplay(const PlayerDisplayReviewSnapshot &review)
{
    if(_view->rejectUnreadyPlayerReview(review))return false;
    const bool armed=_currentCampaign.localPlayerDisplay.enabled;
    const auto heldMemo=_view->compileMemoFor(review);
    gm::map_render::MapRenderCompileMemo localMemo;
    if (!PlayerDisplayReview::publish(_currentCampaign, review, heldMemo?heldMemo.get():&localMemo))
    { if(armed)_view->invalidatePlayerReview(review);
      ::blackoutLocalPlayerDisplay(_currentCampaign.localPlayerDisplay); return false; }
    touchLocalPlayerDisplay();
    return true;
}

void GMScreenController::blackoutLocalPlayerDisplayOutput()
{
    ::blackoutLocalPlayerDisplay(_currentCampaign.localPlayerDisplay);
    touchLocalPlayerDisplay();
}

bool GMScreenController::pingLocalPlayerDisplay()
{
    if (!PlayerDisplayReview::publishedStateIsCurrent(_currentCampaign))
        return false;
    return startPlayerSafeDisplayPing(_currentCampaign,
                                      _currentCampaign.localPlayerDisplay.publicCamera.normalizedCenterX,
                                      _currentCampaign.localPlayerDisplay.publicCamera.normalizedCenterY);
}

void GMScreenController::setPublicScene(const EntityId &sceneId)
{
    _currentCampaign.localPlayerDisplay.publicSceneId = sceneId;
    ::blackoutLocalPlayerDisplay(_currentCampaign.localPlayerDisplay);
    touchLocalPlayerDisplay();
}

void GMScreenController::setPublicMap(const EntityId &mapId)
{
    LocalPlayerDisplayModel &display = _currentCampaign.localPlayerDisplay;
    if (display.publicMapId != mapId)
        resetLocalPlayerDisplayCameraForMapChange(display.publicCamera);
    display.publicMapId = mapId;
    ::blackoutLocalPlayerDisplay(_currentCampaign.localPlayerDisplay);
    const auto map = std::find_if(_currentCampaign.maps.begin(), _currentCampaign.maps.end(), [&](const MapModel &candidate) { return candidate.metadata.id == mapId; });
    if (map != _currentCampaign.maps.end())
        map->visibility = MapVisibility::PlayersVisible;
    touchLocalPlayerDisplay();
}

void GMScreenController::setPublicMapReleaseCategory(MapPlayerReleaseCategory category, bool visible)
{
    const EntityId &mapId = _currentCampaign.localPlayerDisplay.publicMapId;
    const auto map = std::find_if(_currentCampaign.maps.begin(), _currentCampaign.maps.end(),
                                  [&](const MapModel &candidate) { return candidate.metadata.id == mapId; });
    if (map == _currentCampaign.maps.end() ||
        !setMapPlayerReleaseCategory(map->playerRelease, category, visible))
        return;

    map->metadata.touch();
    ::blackoutLocalPlayerDisplay(_currentCampaign.localPlayerDisplay);
    touchLocalPlayerDisplay();
    ProjectSaveService::getInstance().saveCampaign(
        _currentCampaign,
        visible ? "release public map content category" : "hide public map content category");
}

void GMScreenController::setPublicEncounter(const EntityId &encounterId)
{
    _currentCampaign.localPlayerDisplay.publicEncounterId = encounterId;
    ::blackoutLocalPlayerDisplay(_currentCampaign.localPlayerDisplay);
    touchLocalPlayerDisplay();
}

void GMScreenController::clearLocalPlayerDisplay()
{
    LocalPlayerDisplayModel &display = _currentCampaign.localPlayerDisplay;
    disableLocalPlayerDisplay(display);
    display.publicSceneId.clear();
    display.publicMapId.clear();
    display.publicEncounterId.clear();
    display.visibleNpcIds.clear();
    display.publicHandoutIds.clear();
    display.sharedArtAssetIds.clear();
    touchLocalPlayerDisplay();
}
