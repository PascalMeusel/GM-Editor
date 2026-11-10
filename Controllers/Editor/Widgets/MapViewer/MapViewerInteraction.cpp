#include "Controllers/Editor/Widgets/MapViewer/MapViewerControllerInternals.hpp"

void MapViewerController::setInteractionMode(MapViewerInteractionMode mode)
{
    if (_interactionMode == mode)
        return;

    cancelFogStroke();
    _interactionMode = mode;
    if (_interactionMode != MapViewerInteractionMode::MoveParty)
        clearPendingPartyMove();
    if (_interactionMode != MapViewerInteractionMode::SelectDetailArea)
    {
        _detailSelectionDragging = false;
        cancelDetailGeneration();
    }
}

bool MapViewerController::playerPingModeAvailable() const
{
    const MapModel *map = getSelectedMap();
    const LocalPlayerDisplayModel &display = _currentCampaign.localPlayerDisplay;
    return map && display.publicMapId == map->metadata.id &&
           localPlayerDisplayIsPublished(display) &&
           display.publishedReviewFingerprint != 0;
}

void MapViewerController::beginFogStroke(int x, int y)
{
    MapModel *map = getSelectedMap();
    const bool fogMode = _interactionMode == MapViewerInteractionMode::RevealFog ||
                         _interactionMode == MapViewerInteractionMode::HideFog;
    if (!fogMode || !map || !isMapCellInside(*map, x, y))
        return;

    cancelFogStroke();
    _mapInteractionStatusMessage.clear();
    const bool targetFogged = _interactionMode == MapViewerInteractionMode::HideFog;
    if (!_fogStroke.begin(*map, targetFogged))
    {
        _mapInteractionStatusMessage = gm::localization::trUi(
            "Fog editing is unavailable because this map grid is incomplete.");
        return;
    }
    _fogStrokeLastX = x;
    _fogStrokeLastY = y;
    continueFogStroke(x, y);
}

void MapViewerController::continueFogStroke(int x, int y)
{
    if (!_fogStroke.active())
        return;
    MapModel *map = findMapById(_fogStroke.mapId());
    if (!map || !isMapCellInside(*map, x, y))
        return;

    if (_fogStrokeLastX < 0 || _fogStrokeLastY < 0)
    {
        _fogStrokeLastX = x;
        _fogStrokeLastY = y;
    }

    // Bresenham interpolation prevents gaps when the mouse crosses several
    // cells between two rendered frames.
    int currentX = _fogStrokeLastX;
    int currentY = _fogStrokeLastY;
    const bool strokeAlreadyChanged = _fogStroke.changed();
    const int deltaX = std::abs(x - currentX);
    const int stepX = currentX < x ? 1 : -1;
    const int deltaY = -std::abs(y - currentY);
    const int stepY = currentY < y ? 1 : -1;
    int error = deltaX + deltaY;
    bool changed = false;
    while (true)
    {
        changed = _fogStroke.paint(*map, currentX, currentY) || changed;
        if (currentX == x && currentY == y)
            break;
        const int doubledError = error * 2;
        if (doubledError >= deltaY)
        {
            error += deltaY;
            currentX += stepX;
        }
        if (doubledError <= deltaX)
        {
            error += deltaX;
            currentY += stepY;
        }
    }

    _fogStrokeLastX = x;
    _fogStrokeLastY = y;
    if (changed && !strokeAlreadyChanged)
    {
        // Never let a previously reviewed frame remain visible after even the
        // first in-memory fog mutation. The cell fog overlay is drawn directly,
        // so one plan invalidation per stroke is sufficient and avoids a full
        // plan rebuild for every crossed cell.
        blackoutLocalPlayerDisplay(_currentCampaign.localPlayerDisplay);
        ++_mapRenderContentVersion;
    }
}

void MapViewerController::finishFogStroke()
{
    if (!_fogStroke.active())
        return;
    MapModel *map = findMapById(_fogStroke.mapId());
    _fogStrokeLastX = _fogStrokeLastY = -1;
    if (!map)
    {
        _fogStroke.accept();
        return;
    }
    if (!_fogStroke.changed())
    {
        _fogStroke.accept();
        return;
    }
    if (!_fogStroke.readyToPersist(*map))
    {
        _fogStroke.rollback(*map);
        ++_mapRenderContentVersion;
        _mapInteractionStatusMessage = gm::localization::trUi(
            "The fog stroke conflicted with another map change and was cancelled.");
        return;
    }

    const std::size_t changedCells = _fogStroke.changedCellCount();
    if (!saveCampaign())
    {
        _fogStroke.rollback(*map);
        ++_mapRenderContentVersion;
        _mapInteractionStatusMessage.clear();
        return;
    }
    _fogStroke.accept();
    _mapInteractionStatusMessage = gm::localization::trUi(
        changedCells == 1
            ? "Fog change saved. Review and publish the player view again when it is ready."
            : "Fog stroke saved. Review and publish the player view again when it is ready.");
}

void MapViewerController::cancelFogStroke()
{
    if (!_fogStroke.active())
        return;
    MapModel *map = findMapById(_fogStroke.mapId());
    const bool changed = _fogStroke.changed();
    if (map)
        _fogStroke.rollback(*map);
    else
        _fogStroke.accept();
    _fogStrokeLastX = _fogStrokeLastY = -1;
    if (changed)
    {
        ++_mapRenderContentVersion;
        _mapInteractionStatusMessage = gm::localization::trUi(
            "Fog stroke cancelled. The map was restored; the player display remains blacked out.");
    }
}

bool MapViewerController::pingPlayersAtCell(int x, int y)
{
    const MapModel *map = getSelectedMap();
    if (!map || !isMapCellInside(*map, x, y))
        return false;

    const LocalPlayerDisplayModel &display = _currentCampaign.localPlayerDisplay;
    if (display.publicMapId != map->metadata.id || !localPlayerDisplayIsPublished(display))
    {
        _mapInteractionStatusMessage = gm::localization::trUi(
            "Publish this map to the Player Display before sending a ping.");
        return false;
    }
    if (!PlayerDisplayReview::publishedStateIsCurrent(_currentCampaign))
    {
        _mapInteractionStatusMessage = gm::localization::trUi(
            "Review and publish the current player view before sending a ping.");
        return false;
    }
    if (!playerSafeMapCellIsRevealed(*map, x, y))
    {
        _mapInteractionStatusMessage = gm::localization::trUi(
            "Reveal this cell before sending a player ping.");
        return false;
    }

    const float normalizedX = (static_cast<float>(x) + 0.5f) /
                              static_cast<float>(std::max(1, map->width));
    const float normalizedY = (static_cast<float>(y) + 0.5f) /
                              static_cast<float>(std::max(1, map->height));
    if (!startPlayerSafeDisplayPing(_currentCampaign, normalizedX, normalizedY, 3.0f))
    {
        _mapInteractionStatusMessage = gm::localization::trUi(
            "The player ping was blocked because the published view is no longer safe.");
        return false;
    }
    _mapInteractionStatusMessage = gm::localization::trUi(
        "Player ping shown for three seconds.");
    return true;
}
