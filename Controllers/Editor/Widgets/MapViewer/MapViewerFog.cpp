#include "Controllers/Editor/Widgets/MapViewer/MapViewerControllerInternals.hpp"

void MapViewerController::revealAllFogOnSelectedMap()
{
    MapModel *map = getSelectedMap();
    if (!map)
        return;
    MapViewerFogStroke transaction;
    if (!transaction.begin(*map, false))
    {
        _mapInteractionStatusMessage = gm::localization::trUi(
            "Fog editing is unavailable because this map grid is incomplete.");
        return;
    }
    for (int y = 0; y < map->height; ++y)
        for (int x = 0; x < map->width; ++x)
            transaction.paint(*map, x, y);
    if (!transaction.changed())
    {
        transaction.accept();
        return;
    }
    blackoutLocalPlayerDisplay(_currentCampaign.localPlayerDisplay);
    ++_mapRenderContentVersion;
    if (saveCampaign())
    {
        transaction.accept();
        _mapInteractionStatusMessage = gm::localization::trUi(
            "All fog was revealed. Review and publish the player view again when it is ready.");
    }
    else
    {
        transaction.rollback(*map);
        ++_mapRenderContentVersion;
        _mapInteractionStatusMessage.clear();
    }
}

bool MapViewerController::currentCameraForMap(const EntityId &mapId,
                                              LocalPlayerDisplayCameraState &camera) const
{
    if (!_viewCameraValid || mapId.empty() || _viewCameraMapId != mapId)
        return false;
    camera = _viewCamera;
    return true;
}

void MapViewerController::updateViewCameraSnapshot(const EntityId &mapId,
                                                   float centerX,
                                                   float centerY,
                                                   float zoom)
{
    if (mapId.empty())
    {
        _viewCameraValid = false;
        return;
    }
    _viewCameraMapId = mapId;
    _viewCamera.normalizedCenterX = centerX;
    _viewCamera.normalizedCenterY = centerY;
    _viewCamera.zoom = zoom;
    _viewCamera.followGmCamera = true;
    normalizeLocalPlayerDisplayCamera(_viewCamera);
    _viewCameraValid = true;
}

void MapViewerController::clearAllFogOnSelectedMap()
{
    MapModel *map = getSelectedMap();
    if (!map)
        return;
    MapViewerFogStroke transaction;
    if (!transaction.begin(*map, true))
    {
        _mapInteractionStatusMessage = gm::localization::trUi(
            "Fog editing is unavailable because this map grid is incomplete.");
        return;
    }
    for (int y = 0; y < map->height; ++y)
        for (int x = 0; x < map->width; ++x)
            transaction.paint(*map, x, y);
    if (!transaction.changed())
    {
        transaction.accept();
        return;
    }
    blackoutLocalPlayerDisplay(_currentCampaign.localPlayerDisplay);
    ++_mapRenderContentVersion;
    if (saveCampaign())
    {
        transaction.accept();
        _mapInteractionStatusMessage = gm::localization::trUi(
            "The map was hidden by fog. Review and publish the player view again when it is ready.");
    }
    else
    {
        transaction.rollback(*map);
        ++_mapRenderContentVersion;
        _mapInteractionStatusMessage.clear();
    }
}

bool MapViewerController::saveCampaign()
{
    // Invalidate view plans for the in-memory mutation before attempting disk
    // persistence. Transactional callers that roll back a failed write bump
    // the version once more after restoring their original snapshot.
    ++_mapRenderContentVersion;
    ProjectSaveService &saveService = ProjectSaveService::getInstance();
    if (saveService.saveCampaign(_currentCampaign, "map viewer change"))
        return true;

    // ProjectSaveService deliberately leaves its global dirty bit set after a
    // failed write. Surface that state in the atlas as well; callers must not
    // overwrite this message with a success message.
    _atlasStatusMessage =
        std::string(gm::localization::trUi(
            "The campaign could not be saved. Open Settings > Project Health for details.")) +
        " " + gm::localization::trUi("Unsaved campaign changes are pending.");
    return false;
}

void MapViewerController::onShow()
{
    _focusViewRequested = true;
    _fitZoomRequested = true;
    _view->show();
    _view->requestDockOnNextUpdate();
}
