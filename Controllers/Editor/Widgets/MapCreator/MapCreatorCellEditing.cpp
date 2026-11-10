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

void MapCreatorController::paintCell(int x, int y)
{
    const std::vector<MapTileKind> palette = getPalette();
    if (_editing.selectedTileIndex < 0 || _editing.selectedTileIndex >= static_cast<int>(palette.size()))
        _editing.selectedTileIndex = 0;

    _editing.brushSize = std::max(1, std::min(_editing.brushSize, 24));
    const std::vector<MapCellCoordinate> footprint = mapBrushFootprint(_draftMap, x, y, _editing.brushSize, _editing.roundBrush);
    applyMapTerrainBrush(_draftMap, footprint, palette[_editing.selectedTileIndex], _editing.rotationQuarterTurns, !_editing.paintMode, activeSymbolVariantMode(), _editing.autoRotateTiles);
    if (_editing.strokeInProgress) _editing.brushStrokePreview.record(footprint);
}

void MapCreatorController::paintWayCell(int x, int y)
{
    const std::vector<MapCellCoordinate> footprint = mapBrushFootprint(_draftMap, x, y, activeBrushSize(), _editing.roundBrush);
    applyMapWayBrush(_draftMap, footprint, activeWayKind(), _editing.activeTool == Tool::EraseWay,
                     _editing.strokeInProgress ? &_editing.brushProtectedWayCells : nullptr);
    if (_editing.activeTool == Tool::EraseWay)
        eraseContinuousMapWayStrokes(_draftMap, footprint);
    if (_editing.strokeInProgress) _editing.brushStrokePreview.record(footprint);
}

void MapCreatorController::paintElevationCell(int x, int y)
{
    _editing.brushSize = std::clamp(_editing.brushSize, 1, 24);
    const std::vector<MapCellCoordinate> footprint = mapBrushFootprint(_draftMap, x, y, _editing.brushSize, _editing.roundBrush);
    applyMapElevationBrush(_draftMap, footprint, _editing.selectedElevation);
    if (_editing.strokeInProgress) _editing.brushStrokePreview.record(footprint);
}

void MapCreatorController::paintTacticalEdge(int x, int y)
{
    _editing.brushSize = std::clamp(_editing.brushSize, 1, 24);
    const MapTacticalPreset preset = static_cast<MapTacticalPreset>(std::clamp(
        _editing.selectedTacticalPresetIndex,
        0,
        static_cast<int>(MapTacticalPreset::ClearEdge)));
    const bool blockMovement = _editing.selectedTacticalChannelIndex != 1;
    const bool blockLineOfSight = _editing.selectedTacticalChannelIndex != 0;
    for (const MapCellCoordinate &coordinate : mapBrushFootprint(_draftMap, x, y, _editing.brushSize, _editing.roundBrush))
    {
        applyMapTacticalPreset(
            _draftMap,
            coordinate.x,
            coordinate.y,
            _editing.rotationQuarterTurns,
            preset,
            blockMovement,
            blockLineOfSight,
            _editing.tacticalOneWay);
    }
}

void MapCreatorController::fogCell(int x, int y)
{
    _editing.brushSize = std::max(1, std::min(_editing.brushSize, 24));
    for (const MapCellCoordinate &coordinate : mapBrushFootprint(_draftMap, x, y, _editing.brushSize, _editing.roundBrush))
        if (MapCellModel *cell = mapCellAt(_draftMap, coordinate.x, coordinate.y))
            cell->fogged = !_editing.revealFog;
}

void MapCreatorController::configureCell(int x, int y)
{
    MapCellModel *cell = mapCellAt(_draftMap, x, y);
    if (!cell)
        return;

    cell->movementCost = std::max(1, _editing.brushSize);
    cell->difficultTerrain = _editing.paintMode;
    cell->blocksLineOfSight = !_editing.paintMode;
    cell->movementBlockedMask = _editing.paintMode ? 0 : 0x0f;
    cell->lineOfSightBlockedMask = _editing.paintMode ? 0 : 0x0f;
    cell->lighting = static_cast<MapLightingMode>(std::clamp(_editing.selectedLightingIndex, 0, 3));
    cell->fogged = true;
}

