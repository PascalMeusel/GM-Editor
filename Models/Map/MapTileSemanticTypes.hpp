#pragma once

#include "Models/Map/MapModels.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <vector>

enum class MapTileNeighborSlot
{
    North,
    East,
    South,
    West,
    NorthEast,
    SouthEast,
    SouthWest,
    NorthWest,
    Count
};

struct MapTileNeighborInfo
{
    bool exists = false;
    MapTileKind kind = MapTileKind::Empty;
    MapWayKind wayKind = MapWayKind::None;
};

struct MapTileRenderContext
{
    MapTileKind kind = MapTileKind::Empty;
    int rotationQuarterTurns = 0;
    bool rotationLocked = false;
    MapSymbolVariantMode symbolVariantMode = MapSymbolVariantMode::Auto;
    int tileX = 0;
    int tileY = 0;
    std::array<MapTileNeighborInfo, static_cast<size_t>(MapTileNeighborSlot::Count)> neighbors{};
};

struct MapWayNeighborInfo
{
    bool exists = false;
    MapTileKind terrainKind = MapTileKind::Empty;
    MapWayKind wayKind = MapWayKind::None;
};

struct MapWayRenderContext
{
    MapTileKind terrainKind = MapTileKind::Empty;
    MapWayKind wayKind = MapWayKind::None;
    int tileX = 0;
    int tileY = 0;
    std::array<MapWayNeighborInfo, static_cast<size_t>(MapTileNeighborSlot::Count)> neighbors{};
};

enum class MapTileRelationGroup
{
    Water,
    Sand,
    Snow,
    Rock,
    Swamp,
    DungeonWall
};

struct MapTileTransitionRule
{
    MapTileKind owner = MapTileKind::Empty;
    MapTileRelationGroup neighborGroup = MapTileRelationGroup::Water;
    std::string transitionKey;
    int salt = 0;
};

struct MapTileNeighborRelationship
{
    MapTileKind neighborKind = MapTileKind::Empty;
    int mask = 0;
};

struct MapTileResolvedTransitionOverlay
{
    MapTileKind owner = MapTileKind::Empty;
    MapTileRelationGroup neighborGroup = MapTileRelationGroup::Water;
    std::string transitionKey;
    int mask = 0;
    int variant = 0;
    int salt = 0;
};

struct MapTileContextualSymbolVariant
{
    bool enabled = false;
    MapTileKind owner = MapTileKind::Empty;
    int sameFamilyMask = 0;
    int shapeVariant = 0;
};

struct MapTileSemanticResolution
{
    MapTileKind owner = MapTileKind::Empty;
    int sameFamilyMask = 0;
    int sameFamilyVariant = 0;
    MapTileContextualSymbolVariant contextualSymbolVariant;
    std::vector<MapTileNeighborRelationship> neighborRelationships;
    std::vector<MapTileResolvedTransitionOverlay> transitionOverlays;
};

inline int deterministicMapTileIndex(int tileX, int tileY, int salt, int count)
{
    if (count <= 1)
        return 0;
    const int value = tileX * 73856093 ^ tileY * 19349663 ^ salt * 83492791;
    return (value & 0x7fffffff) % count;
}

inline int deterministicMapTileVariant(int tileX, int tileY, int salt, int variantCount = 4)
{
    return deterministicMapTileIndex(tileX, tileY, salt, variantCount);
}

inline int mapTileCardinalMask(bool north, bool east, bool south, bool west)
{
    int mask = 0;
    if (north)
        mask |= 1;
    if (east)
        mask |= 2;
    if (south)
        mask |= 4;
    if (west)
        mask |= 8;
    return mask;
}

inline int mapTileCardinalConnectionCount(int mask)
{
    int count = 0;
    if ((mask & 1) != 0)
        ++count;
    if ((mask & 2) != 0)
        ++count;
    if ((mask & 4) != 0)
        ++count;
    if ((mask & 8) != 0)
        ++count;
    return count;
}

inline const MapTileNeighborInfo &mapTileNeighborAt(const MapTileRenderContext &context, MapTileNeighborSlot slot)
{
    return context.neighbors[static_cast<size_t>(slot)];
}

inline bool mapTileConnectsToNeighborInfo(MapTileKind kind, const MapTileNeighborInfo &neighbor)
{
    if (!neighbor.exists)
        return false;

    if (kind == MapTileKind::Road)
        return neighbor.kind == MapTileKind::Road || neighbor.kind == MapTileKind::Bridge;
    if (kind == MapTileKind::Bridge)
        return neighbor.kind == MapTileKind::Road || neighbor.kind == MapTileKind::Bridge;

    return neighbor.kind == kind;
}

