#pragma once

#include "Models/Map/MapTileRenderContexts.hpp"

inline bool mapWayKindIsRoadLike(MapWayKind kind)
{
    return kind == MapWayKind::Trail || kind == MapWayKind::DirtRoad || kind == MapWayKind::StoneRoad;
}

inline bool mapWayKindIsWaterLike(MapWayKind kind)
{
    return kind == MapWayKind::River || kind == MapWayKind::Canal;
}

inline bool mapWayKindIsInteriorLike(MapWayKind kind)
{
    return kind == MapWayKind::DungeonCorridor || kind == MapWayKind::CaveTunnel;
}

inline bool mapWayKindsConnect(MapWayKind owner, MapWayKind neighbor)
{
    if (owner == MapWayKind::None || neighbor == MapWayKind::None)
        return false;
    if (owner == neighbor)
        return true;
    if (mapWayKindIsRoadLike(owner) && mapWayKindIsRoadLike(neighbor))
        return true;
    if (mapWayKindIsWaterLike(owner) && mapWayKindIsWaterLike(neighbor))
        return true;
    if (mapWayKindIsInteriorLike(owner) && mapWayKindIsInteriorLike(neighbor))
        return true;
    return false;
}

inline bool mapWayConnectsToNeighborInfo(MapWayKind owner, const MapWayNeighborInfo &neighbor)
{
    if (!neighbor.exists)
        return false;
    if (mapWayKindsConnect(owner, neighbor.wayKind))
        return true;
    return mapWayKindIsWaterLike(owner) && neighbor.terrainKind == MapTileKind::Bridge && mapWayKindIsRoadLike(neighbor.wayKind);
}

inline int mapWayCardinalMask(const MapWayRenderContext &context)
{
    return mapTileCardinalMask(
        mapWayConnectsToNeighborInfo(context.wayKind, context.neighbors[static_cast<size_t>(MapTileNeighborSlot::North)]),
        mapWayConnectsToNeighborInfo(context.wayKind, context.neighbors[static_cast<size_t>(MapTileNeighborSlot::East)]),
        mapWayConnectsToNeighborInfo(context.wayKind, context.neighbors[static_cast<size_t>(MapTileNeighborSlot::South)]),
        mapWayConnectsToNeighborInfo(context.wayKind, context.neighbors[static_cast<size_t>(MapTileNeighborSlot::West)]));
}

inline int mapWayEightNeighborMask(const MapWayRenderContext &context)
{
    int mask = 0;
    if (mapWayConnectsToNeighborInfo(context.wayKind, context.neighbors[static_cast<size_t>(MapTileNeighborSlot::North)]))
        mask |= 1;
    if (mapWayConnectsToNeighborInfo(context.wayKind, context.neighbors[static_cast<size_t>(MapTileNeighborSlot::NorthEast)]))
        mask |= 2;
    if (mapWayConnectsToNeighborInfo(context.wayKind, context.neighbors[static_cast<size_t>(MapTileNeighborSlot::East)]))
        mask |= 4;
    if (mapWayConnectsToNeighborInfo(context.wayKind, context.neighbors[static_cast<size_t>(MapTileNeighborSlot::SouthEast)]))
        mask |= 8;
    if (mapWayConnectsToNeighborInfo(context.wayKind, context.neighbors[static_cast<size_t>(MapTileNeighborSlot::South)]))
        mask |= 16;
    if (mapWayConnectsToNeighborInfo(context.wayKind, context.neighbors[static_cast<size_t>(MapTileNeighborSlot::SouthWest)]))
        mask |= 32;
    if (mapWayConnectsToNeighborInfo(context.wayKind, context.neighbors[static_cast<size_t>(MapTileNeighborSlot::West)]))
        mask |= 64;
    if (mapWayConnectsToNeighborInfo(context.wayKind, context.neighbors[static_cast<size_t>(MapTileNeighborSlot::NorthWest)]))
        mask |= 128;
    return mask;
}

inline bool mapWayRendersAsBridge(const MapWayRenderContext &context)
{
    return mapWayKindIsRoadLike(context.wayKind) && (context.terrainKind == MapTileKind::Water || context.terrainKind == MapTileKind::Swamp || context.terrainKind == MapTileKind::Bridge);
}

inline int automaticMapTileRotation(const MapTileRenderContext &context)
{
    const int manualRotation = normalizeMapRotation(context.rotationQuarterTurns);
    if (context.rotationLocked)
        return manualRotation;

    const int sameMask = connectedMapTileMask(context);
    switch (context.kind)
    {
    case MapTileKind::Bridge:
    {
        const bool horizontal = (sameMask & (2 | 8)) != 0;
        const bool vertical = (sameMask & (1 | 4)) != 0;
        return horizontal && !vertical ? 1 : 0;
    }
    case MapTileKind::Door:
    {
        const bool blockedNorthSouth =
            mapTileKindDefaultsToLineOfSightBlock(mapTileNeighborAt(context, MapTileNeighborSlot::North).kind) ||
            mapTileKindDefaultsToLineOfSightBlock(mapTileNeighborAt(context, MapTileNeighborSlot::South).kind);
        const bool blockedEastWest =
            mapTileKindDefaultsToLineOfSightBlock(mapTileNeighborAt(context, MapTileNeighborSlot::East).kind) ||
            mapTileKindDefaultsToLineOfSightBlock(mapTileNeighborAt(context, MapTileNeighborSlot::West).kind);
        return blockedEastWest && !blockedNorthSouth ? 1 : 0;
    }
    case MapTileKind::Stairs:
    {
        const bool horizontal = (sameMask & (2 | 8)) != 0;
        const bool vertical = (sameMask & (1 | 4)) != 0;
        return horizontal && !vertical ? 1 : 0;
    }
    default:
        return 0;
    }
}

