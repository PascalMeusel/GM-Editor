#pragma once

#include "Models/Map/MapStyleRegistry.hpp"
#include "Models/Map/MapTileSemantics.hpp"

#include <string>
#include <utility>
#include <vector>

inline bool mapStyleTileNeighborRendersAsComponent(const MapTileRenderContext &context, MapTileNeighborSlot slot)
{
    return mapTileConnectsToNeighborInfo(context.kind, mapTileNeighborAt(context, slot));
}

inline int mapStyleComponentDiagonalConnectionCount(const MapTileRenderContext &context)
{
    return (mapStyleTileNeighborRendersAsComponent(context, MapTileNeighborSlot::NorthEast) ? 1 : 0) +
           (mapStyleTileNeighborRendersAsComponent(context, MapTileNeighborSlot::SouthEast) ? 1 : 0) +
           (mapStyleTileNeighborRendersAsComponent(context, MapTileNeighborSlot::SouthWest) ? 1 : 0) +
           (mapStyleTileNeighborRendersAsComponent(context, MapTileNeighborSlot::NorthWest) ? 1 : 0);
}

inline std::string mapStyleComponentRoleKey(const MapTileRenderContext &context)
{
    const int mask = connectedMapTileMask(context);
    const int cardinalCount = mapTileCardinalConnectionCount(mask);
    const int diagonalCount = mapStyleComponentDiagonalConnectionCount(context);
    const bool north = (mask & 1) != 0;
    const bool east = (mask & 2) != 0;
    const bool south = (mask & 4) != 0;
    const bool west = (mask & 8) != 0;

    if (cardinalCount == 0)
        return "isolated";
    if (cardinalCount == 1)
        return "end";
    if (cardinalCount == 2)
        return ((north && south) || (east && west)) ? "spine" : "bend";
    if (cardinalCount == 3)
        return "fork";
    if (cardinalCount == 4)
        return diagonalCount >= 3 ? "interior" : "knot";
    return {};
}

inline int mapStyleStructureGroundPriority(MapTileKind kind)
{
    switch (kind)
    {
    case MapTileKind::Cave:
    case MapTileKind::Chasm:
    case MapTileKind::Room:
    case MapTileKind::Wall:
        return 80;
    case MapTileKind::Water:
    case MapTileKind::Swamp:
    case MapTileKind::Bridge:
        return 70;
    case MapTileKind::Snow:
    case MapTileKind::Ice:
        return 60;
    case MapTileKind::Desert:
        return 50;
    case MapTileKind::Stone:
    case MapTileKind::Road:
        return 40;
    case MapTileKind::Dirt:
        return 30;
    case MapTileKind::Empty:
        return 20;
    default:
        return 10;
    }
}

inline const char *mapStyleStructureEntranceWayKey(MapWayKind kind)
{
    switch (kind)
    {
    case MapWayKind::Trail:
        return "way_trail_narrow";
    case MapWayKind::DirtRoad:
        return "way_dirt_road";
    case MapWayKind::StoneRoad:
        return "way_stone_road";
    case MapWayKind::DungeonCorridor:
        return "dungeon_corridor";
    case MapWayKind::CaveTunnel:
        return "cave_tunnel";
    case MapWayKind::River:
    case MapWayKind::Canal:
    case MapWayKind::None:
        return "";
    }
    return "";
}

inline int mapStyleStructureGroundPriority(const MapTileNeighborInfo &neighbor)
{
    if (neighbor.kind == MapTileKind::Road && neighbor.wayKind == MapWayKind::StoneRoad)
        return 45;
    if (neighbor.kind == MapTileKind::Road && (neighbor.wayKind == MapWayKind::DirtRoad || neighbor.wayKind == MapWayKind::Trail))
        return 35;
    return mapStyleStructureGroundPriority(neighbor.kind);
}

inline const char *mapStyleStructureGroundCandidateKey(const MapTileNeighborInfo &neighbor)
{
    if (neighbor.kind == MapTileKind::Road && neighbor.wayKind == MapWayKind::StoneRoad)
        return "stone_paved";
    if (neighbor.kind == MapTileKind::Road && (neighbor.wayKind == MapWayKind::DirtRoad || neighbor.wayKind == MapWayKind::Trail))
        return "dirt";
    return mapStyleTerrainFamilyKey(neighbor.kind);
}

