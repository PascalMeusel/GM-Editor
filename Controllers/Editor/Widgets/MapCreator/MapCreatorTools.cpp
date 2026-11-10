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

void MapCreatorController::setActiveTool(Tool tool)
{
    if (_editing.activeTool != tool)
        endBrushStroke();
    _editing.activeTool = tool;
    _editing.paintMode = tool != Tool::EraseTile;
}

void MapCreatorController::cancelActiveTool()
{
    endBrushStroke();
    setActiveTool(Tool::InspectSelect);
    _integration.statusMessage = gm::localization::trUi("Map tool cancelled. Inspect/select is active.");
}

void MapCreatorController::selectTileFromLegend(int tileIndex)
{
    const std::vector<MapTileKind> palette = getPalette();
    if (tileIndex < 0 || tileIndex >= static_cast<int>(palette.size()))
        return;

    _editing.selectedTileIndex = tileIndex;
    setActiveTool(Tool::PaintTile);
    _integration.statusMessage = std::string(gm::localization::trUi("Selected tile:")) + " " +
                     gm::localization::trUi(mapTileKindLabel(palette[_editing.selectedTileIndex]));
}

void MapCreatorController::applyActiveToolAt(int x, int y, const MapFeaturePoint *strokePoint)
{
    if (_generation.inProgress)
        return;

    if (!isMapCellInside(_draftMap, x, y))
        return;

    if (activeToolUsesBrush())
    {
        if (_editing.strokeInProgress && _editing.activeTool == Tool::PaintWay && _draftMap.generation.continuousRendering)
        {
            if (_editing.wayStrokeParts.empty() || _editing.lastBrushX < 0)
                _editing.wayStrokeParts.emplace_back();
            auto &points = _editing.wayStrokeParts.back();
            const MapFeaturePoint point = strokePoint ? *strokePoint : MapFeaturePoint{x + .5f, y + .5f};
            if (points.empty() || mapFeaturePointDistanceSquared(points.back(), point) > .01f)
                points.push_back(point);
        }
        if (_editing.strokeInProgress && _editing.lastBrushX == x && _editing.lastBrushY == y)
            return;
        if (_editing.strokeInProgress && _editing.lastBrushX >= 0 && _editing.lastBrushY >= 0)
        {
            int currentX = _editing.lastBrushX;
            int currentY = _editing.lastBrushY;
            const int dx = std::abs(x - currentX);
            const int sx = currentX < x ? 1 : -1;
            const int dy = -std::abs(y - currentY);
            const int sy = currentY < y ? 1 : -1;
            int error = dx + dy;

            for (;;)
            {
                applyBrushStampAt(currentX, currentY);
                if (currentX == x && currentY == y)
                    break;
                const int doubledError = 2 * error;
                if (doubledError >= dy)
                {
                    error += dy;
                    currentX += sx;
                }
                if (doubledError <= dx)
                {
                    error += dx;
                    currentY += sy;
                }
            }
        }
        else
        {
            applyBrushStampAt(x, y);
        }
        _editing.lastBrushX = x;
        _editing.lastBrushY = y;
        markDraftEdited(); // Keep actual dirty/player state current; GM art is cached during the stroke.
        _history.crashJournalPending = true;
        return;
    }

    switch (_editing.activeTool)
    {
    case Tool::InspectSelect:
        selectObjectAt(x, y);
        break;
    case Tool::PlaceToken:
        addTokenAt(x, y);
        break;
    case Tool::AddFeature:
        addFeatureAt(x, y);
        break;
    case Tool::AddRegion:
        addRegionAt(x, y);
        break;
    case Tool::AddRoute:
        addRouteAt(x, y);
        break;
    case Tool::PaintTile:
    case Tool::EraseTile:
    case Tool::PaintWay:
    case Tool::EraseWay:
    case Tool::PaintElevation:
    case Tool::PaintTacticalEdge:
    case Tool::Fog:
        break;
    }
}

bool MapCreatorController::activeToolUsesBrush() const
{
    return _editing.activeTool == Tool::PaintTile || _editing.activeTool == Tool::EraseTile || _editing.activeTool == Tool::PaintWay || _editing.activeTool == Tool::EraseWay ||
           _editing.activeTool == Tool::PaintElevation || _editing.activeTool == Tool::PaintTacticalEdge || _editing.activeTool == Tool::Fog;
}

int MapCreatorController::activeBrushSize() const
{
    return _editing.activeTool == Tool::PaintWay && _draftMap.generation.continuousRendering
        ? _editing.strokeWidths.value(activeWayKind()) : std::clamp(_editing.brushSize, 1, 24);
}

