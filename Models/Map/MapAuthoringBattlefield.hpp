#pragma once

#include "Models/Map/MapAuthoringBrushes.hpp"

inline void setMapCellEdgeBit(int &mask, int edge, bool enabled)
{
    mask = enabled ? normalizeMapEdgeMask(mask | edge) : normalizeMapEdgeMask(mask & ~edge);
}

inline void clearMapTacticalEdge(MapCellModel &cell, int edge)
{
    setMapCellEdgeBit(cell.movementBlockedMask, edge, false);
    setMapCellEdgeBit(cell.lineOfSightBlockedMask, edge, false);
    setMapCellEdgeBit(cell.movementOneWayBlockedMask, edge, false);
    setMapCellEdgeBit(cell.lineOfSightOneWayBlockedMask, edge, false);
}

inline void applyMapElevationBrush(MapModel &map, const std::vector<MapCellCoordinate> &footprint, int elevation)
{
    elevation = std::clamp(elevation, -20, 20);
    for (const MapCellCoordinate &coordinate : footprint)
    {
        if (MapCellModel *cell = mapCellAt(map, coordinate.x, coordinate.y))
            cell->elevation = elevation;
    }
}

inline void applyMapTacticalPreset(
    MapModel &map,
    int x,
    int y,
    int directionQuarterTurns,
    MapTacticalPreset preset,
    bool customBlockMovement = true,
    bool customBlockLineOfSight = true,
    bool customOneWay = false)
{
    MapCellModel *owner = mapCellAt(map, x, y);
    if (!owner)
        return;

    directionQuarterTurns = normalizeMapRotation(directionQuarterTurns);
    const int ownerEdge = mapCardinalEdgeMask(directionQuarterTurns);
    const int neighborEdge = oppositeMapCardinalEdgeMask(directionQuarterTurns);
    MapCellModel *neighbor = mapCellAt(
        map,
        x + mapCardinalEdgeDeltaX(directionQuarterTurns),
        y + mapCardinalEdgeDeltaY(directionQuarterTurns));

    // Repainting an edge is deterministic: stale symmetric and directional
    // metadata is removed before applying the selected preset.
    clearMapTacticalEdge(*owner, ownerEdge);
    if (neighbor)
        clearMapTacticalEdge(*neighbor, neighborEdge);

    bool blockMovement = false;
    bool blockLineOfSight = false;
    bool oneWay = false;
    int elevationDelta = 0;
    switch (preset)
    {
    case MapTacticalPreset::CustomBlocker:
        blockMovement = customBlockMovement;
        blockLineOfSight = customBlockLineOfSight;
        oneWay = customOneWay;
        break;
    case MapTacticalPreset::Wall:
    case MapTacticalPreset::ClosedDoor:
    case MapTacticalPreset::TreeTrunk:
        blockMovement = true;
        blockLineOfSight = true;
        break;
    case MapTacticalPreset::Ledge:
        blockMovement = true;
        oneWay = true;
        elevationDelta = 1;
        break;
    case MapTacticalPreset::Window:
    case MapTacticalPreset::LowCover:
        blockMovement = true;
        break;
    case MapTacticalPreset::StairsUp:
    case MapTacticalPreset::RampUp:
        elevationDelta = 1;
        break;
    case MapTacticalPreset::OpenDoor:
    case MapTacticalPreset::ClearEdge:
        break;
    }

    auto applyBlocker = [&](int &symmetricOwner, int &oneWayOwner, int MapCellModel::*symmetricMember, bool enabled) {
        if (!enabled)
            return;
        if (oneWay || !neighbor)
        {
            setMapCellEdgeBit(oneWayOwner, ownerEdge, true);
            return;
        }
        setMapCellEdgeBit(symmetricOwner, ownerEdge, true);
        setMapCellEdgeBit(neighbor->*symmetricMember, neighborEdge, true);
    };
    applyBlocker(owner->movementBlockedMask, owner->movementOneWayBlockedMask, &MapCellModel::movementBlockedMask, blockMovement);
    applyBlocker(owner->lineOfSightBlockedMask, owner->lineOfSightOneWayBlockedMask, &MapCellModel::lineOfSightBlockedMask, blockLineOfSight);

    if (neighbor && elevationDelta != 0)
        neighbor->elevation = std::clamp(owner->elevation + elevationDelta, -20, 20);
}

inline std::vector<MapCellCoordinate> battlefieldBrushFootprint(const EncounterModel &battlefield, int centerX, int centerY, int brushSize, bool round = false)
{
    std::vector<MapCellCoordinate> cells;
    brushSize = std::max(1, brushSize);
    const int start = -(brushSize / 2);
    const int end = start + brushSize - 1;
    const float midpoint = (start + end) * 0.5f;
    const float radiusSquared = brushSize * brushSize * 0.25f;
    for (int y = centerY + start; y <= centerY + end; ++y)
    {
        for (int x = centerX + start; x <= centerX + end; ++x)
        {
            if (isEncounterCellInside(battlefield, x, y))
                cells.push_back({x, y});
        }
    }
    return normalizeMapCellCoordinates(cells);
}

inline std::vector<MapCellCoordinate> battlefieldTerrainDirtyCellsForFootprint(const EncounterModel &battlefield, const std::vector<MapCellCoordinate> &footprint)
{
    std::vector<MapCellCoordinate> cells;
    for (const MapCellCoordinate &cell : footprint)
    {
        if (isEncounterCellInside(battlefield, cell.x, cell.y))
            cells.push_back(cell);
        if (isEncounterCellInside(battlefield, cell.x, cell.y - 1))
            cells.push_back({cell.x, cell.y - 1});
        if (isEncounterCellInside(battlefield, cell.x + 1, cell.y))
            cells.push_back({cell.x + 1, cell.y});
        if (isEncounterCellInside(battlefield, cell.x, cell.y + 1))
            cells.push_back({cell.x, cell.y + 1});
        if (isEncounterCellInside(battlefield, cell.x - 1, cell.y))
            cells.push_back({cell.x - 1, cell.y});
    }
    return normalizeMapCellCoordinates(cells);
}

inline void applyBattlefieldTerrainBrush(
    EncounterModel &battlefield,
    const std::vector<MapCellCoordinate> &footprint,
    MapTileKind kind)
{
    for (const MapCellCoordinate &coordinate : footprint)
    {
        MapCellModel *cell = encounterCellAt(battlefield, coordinate.x, coordinate.y);
        if (cell)
            applyMapCellTerrainDefaults(*cell, kind);
    }
}

inline EncounterModel previewBattlefieldTerrainBrush(
    const EncounterModel &battlefield,
    const std::vector<MapCellCoordinate> &footprint,
    MapTileKind kind)
{
    EncounterModel preview = battlefield;
    applyBattlefieldTerrainBrush(preview, footprint, kind);
    return preview;
}
