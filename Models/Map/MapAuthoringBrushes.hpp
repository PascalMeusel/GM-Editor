#pragma once

#include "Models/Map/MapAuthoringGeometryCommit.hpp"

inline std::vector<MapCellCoordinate> mapBrushFootprint(const MapModel &map, int centerX, int centerY, int brushSize, bool round = false)
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
            const float dx = x - centerX - midpoint;
            const float dy = y - centerY - midpoint;
            if (!round || dx * dx + dy * dy <= radiusSquared)
                addUniqueMapCellCoordinate(cells, map, x, y);
        }
    }
    return normalizeMapCellCoordinates(cells);
}

inline std::vector<MapCellCoordinate> mapTerrainDirtyCellsForFootprint(const MapModel &map, const std::vector<MapCellCoordinate> &footprint)
{
    std::vector<MapCellCoordinate> cells;
    for (const MapCellCoordinate &cell : footprint)
    {
        addUniqueMapCellCoordinate(cells, map, cell.x, cell.y);
        addUniqueMapCellCoordinate(cells, map, cell.x, cell.y - 1);
        addUniqueMapCellCoordinate(cells, map, cell.x + 1, cell.y);
        addUniqueMapCellCoordinate(cells, map, cell.x, cell.y + 1);
        addUniqueMapCellCoordinate(cells, map, cell.x - 1, cell.y);
    }
    return normalizeMapCellCoordinates(cells);
}

inline std::vector<MapCellCoordinate> mapWayDirtyCellsForFootprint(const MapModel &map, const std::vector<MapCellCoordinate> &footprint)
{
    std::vector<MapCellCoordinate> cells;
    for (const MapCellCoordinate &cell : footprint)
    {
        for (int y = cell.y - 1; y <= cell.y + 1; ++y)
        {
            for (int x = cell.x - 1; x <= cell.x + 1; ++x)
                addUniqueMapCellCoordinate(cells, map, x, y);
        }
    }
    return normalizeMapCellCoordinates(cells);
}

inline bool mapCellCoordinateInList(const std::vector<MapCellCoordinate> &cells, int x, int y)
{
    return std::find(cells.begin(), cells.end(), MapCellCoordinate{x, y}) != cells.end();
}

inline void applyMapTerrainBrush(
    MapModel &map,
    const std::vector<MapCellCoordinate> &footprint,
    MapTileKind kind,
    int rotationQuarterTurns,
    bool erase,
    MapSymbolVariantMode symbolVariantMode = MapSymbolVariantMode::Auto,
    bool autoRotate = true)
{
    for (const MapCellCoordinate &coordinate : footprint)
    {
        MapCellModel *cell = mapCellAt(map, coordinate.x, coordinate.y);
        if (!cell)
            continue;

        if (erase)
        {
            // Terrain erasure is not a reveal action. Fog and authored
            // lighting belong to their own tools and must survive clearing
            // the terrain, including in the live brush preview.
            const bool fogged=cell->fogged;
            const auto lighting=cell->lighting;
            *cell = MapCellModel{};
            cell->fogged=fogged;
            cell->lighting=lighting;
        }
        else
        {
            applyMapCellTerrainDefaults(*cell, kind, autoRotate ? 0 : rotationQuarterTurns);
            // Water terrain is a lake/coast, not the raster underlay of an
            // older river. Keep that distinction when painting across ways.
            if(kind==MapTileKind::Water && map.generation.continuousRendering &&
               (map.atlasLevel==MapLevel::World || map.atlasLevel==MapLevel::Region))
                cell->wayKind=MapWayKind::None;
            cell->symbolVariantMode = symbolVariantMode;
            cell->rotationLocked = !autoRotate;
        }
    }
}

inline MapModel previewMapTerrainBrush(
    const MapModel &map,
    const std::vector<MapCellCoordinate> &footprint,
    MapTileKind kind,
    int rotationQuarterTurns,
    bool erase,
    MapSymbolVariantMode symbolVariantMode = MapSymbolVariantMode::Auto,
    bool autoRotate = true)
{
    MapModel preview = map;
    applyMapTerrainBrush(preview, footprint, kind, rotationQuarterTurns, erase, symbolVariantMode, autoRotate);
    return preview;
}

