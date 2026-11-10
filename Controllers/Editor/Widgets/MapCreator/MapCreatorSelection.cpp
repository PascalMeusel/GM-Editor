#include "Models/Project/ContextLinkMetadata.hpp"
#include "Models/Map/MapTravelCreation.hpp"
#include "MapCreatorController.hpp"
#include <random>
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorArtStylePolicy.hpp"
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorFilePathPolicy.hpp"
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorPersistencePolicy.hpp"
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorPlayerOutputPolicy.hpp"
#include "Models/Map/MapAtlasService.hpp"
#include "Models/Map/MapAuthoring.hpp"
#include "Models/Map/MapStrokeAuthoring.hpp"
#include "Models/Map/MapCheckpointService.hpp"
#include "Models/Map/MapGeneration.hpp"
#include "Models/Map/MapImageImportService.hpp"
#include "Models/Map/MapPackageDraftInstallService.hpp"
#include "Models/Map/MapPackageService.hpp"
#include "Models/Map/MapPngExport.hpp"
#include "Models/Map/MapRenderPlan.hpp"
#include "Models/Map/TacticalMapGenerator.hpp"
#include "Models/Map/MapWorldBrief.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Persistence/PersistenceSerialization.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Project/WorldInspectorService.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <Windows.h>
#include <commdlg.h>
#endif

using namespace PersistenceSerialization;

#include "MapCreatorInternals.hpp"
using namespace gm::map_creator_detail;

void MapCreatorController::selectObjectAt(int x, int y)
{
    clearObjectSelection();

    for (int index = static_cast<int>(_draftMap.tokens.size()) - 1; index >= 0; --index)
    {
        const MapTokenModel &token = _draftMap.tokens[index];
        if (static_cast<int>(token.x) == x && static_cast<int>(token.y) == y)
        {
            _editing.selectedTokenIndex = index;
            _integration.statusMessage = selectedObjectSummary();
            return;
        }
    }

    for (int index = static_cast<int>(_draftMap.features.size()) - 1; index >= 0; --index)
    {
        const MapFeatureModel &feature = _draftMap.features[index];
        if (feature.x == x && feature.y == y)
        {
            _editing.selectedFeatureIndex = index;
            _integration.statusMessage = selectedObjectSummary();
            return;
        }
    }

    for (int index = static_cast<int>(_draftMap.regions.size()) - 1; index >= 0; --index)
    {
        const MapRegionModel &region = _draftMap.regions[index];
        if (x >= region.x && y >= region.y && x < region.x + region.width && y < region.y + region.height)
        {
            _editing.selectedRegionIndex = index;
            _integration.statusMessage = selectedObjectSummary();
            return;
        }
    }

    for (int index = static_cast<int>(_draftMap.routes.size()) - 1; index >= 0; --index)
    {
        const MapRouteModel &route = _draftMap.routes[index];
        if ((route.startX == x && route.startY == y) || (route.endX == x && route.endY == y))
        {
            _editing.selectedRouteIndex = index;
            _integration.statusMessage = selectedObjectSummary();
            return;
        }
    }

    _integration.statusMessage = gm::localization::trUi("Nothing selectable at that cell.");
}

void MapCreatorController::selectTokenObject(int index)
{
    clearObjectSelection();
    if (index >= 0 && index < static_cast<int>(_draftMap.tokens.size()))
        _editing.selectedTokenIndex = index;
    _integration.statusMessage = selectedObjectSummary();
}

void MapCreatorController::selectFeatureObject(int index)
{
    clearObjectSelection();
    if (index >= 0 && index < static_cast<int>(_draftMap.features.size()))
        _editing.selectedFeatureIndex = index;
    _integration.statusMessage = selectedObjectSummary();
}

void MapCreatorController::selectGeneratedFeatureObject(int index)
{
    clearObjectSelection();
    if (index >= 0 && index < static_cast<int>(_draftMap.generation.graph.features.size()))
        _editing.selectedGeneratedFeatureIndex = index;
    _integration.statusMessage = selectedObjectSummary();
}

void MapCreatorController::selectRegionObject(int index)
{
    clearObjectSelection();
    if (index >= 0 && index < static_cast<int>(_draftMap.regions.size()))
        _editing.selectedRegionIndex = index;
    _integration.statusMessage = selectedObjectSummary();
}

void MapCreatorController::selectRouteObject(int index)
{
    clearObjectSelection();
    if (index >= 0 && index < static_cast<int>(_draftMap.routes.size()))
        _editing.selectedRouteIndex = index;
    _integration.statusMessage = selectedObjectSummary();
}