const char *MapCreatorController::activeToolLabel() const
{
    switch (_editing.activeTool)
    {
    case Tool::InspectSelect:
        return gm::localization::trUi("Inspect / Select");
    case Tool::PaintTile:
        return gm::localization::trUi("Paint Tile");
    case Tool::EraseTile:
        return gm::localization::trUi("Erase");
    case Tool::PaintWay:
        return gm::localization::trUi("Paint Way");
    case Tool::EraseWay:
        return gm::localization::trUi("Erase Way");
    case Tool::PaintElevation:
        return gm::localization::trUi("Paint Elevation");
    case Tool::PaintTacticalEdge:
        return gm::localization::trUi("Paint Tactical Edge");
    case Tool::PlaceToken:
        return gm::localization::trUi("Place Token");
    case Tool::AddFeature:
        return gm::localization::trUi("Add Feature");
    case Tool::AddRegion:
        return gm::localization::trUi("Add Region");
    case Tool::AddRoute:
        return gm::localization::trUi("Add Route");
    case Tool::Fog:
        return gm::localization::trUi("Fog");
    }
    return gm::localization::trUi("Inspect / Select");
}

const char *MapCreatorController::nextActionHint() const
{
    switch (_editing.activeTool)
    {
    case Tool::InspectSelect:
        return gm::localization::trUi("Left click selects an existing token, feature, region, or route.");
    case Tool::PaintTile:
        return gm::localization::trUi("Left click or drag paints. Right click returns to Select.");
    case Tool::EraseTile:
        return gm::localization::trUi("Left click or drag erases cells. Right click returns to Select.");
    case Tool::PaintWay:
        return gm::localization::trUi("Left click or drag draws the selected connected way overlay.");
    case Tool::EraseWay:
        return gm::localization::trUi("Left click or drag clears way overlays without changing terrain.");
    case Tool::PaintElevation:
        return gm::localization::trUi("Left click or drag sets an exact elevation on each cell.");
    case Tool::PaintTacticalEdge:
        return gm::localization::trUi("Left click or drag paints the chosen preset on the selected cell edge.");
    case Tool::PlaceToken:
        return gm::localization::trUi("Left click places a token. Right click cancels this tool.");
    case Tool::AddFeature:
        return gm::localization::trUi("Left click adds the chosen feature. Right click cancels this tool.");
    case Tool::AddRegion:
        return gm::localization::trUi("Left-drag draws a region. Drag its edges to resize. Right click cancels.");
    case Tool::AddRoute:
        return gm::localization::trUi("Left click starts a route from that cell. Right click cancels this tool.");
    case Tool::Fog:
        return gm::localization::trUi("Left click or drag adds fog. Right click returns to Select.");
    }
    return gm::localization::trUi("Choose a tool, then use the map canvas.");
}

const char *MapCreatorController::toolHelpText() const
{
    return gm::localization::trUi("Mouse wheel zooms the hovered map canvas. Shortcuts still work: Ctrl places a token, Shift adds a feature, Alt adds a region and temporarily shows tactical overlays.");
}

std::string MapCreatorController::activeSelectionSummary() const
{
    const auto ui = [](const char *text) { return std::string(gm::localization::trUi(text)); };
    switch (_editing.activeTool)
    {
    case Tool::InspectSelect:
        return selectedObjectSummary();
    case Tool::PaintTile:
        return ui("Tile:") + " " + ui(mapTileKindLabel(activeTileKind())) + ", " + ui("brush") + " " + std::to_string(_editing.brushSize);
    case Tool::EraseTile:
        return ui("Tile:") + " " + ui("Empty") + ", " + ui("brush") + " " + std::to_string(_editing.brushSize);
    case Tool::PaintWay:
        return ui("Way:") + " " + ui(mapWayKindLabel(activeWayKind())) + ", " + ui("brush") + " " + std::to_string(activeBrushSize());
    case Tool::EraseWay:
        return ui("Way:") + " " + ui("None") + ", " + ui("brush") + " " + std::to_string(_editing.brushSize);
    case Tool::PaintElevation:
        return ui("Elevation:") + " " + std::to_string(_editing.selectedElevation) + ", " + ui("brush") + " " + std::to_string(_editing.brushSize);
    case Tool::PaintTacticalEdge:
    {
        const MapTacticalPreset preset = static_cast<MapTacticalPreset>(std::clamp(
            _editing.selectedTacticalPresetIndex,
            0,
            static_cast<int>(MapTacticalPreset::ClearEdge)));
        static const char *directions[] = {"north", "east", "south", "west"};
        return ui(mapTacticalPresetLabel(preset)) + ", " + ui(directions[normalizeMapRotation(_editing.rotationQuarterTurns)]) + " " + ui("edge");
    }
    case Tool::PlaceToken:
    {
        const MapTokenKind tokenKind = static_cast<MapTokenKind>(std::clamp(_editing.selectedTokenKindIndex, 0, static_cast<int>(MapTokenKind::Party)));
        std::string summary = ui("Token:") + " " + ui(mapTokenKindLabel(tokenKind));
        const std::vector<const char *> labels = getLinkedEntityLabels();
        if (_editing.selectedLinkedEntityIndex > 0 && _editing.selectedLinkedEntityIndex < static_cast<int>(labels.size()))
            summary += ", " + ui("link:") + " " + labels[_editing.selectedLinkedEntityIndex];
        return summary;
    }
    case Tool::AddFeature:
    {
        const MapFeatureKind featureKind = static_cast<MapFeatureKind>(std::clamp(_editing.selectedFeatureKindIndex, 0, static_cast<int>(MapFeatureKind::Pin)));
        return ui("Feature:") + " " + ui(mapFeatureKindLabel(featureKind));
    }
    case Tool::AddRegion:
        return ui("Region:") + " " + ui("drag to draw an area");
    case Tool::AddRoute:
        return ui("Route:") + " " + ui("clicked start cell");
    case Tool::Fog:
        return ui("Fog brush") + " " + std::to_string(_editing.brushSize);
    }
    return selectedObjectSummary();
}

