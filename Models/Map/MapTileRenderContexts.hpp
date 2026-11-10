#pragma once

#include "Models/Map/MapTileSemanticTypes.hpp"

inline void setMapTileNeighbor(MapTileRenderContext &context, MapTileNeighborSlot slot, const MapCellModel *neighbor)
{
    MapTileNeighborInfo &info = context.neighbors[static_cast<size_t>(slot)];
    info.exists = neighbor != nullptr;
    info.kind = neighbor ? neighbor->kind : MapTileKind::Empty;
    info.wayKind = neighbor ? neighbor->wayKind : MapWayKind::None;
}

inline MapTileRenderContext makeMapTileRenderContext(
    MapTileKind kind,
    int rotationQuarterTurns,
    int tileX,
    int tileY,
    const MapCellModel *north,
    const MapCellModel *east,
    const MapCellModel *south,
    const MapCellModel *west,
    const MapCellModel *northEast = nullptr,
    const MapCellModel *southEast = nullptr,
    const MapCellModel *southWest = nullptr,
    const MapCellModel *northWest = nullptr)
{
    MapTileRenderContext context;
    context.kind = kind;
    context.rotationQuarterTurns = rotationQuarterTurns;
    context.rotationLocked = false;
    context.symbolVariantMode = MapSymbolVariantMode::Auto;
    context.tileX = tileX;
    context.tileY = tileY;
    setMapTileNeighbor(context, MapTileNeighborSlot::North, north);
    setMapTileNeighbor(context, MapTileNeighborSlot::East, east);
    setMapTileNeighbor(context, MapTileNeighborSlot::South, south);
    setMapTileNeighbor(context, MapTileNeighborSlot::West, west);
    setMapTileNeighbor(context, MapTileNeighborSlot::NorthEast, northEast);
    setMapTileNeighbor(context, MapTileNeighborSlot::SouthEast, southEast);
    setMapTileNeighbor(context, MapTileNeighborSlot::SouthWest, southWest);
    setMapTileNeighbor(context, MapTileNeighborSlot::NorthWest, northWest);
    return context;
}

inline MapTileRenderContext makeMapTileRenderContext(MapTileKind kind, int rotationQuarterTurns, int tileX, int tileY)
{
    MapTileRenderContext context;
    context.kind = kind;
    context.rotationQuarterTurns = rotationQuarterTurns;
    context.tileX = tileX;
    context.tileY = tileY;
    return context;
}

inline MapTileRenderContext makeMapTileRenderContext(
    const MapCellModel *cell,
    int tileX,
    int tileY,
    const MapCellModel *north,
    const MapCellModel *east,
    const MapCellModel *south,
    const MapCellModel *west,
    const MapCellModel *northEast = nullptr,
    const MapCellModel *southEast = nullptr,
    const MapCellModel *southWest = nullptr,
    const MapCellModel *northWest = nullptr)
{
    MapTileRenderContext context = makeMapTileRenderContext(
        cell ? cell->kind : MapTileKind::Empty,
        cell ? cell->rotationQuarterTurns : 0,
        tileX,
        tileY,
        north,
        east,
        south,
        west,
        northEast,
        southEast,
        southWest,
        northWest);
    context.symbolVariantMode = cell ? cell->symbolVariantMode : MapSymbolVariantMode::Auto;
    context.rotationLocked = cell ? cell->rotationLocked : false;
    return context;
}

inline void setMapWayNeighbor(MapWayRenderContext &context, MapTileNeighborSlot slot, const MapCellModel *neighbor)
{
    MapWayNeighborInfo &info = context.neighbors[static_cast<size_t>(slot)];
    info.exists = neighbor != nullptr;
    info.terrainKind = neighbor ? neighbor->kind : MapTileKind::Empty;
    info.wayKind = neighbor ? neighbor->wayKind : MapWayKind::None;
}

inline MapWayRenderContext makeMapWayRenderContext(
    const MapCellModel *cell,
    int tileX,
    int tileY,
    const MapCellModel *north,
    const MapCellModel *east,
    const MapCellModel *south,
    const MapCellModel *west,
    const MapCellModel *northEast = nullptr,
    const MapCellModel *southEast = nullptr,
    const MapCellModel *southWest = nullptr,
    const MapCellModel *northWest = nullptr)
{
    MapWayRenderContext context;
    context.terrainKind = cell ? cell->kind : MapTileKind::Empty;
    context.wayKind = cell ? cell->wayKind : MapWayKind::None;
    context.tileX = tileX;
    context.tileY = tileY;
    setMapWayNeighbor(context, MapTileNeighborSlot::North, north);
    setMapWayNeighbor(context, MapTileNeighborSlot::East, east);
    setMapWayNeighbor(context, MapTileNeighborSlot::South, south);
    setMapWayNeighbor(context, MapTileNeighborSlot::West, west);
    setMapWayNeighbor(context, MapTileNeighborSlot::NorthEast, northEast);
    setMapWayNeighbor(context, MapTileNeighborSlot::SouthEast, southEast);
    setMapWayNeighbor(context, MapTileNeighborSlot::SouthWest, southWest);
    setMapWayNeighbor(context, MapTileNeighborSlot::NorthWest, northWest);
    return context;
}

inline MapTileRenderContext makeMapTileRenderContext(const MapModel &map, int tileX, int tileY)
{
    return makeMapTileRenderContext(
        mapCellAt(map, tileX, tileY),
        tileX,
        tileY,
        mapCellAt(map, tileX, tileY - 1),
        mapCellAt(map, tileX + 1, tileY),
        mapCellAt(map, tileX, tileY + 1),
        mapCellAt(map, tileX - 1, tileY),
        mapCellAt(map, tileX + 1, tileY - 1),
        mapCellAt(map, tileX + 1, tileY + 1),
        mapCellAt(map, tileX - 1, tileY + 1),
        mapCellAt(map, tileX - 1, tileY - 1));
}

inline MapWayRenderContext makeMapWayRenderContext(const MapModel &map, int tileX, int tileY)
{
    return makeMapWayRenderContext(
        mapCellAt(map, tileX, tileY),
        tileX,
        tileY,
        mapCellAt(map, tileX, tileY - 1),
        mapCellAt(map, tileX + 1, tileY),
        mapCellAt(map, tileX, tileY + 1),
        mapCellAt(map, tileX - 1, tileY),
        mapCellAt(map, tileX + 1, tileY - 1),
        mapCellAt(map, tileX + 1, tileY + 1),
        mapCellAt(map, tileX - 1, tileY + 1),
        mapCellAt(map, tileX - 1, tileY - 1));
}