void MapCreatorController::normalizeObjectSelection()
{
    _editing.selectedTokenIndex = clampOptionalIndex(_editing.selectedTokenIndex, static_cast<int>(_draftMap.tokens.size()));
    _editing.selectedFeatureIndex = clampOptionalIndex(_editing.selectedFeatureIndex, static_cast<int>(_draftMap.features.size()));
    _editing.selectedGeneratedFeatureIndex = clampOptionalIndex(
        _editing.selectedGeneratedFeatureIndex,
        static_cast<int>(_draftMap.generation.graph.features.size()));
    _editing.selectedRegionIndex = clampOptionalIndex(_editing.selectedRegionIndex, static_cast<int>(_draftMap.regions.size()));
    _editing.selectedRouteIndex = clampOptionalIndex(_editing.selectedRouteIndex, static_cast<int>(_draftMap.routes.size()));

    const SelectedObjectKind selectedKind = selectedObjectKind();
    if (selectedKind != SelectedObjectKind::Token)
        _editing.selectedTokenIndex = -1;
    if (selectedKind != SelectedObjectKind::Feature)
        _editing.selectedFeatureIndex = -1;
    if (selectedKind != SelectedObjectKind::GeneratedFeature)
        _editing.selectedGeneratedFeatureIndex = -1;
    if (selectedKind != SelectedObjectKind::Region)
        _editing.selectedRegionIndex = -1;
    if (selectedKind != SelectedObjectKind::Route)
        _editing.selectedRouteIndex = -1;
}

void MapCreatorController::clearObjectSelection()
{
    _editing.selectedTokenIndex = -1;
    _editing.selectedRegionIndex = -1;
    _editing.selectedFeatureIndex = -1;
    _editing.selectedGeneratedFeatureIndex = -1;
    _editing.selectedRouteIndex = -1;
}

void MapCreatorController::applyBrushStampAt(int x, int y)
{
    if (!isMapCellInside(_draftMap, x, y))
        return;

    switch (_editing.activeTool)
    {
    case Tool::PaintTile:
    case Tool::EraseTile:
        paintCell(x, y);
        break;
    case Tool::PaintWay:
    case Tool::EraseWay:
        paintWayCell(x, y);
        break;
    case Tool::PaintElevation:
        paintElevationCell(x, y);
        break;
    case Tool::PaintTacticalEdge:
        paintTacticalEdge(x, y);
        break;
    case Tool::Fog:
        fogCell(x, y);
        break;
    default:
        break;
    }
}

void MapCreatorController::beginBrushStroke()
{
    if (_editing.strokeInProgress || !activeToolUsesBrush())
        return;

    // Brush overlays update immediately over the cached GM illustration.
    // Capture before the undo snapshot increments the authoring serial.
    _editing.brushStrokeRenderSerial = _editing.authoringSerial;
    _editing.brushRenderSnapshot.begin(_draftMap);
    _editing.brushStrokePreview.begin(_draftMap);
    _editing.brushProtectedWayCells = (_editing.activeTool == Tool::PaintWay || _editing.activeTool == Tool::EraseWay)
        ? mapProtectedWayBrushCells(_draftMap) : std::vector<MapCellCoordinate>{};
    captureUndoSnapshot(activeToolLabel());
    _editing.wayStrokeParts.clear();
    _editing.strokeInProgress = true;
    _editing.lastBrushX = -1;
    _editing.lastBrushY = -1;
}

void MapCreatorController::endBrushStroke()
{
    if (_editing.strokeInProgress)
    {
        if (_editing.activeTool == Tool::PaintWay && _draftMap.generation.continuousRendering)
        {
            for (const auto &points : _editing.wayStrokeParts)
                if (appendContinuousMapWayStroke(_draftMap, points, activeWayKind(), activeBrushSize()))
                    projectGeneratedWayFeatureToCells(_draftMap, _draftMap.generation.graph.features.back(), false);
            rebuildContinuousMapWayFeaturesFromCells(_draftMap);
        }
        else if (_editing.activeTool == Tool::PaintWay || _editing.activeTool == Tool::EraseWay)
            rebuildContinuousMapWayFeaturesFromCells(_draftMap);
        markDraftEdited();
        _history.crashJournalPending = true;
    }
    _editing.strokeInProgress = false;
    _editing.brushStrokePreview.clear();
    _editing.brushRenderSnapshot.clear();
    _editing.brushProtectedWayCells.clear();
    _editing.wayStrokeParts.clear();
    _editing.lastBrushX = -1;
    _editing.lastBrushY = -1;
}