inline std::string mapStyleStructureGroundKey(const MapTileRenderContext &context)
{
    constexpr MapTileNeighborSlot cardinalSlots[] = {
        MapTileNeighborSlot::North,
        MapTileNeighborSlot::East,
        MapTileNeighborSlot::South,
        MapTileNeighborSlot::West};

    for (MapTileNeighborSlot slot : cardinalSlots)
    {
        const MapTileNeighborInfo &neighbor = mapTileNeighborAt(context, slot);
        if (neighbor.exists && neighbor.kind == MapTileKind::Bridge)
        {
            const char *candidate = mapStyleStructureGroundCandidateKey(neighbor);
            if (candidate[0] != '\0')
                return candidate;
        }
    }

    for (MapTileNeighborSlot slot : cardinalSlots)
    {
        const MapTileNeighborInfo &neighbor = mapTileNeighborAt(context, slot);
        if (!neighbor.exists)
            continue;

        if (mapStyleStructureEntranceWayKey(neighbor.wayKind)[0] != '\0' || neighbor.kind == MapTileKind::Road)
        {
            const char *candidate = mapStyleStructureGroundCandidateKey(neighbor);
            if (candidate[0] != '\0')
                return candidate;
        }
    }

    std::string ground = "grass_open";
    int bestPriority = 0;
    for (const MapTileNeighborInfo &neighbor : context.neighbors)
    {
        if (!neighbor.exists)
            continue;

        const char *candidate = mapStyleStructureGroundCandidateKey(neighbor);
        if (candidate[0] == '\0')
            continue;

        const int priority = mapStyleStructureGroundPriority(neighbor);
        if (priority > bestPriority)
        {
            bestPriority = priority;
            ground = candidate;
        }
    }
    return ground;
}

struct MapStyleStructureEntranceContext
{
    std::string entrance = "none";
    std::string orientation = "none";
};

inline const char *mapStyleStructureEntranceOrientationKey(MapTileNeighborSlot slot)
{
    switch (slot)
    {
    case MapTileNeighborSlot::North:
        return "n";
    case MapTileNeighborSlot::East:
        return "e";
    case MapTileNeighborSlot::South:
        return "s";
    case MapTileNeighborSlot::West:
        return "w";
    default:
        return "none";
    }
}

inline MapStyleStructureEntranceContext mapStyleStructureEntranceContext(const MapTileRenderContext &context)
{
    constexpr MapTileNeighborSlot cardinalSlots[] = {
        MapTileNeighborSlot::North,
        MapTileNeighborSlot::East,
        MapTileNeighborSlot::South,
        MapTileNeighborSlot::West};

    for (MapTileNeighborSlot slot : cardinalSlots)
    {
        const MapTileNeighborInfo &neighbor = mapTileNeighborAt(context, slot);
        if (neighbor.exists && neighbor.kind == MapTileKind::Bridge)
            return MapStyleStructureEntranceContext{"way_wood_bridge", mapStyleStructureEntranceOrientationKey(slot)};
    }

    for (MapTileNeighborSlot slot : cardinalSlots)
    {
        const MapTileNeighborInfo &neighbor = mapTileNeighborAt(context, slot);
        if (!neighbor.exists)
            continue;

        const char *way = mapStyleStructureEntranceWayKey(neighbor.wayKind);
        if (way[0] != '\0')
            return MapStyleStructureEntranceContext{way, mapStyleStructureEntranceOrientationKey(slot)};
        if (neighbor.kind == MapTileKind::Road)
            return MapStyleStructureEntranceContext{"way_dirt_road", mapStyleStructureEntranceOrientationKey(slot)};
    }

    return {};
}

inline std::string mapStyleStructureEntranceKey(const MapTileRenderContext &context)
{
    return mapStyleStructureEntranceContext(context).entrance;
}

inline std::string mapStyleStructureEntranceOrientation(const MapTileRenderContext &context)
{
    return mapStyleStructureEntranceContext(context).orientation;
}