void MapCreatorController::addLayer()
{
    captureUndoSnapshot("Add Layer");
    MapLayerModel layer;
    layer.name = std::string(gm::localization::trUi("Layer")) + " " + std::to_string(_draftMap.layers.size() + 1);
    layer.sortOrder = static_cast<int>(_draftMap.layers.size());
    _draftMap.layers.push_back(layer);
    _editing.selectedLayerIndex = static_cast<int>(_draftMap.layers.size()) - 1;
}

void MapCreatorController::removeSelectedLayer()
{
    if (_editing.selectedLayerIndex <= 0 || _editing.selectedLayerIndex >= static_cast<int>(_draftMap.layers.size()))
        return;

    captureUndoSnapshot("Remove Layer");
    const EntityId removedId = _draftMap.layers[_editing.selectedLayerIndex].metadata.id;
    _draftMap.layers.erase(_draftMap.layers.begin() + _editing.selectedLayerIndex);
    for (MapTokenModel &token : _draftMap.tokens)
    {
        if (token.layerId == removedId)
            token.layerId.clear();
    }
    _editing.selectedLayerIndex = std::min(_editing.selectedLayerIndex, static_cast<int>(_draftMap.layers.size()) - 1);
}

void MapCreatorController::addTokenAt(int x, int y)
{
    if (!isMapCellInside(_draftMap, x, y))
        return;

    captureUndoSnapshot("Place Token");
    ensureDefaultMapLayer(_draftMap);
    clearObjectSelection();
    MapTokenModel token;
    token.kind = static_cast<MapTokenKind>(std::clamp(_editing.selectedTokenKindIndex, 0, static_cast<int>(MapTokenKind::Party)));
    token.name = std::string(mapTokenKindLabel(token.kind)) + " Token";
    token.x = static_cast<float>(x);
    token.y = static_cast<float>(y);
    token.layerId = _draftMap.layers[std::clamp(_editing.selectedLayerIndex, 0, static_cast<int>(_draftMap.layers.size()) - 1)].metadata.id;
    setTokenLinkFromSelection(token);
    _draftMap.tokens.push_back(token);
    _editing.selectedTokenIndex = static_cast<int>(_draftMap.tokens.size()) - 1;
}

void MapCreatorController::removeSelectedToken()
{
    if (_editing.selectedTokenIndex < 0 || _editing.selectedTokenIndex >= static_cast<int>(_draftMap.tokens.size()))
        return;
    captureUndoSnapshot("Remove Token");
    _draftMap.tokens.erase(_draftMap.tokens.begin() + _editing.selectedTokenIndex);
    _editing.selectedTokenIndex = std::min(_editing.selectedTokenIndex, static_cast<int>(_draftMap.tokens.size()) - 1);
}

void MapCreatorController::addRegionAt(int x, int y)
{
    if (!isMapCellInside(_draftMap, x, y))
        return;

    captureUndoSnapshot("Add Region");
    clearObjectSelection();
    MapRegionModel region;
    region.name = std::string(gm::localization::trUi("Region")) + " " + std::to_string(_draftMap.regions.size() + 1);
    region.x = std::clamp(x, 0, _draftMap.width - 1);
    region.y = std::clamp(y, 0, _draftMap.height - 1);
    region.width = std::min(4, _draftMap.width - region.x);
    region.height = std::min(3, _draftMap.height - region.y);
    region.lighting = static_cast<MapLightingMode>(std::clamp(_editing.selectedLightingIndex, 0, 3));
    _draftMap.regions.push_back(region);
    _editing.selectedRegionIndex = static_cast<int>(_draftMap.regions.size()) - 1;
}