inline bool mapAuthoringWayKindIsRoadLike(MapWayKind kind)
{
    return kind == MapWayKind::Trail || kind == MapWayKind::DirtRoad || kind == MapWayKind::StoneRoad;
}

inline std::vector<MapCellCoordinate> mapProtectedWayBrushCells(const MapModel &map)
{
    std::vector<MapCellCoordinate> protectedCells;
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if (!mapGeneratedFeatureIsWayOrBridge(feature) ||
            ((mapGeneratedFeatureIsGridDerivedManualWay(feature) || feature.id.rfind("manual_stroke_", 0) == 0) &&
             !mapGeneratedFeatureIsLocked(map, feature)))
            continue;
        const std::vector<MapCellCoordinate> featureCells =
            mapGeneratedFeatureRasterCells(map, feature);
        protectedCells.insert(protectedCells.end(), featureCells.begin(), featureCells.end());
    }
    return normalizeMapCellCoordinates(std::move(protectedCells));
}

inline void applyMapWayBrush(
    MapModel &map,
    const std::vector<MapCellCoordinate> &footprint,
    MapWayKind wayKind,
    bool erase,
    const std::vector<MapCellCoordinate> *strokeProtectedCells = nullptr)
{
    // Protected generated geometry cannot change during one brush stroke.
    // Interactive callers resolve it once; isolated edits retain the same rules.
    const auto resolved = strokeProtectedCells ? std::vector<MapCellCoordinate>{} : mapProtectedWayBrushCells(map);
    const auto &protectedCells = strokeProtectedCells ? *strokeProtectedCells : resolved;

    for (const MapCellCoordinate &coordinate : footprint)
    {
        if (std::binary_search(protectedCells.begin(), protectedCells.end(), coordinate))
            continue;
        MapCellModel *cell = mapCellAt(map, coordinate.x, coordinate.y);
        if (!cell)
            continue;

        if (erase)
        {
            // Restore projected river land while its River marker still
            // distinguishes it from untouched standing water. Clearing the
            // marker first loses that distinction for the stroke eraser.
            if (map.generation.continuousRendering &&
                (map.atlasLevel==MapLevel::World || map.atlasLevel==MapLevel::Region) &&
                cell->kind==MapTileKind::Water &&
                (cell->wayKind==MapWayKind::River || cell->wayKind==MapWayKind::Canal))
            {
                auto defaults=*cell;
                applyMapCellTerrainDefaults(defaults,mapVisualGroundKind(map,coordinate.x,coordinate.y));
                cell->kind=defaults.kind;cell->movementCost=defaults.movementCost;
                cell->difficultTerrain=defaults.difficultTerrain;cell->blocksLineOfSight=defaults.blocksLineOfSight;
            }
            cell->wayKind = MapWayKind::None;
            continue;
        }

        if (mapAuthoringPreservesStandingWater(map,*cell,wayKind)) continue;
        if(mapAuthoringWayKindIsRoadLike(wayKind) && map.generation.continuousRendering &&
           (map.atlasLevel==MapLevel::World || map.atlasLevel==MapLevel::Region) &&
           cell->kind==MapTileKind::Water && cell->wayKind==MapWayKind::None)continue;
        if (mapAuthoringWayKindIsRoadLike(wayKind))
        {
            if (cell->kind == MapTileKind::Water || cell->kind == MapTileKind::Swamp)
                applyMapCellTerrainDefaults(*cell, MapTileKind::Bridge);
        }

        cell->wayKind = wayKind;
    }
}

inline MapModel previewMapWayBrush(
    const MapModel &map,
    const std::vector<MapCellCoordinate> &footprint,
    MapWayKind wayKind,
    bool erase)
{
    MapModel preview = map;
    applyMapWayBrush(preview, footprint, wayKind, erase);
    return preview;
}