inline bool mapTileConnectsToNeighbor(MapTileKind kind, const MapCellModel *neighbor)
{
    if (!neighbor)
        return false;

    return mapTileConnectsToNeighborInfo(kind, MapTileNeighborInfo{true, neighbor->kind});
}

inline bool usesConnectedMapTile(MapTileKind kind)
{
    switch (kind)
    {
    case MapTileKind::Empty:
    case MapTileKind::Dirt:
    case MapTileKind::Water:
    case MapTileKind::Road:
    case MapTileKind::Stone:
    case MapTileKind::Snow:
    case MapTileKind::Desert:
    case MapTileKind::Swamp:
    case MapTileKind::Ice:
    case MapTileKind::Room:
    case MapTileKind::Wall:
    case MapTileKind::Hazard:
    case MapTileKind::Lava:
    case MapTileKind::Pit:
    case MapTileKind::Chasm:
        return true;
    default:
        return false;
    }
}

inline int connectedMapTileMask(const MapTileRenderContext &context)
{
    return mapTileCardinalMask(
        mapTileConnectsToNeighborInfo(context.kind, mapTileNeighborAt(context, MapTileNeighborSlot::North)),
        mapTileConnectsToNeighborInfo(context.kind, mapTileNeighborAt(context, MapTileNeighborSlot::East)),
        mapTileConnectsToNeighborInfo(context.kind, mapTileNeighborAt(context, MapTileNeighborSlot::South)),
        mapTileConnectsToNeighborInfo(context.kind, mapTileNeighborAt(context, MapTileNeighborSlot::West)));
}

inline bool usesContextualSymbolVariant(MapTileKind kind)
{
    switch (kind)
    {
    case MapTileKind::Forest:
    case MapTileKind::Mountain:
    case MapTileKind::Hill:
        return true;
    default:
        return false;
    }
}

inline bool mapTileSymbolVariantModeIgnoresConnectivity(MapSymbolVariantMode mode)
{
    return mode == MapSymbolVariantMode::Individual;
}

inline int mapTileContextualSymbolVariantCount(MapTileKind kind)
{
    switch (kind)
    {
    case MapTileKind::Forest:
    case MapTileKind::Mountain:
    case MapTileKind::Hill:
        return 2;
    default:
        return 1;
    }
}

inline int contextualSymbolShapeVariantForMask(int mask, int variantCount)
{
    if (variantCount <= 1)
        return 0;
    if (mask == 0)
        return 0;
    if (variantCount == 2)
        return mapTileCardinalConnectionCount(mask) >= 2 ? 1 : 0;

    const bool horizontal = (mask & (2 | 8)) != 0;
    const bool vertical = (mask & (1 | 4)) != 0;
    if (horizontal && !vertical)
        return std::min(1, variantCount - 1);
    if (vertical && !horizontal)
        return std::min(2, variantCount - 1);
    if (mapTileCardinalConnectionCount(mask) >= 3)
        return std::min(3, variantCount - 1);
    return std::min(1, variantCount - 1);
}

inline MapTileContextualSymbolVariant resolveMapTileContextualSymbolVariant(const MapTileRenderContext &context, int variantCount)
{
    MapTileContextualSymbolVariant variant;
    if (!usesContextualSymbolVariant(context.kind))
        return variant;

    variant.enabled = true;
    variant.owner = context.kind;
    variant.sameFamilyMask = mapTileSymbolVariantModeIgnoresConnectivity(context.symbolVariantMode) ? 0 : connectedMapTileMask(context);
    variant.shapeVariant = variant.sameFamilyMask == 0
        ? deterministicMapTileIndex(context.tileX, context.tileY, static_cast<int>(context.kind) + 211, variantCount)
        : contextualSymbolShapeVariantForMask(variant.sameFamilyMask, variantCount);
    return variant;
}

inline int contextualSymbolCellIndex(const MapTileRenderContext &context, int salt, int variantCount)
{
    const MapTileContextualSymbolVariant variant = resolveMapTileContextualSymbolVariant(context, variantCount);
    if (variant.enabled && variant.sameFamilyMask != 0)
        return variant.shapeVariant;
    return deterministicMapTileIndex(context.tileX, context.tileY, salt, variantCount);
}