void MapCreatorController::removeSelectedRegion()
{
    if (_editing.selectedRegionIndex < 0 || _editing.selectedRegionIndex >= static_cast<int>(_draftMap.regions.size()))
        return;
    captureUndoSnapshot("Remove Region");
    _draftMap.regions.erase(_draftMap.regions.begin() + _editing.selectedRegionIndex);
    _editing.selectedRegionIndex = std::min(_editing.selectedRegionIndex, static_cast<int>(_draftMap.regions.size()) - 1);
}

void MapCreatorController::addFeatureAt(int x, int y)
{
    if (!isMapCellInside(_draftMap, x, y))
        return;

    captureUndoSnapshot("Add Feature");
    clearObjectSelection();
    MapFeatureModel feature;
    feature.kind = static_cast<MapFeatureKind>(std::clamp(_editing.selectedFeatureKindIndex, 0, static_cast<int>(MapFeatureKind::Pin)));
    feature.name = mapFeatureKindLabel(feature.kind);
    feature.x = std::clamp(x, 0, _draftMap.width - 1);
    feature.y = std::clamp(y, 0, _draftMap.height - 1);
    feature.gmOnly = feature.kind == MapFeatureKind::SecretDoor || feature.kind == MapFeatureKind::Trap;
    _draftMap.features.push_back(feature);
    _editing.selectedFeatureIndex = static_cast<int>(_draftMap.features.size()) - 1;
}

void MapCreatorController::removeSelectedFeature()
{
    if (_editing.selectedFeatureIndex < 0 || _editing.selectedFeatureIndex >= static_cast<int>(_draftMap.features.size()))
        return;
    captureUndoSnapshot("Remove Feature");
    _draftMap.features.erase(_draftMap.features.begin() + _editing.selectedFeatureIndex);
    _editing.selectedFeatureIndex = std::min(_editing.selectedFeatureIndex, static_cast<int>(_draftMap.features.size()) - 1);
}

void MapCreatorController::addRouteAt(int x, int y)
{
    if (!isMapCellInside(_draftMap, x, y))
        return;

    captureUndoSnapshot("Add Route");
    clearObjectSelection();
    MapRouteModel route;
    route.name = std::string(gm::localization::trUi("Route")) + " " + std::to_string(_draftMap.routes.size() + 1);
    route.fromMapId = _draftMap.metadata.id;
    route.startX = std::clamp(x, 0, _draftMap.width - 1);
    route.startY = std::clamp(y, 0, _draftMap.height - 1);
    route.endX = std::clamp(route.startX + 4, 0, _draftMap.width - 1);
    if (route.endX == route.startX && _draftMap.width > 1)
        route.endX = std::max(0, route.startX - 4);
    route.endY = route.startY;
    route.distanceMiles = static_cast<float>(std::max(1, std::abs(route.endX - route.startX)));
    route.terrain = "Road";
    _draftMap.routes.push_back(route);
    _editing.selectedRouteIndex = static_cast<int>(_draftMap.routes.size()) - 1;
}

void MapCreatorController::addRoute()
{
    captureUndoSnapshot("Add Route");
    clearObjectSelection();
    MapRouteModel route;
    route.name = std::string(gm::localization::trUi("Route")) + " " + std::to_string(_draftMap.routes.size() + 1);
    route.fromMapId = _draftMap.metadata.id;
    route.startX = 0;
    route.startY = _draftMap.height / 2;
    route.endX = _draftMap.width - 1;
    route.endY = _draftMap.height / 2;
    route.distanceMiles = static_cast<float>(std::max(1, _draftMap.width / 2));
    route.terrain = "Road";
    _draftMap.routes.push_back(route);
    _editing.selectedRouteIndex = static_cast<int>(_draftMap.routes.size()) - 1;
}

void MapCreatorController::removeSelectedRoute()
{
    if (_editing.selectedRouteIndex < 0 || _editing.selectedRouteIndex >= static_cast<int>(_draftMap.routes.size()))
        return;
    captureUndoSnapshot("Remove Route");
    _draftMap.routes.erase(_draftMap.routes.begin() + _editing.selectedRouteIndex);
    _editing.selectedRouteIndex = std::min(_editing.selectedRouteIndex, static_cast<int>(_draftMap.routes.size()) - 1);
}