std::string MapCreatorController::selectedObjectSummary() const
{
    if (_editing.selectedTokenIndex >= 0 && _editing.selectedTokenIndex < static_cast<int>(_draftMap.tokens.size()))
    {
        const MapTokenModel &token = _draftMap.tokens[_editing.selectedTokenIndex];
        return std::string(gm::localization::trUi("Token:")) + " " +
               (token.name.empty() ? gm::localization::trUi(mapTokenKindLabel(token.kind)) : token.name);
    }
    if (_editing.selectedFeatureIndex >= 0 && _editing.selectedFeatureIndex < static_cast<int>(_draftMap.features.size()))
    {
        const MapFeatureModel &feature = _draftMap.features[_editing.selectedFeatureIndex];
        return std::string(gm::localization::trUi("Feature:")) + " " +
               (feature.name.empty() ? gm::localization::trUi(mapFeatureKindLabel(feature.kind)) : feature.name);
    }
    if (_editing.selectedGeneratedFeatureIndex >= 0 &&
        _editing.selectedGeneratedFeatureIndex < static_cast<int>(_draftMap.generation.graph.features.size()))
    {
        const MapGeneratedFeature &feature =
            _draftMap.generation.graph.features[static_cast<std::size_t>(_editing.selectedGeneratedFeatureIndex)];
        return std::string(gm::localization::trUi("Generated feature:")) + " " +
               (feature.name.empty() ? gm::localization::trUi("Unnamed feature") : feature.name);
    }
    if (_editing.selectedRegionIndex >= 0 && _editing.selectedRegionIndex < static_cast<int>(_draftMap.regions.size()))
    {
        const MapRegionModel &region = _draftMap.regions[_editing.selectedRegionIndex];
        return std::string(gm::localization::trUi("Region:")) + " " +
               (region.name.empty() ? gm::localization::trUi("<Region>") : region.name);
    }
    if (_editing.selectedRouteIndex >= 0 && _editing.selectedRouteIndex < static_cast<int>(_draftMap.routes.size()))
    {
        const MapRouteModel &route = _draftMap.routes[_editing.selectedRouteIndex];
        return std::string(gm::localization::trUi("Route:")) + " " +
               (route.name.empty() ? gm::localization::trUi("<Route>") : route.name);
    }
    return gm::localization::trUi("No object selected");
}

MapCreatorController::SelectedObjectKind MapCreatorController::selectedObjectKind() const
{
    if (_editing.selectedTokenIndex >= 0 && _editing.selectedTokenIndex < static_cast<int>(_draftMap.tokens.size()))
        return SelectedObjectKind::Token;
    if (_editing.selectedFeatureIndex >= 0 && _editing.selectedFeatureIndex < static_cast<int>(_draftMap.features.size()))
        return SelectedObjectKind::Feature;
    if (_editing.selectedGeneratedFeatureIndex >= 0 &&
        _editing.selectedGeneratedFeatureIndex < static_cast<int>(_draftMap.generation.graph.features.size()))
        return SelectedObjectKind::GeneratedFeature;
    if (_editing.selectedRegionIndex >= 0 && _editing.selectedRegionIndex < static_cast<int>(_draftMap.regions.size()))
        return SelectedObjectKind::Region;
    if (_editing.selectedRouteIndex >= 0 && _editing.selectedRouteIndex < static_cast<int>(_draftMap.routes.size()))
        return SelectedObjectKind::Route;
    return SelectedObjectKind::None;
}

bool MapCreatorController::hasSelectedObject() const
{
    return selectedObjectKind() != SelectedObjectKind::None;
}

MapTileKind MapCreatorController::activeTileKind() const
{
    const std::vector<MapTileKind> palette = getPalette();
    if (palette.empty())
        return MapTileKind::Empty;
    const int selectedIndex = std::clamp(_editing.selectedTileIndex, 0, static_cast<int>(palette.size()) - 1);
    return palette[selectedIndex];
}

MapWayKind MapCreatorController::activeWayKind() const
{
    const std::vector<MapWayKind> &palette = wayPalette();
    if (palette.empty())
        return MapWayKind::DirtRoad;
    const int selectedIndex = std::clamp(_editing.selectedWayIndex, 0, static_cast<int>(palette.size()) - 1);
    return palette[selectedIndex];
}

MapSymbolVariantMode MapCreatorController::activeSymbolVariantMode() const
{
    return normalizeMapSymbolVariantMode(_generation.selectedSymbolVariantModeIndex);
}
