#pragma once

#include "Models/Map/MapStyleRegistry.hpp"
#include "Models/Map/MapTileSemantics.hpp"

#include <string>
#include <utility>
#include <vector>

inline const char *mapStyleSymbolVariantModeKey(MapSymbolVariantMode mode)
{
    switch (mode)
    {
    case MapSymbolVariantMode::Auto:
        return "auto";
    case MapSymbolVariantMode::Continuous:
        return "continuous";
    case MapSymbolVariantMode::Individual:
        return "individual";
    }
    return "auto";
}

inline std::string mapStyleBridgeFamilyKey(const std::string &owner)
{
    const std::size_t separator = owner.find(':');
    return separator == std::string::npos ? owner : owner.substr(0, separator);
}

inline std::string mapStyleBridgeCrossingKey(const std::string &owner)
{
    const std::size_t separator = owner.find(':');
    return separator == std::string::npos ? std::string() : owner.substr(separator + 1);
}

inline bool mapStyleWayNeighborRendersAsBridgeSpan(const MapWayNeighborInfo &neighbor)
{
    return neighbor.exists &&
           mapWayKindIsRoadLike(neighbor.wayKind) &&
           (neighbor.terrainKind == MapTileKind::Water ||
            neighbor.terrainKind == MapTileKind::Swamp ||
            neighbor.terrainKind == MapTileKind::Bridge);
}

inline bool mapStyleWayNeighborIsBridgeApproach(const MapWayNeighborInfo &neighbor)
{
    return neighbor.exists &&
           mapWayKindIsRoadLike(neighbor.wayKind) &&
           !mapStyleWayNeighborRendersAsBridgeSpan(neighbor);
}

inline std::string mapStyleBridgeSpanPositionKey(const MapWayRenderContext &context)
{
    const MapWayNeighborInfo &north = context.neighbors[static_cast<size_t>(MapTileNeighborSlot::North)];
    const MapWayNeighborInfo &east = context.neighbors[static_cast<size_t>(MapTileNeighborSlot::East)];
    const MapWayNeighborInfo &south = context.neighbors[static_cast<size_t>(MapTileNeighborSlot::South)];
    const MapWayNeighborInfo &west = context.neighbors[static_cast<size_t>(MapTileNeighborSlot::West)];
    const bool northBridge = mapStyleWayNeighborRendersAsBridgeSpan(north);
    const bool eastBridge = mapStyleWayNeighborRendersAsBridgeSpan(east);
    const bool southBridge = mapStyleWayNeighborRendersAsBridgeSpan(south);
    const bool westBridge = mapStyleWayNeighborRendersAsBridgeSpan(west);
    const bool northApproach = mapStyleWayNeighborIsBridgeApproach(north);
    const bool eastApproach = mapStyleWayNeighborIsBridgeApproach(east);
    const bool southApproach = mapStyleWayNeighborIsBridgeApproach(south);
    const bool westApproach = mapStyleWayNeighborIsBridgeApproach(west);
    const int bridgeNeighborCount =
        (northBridge ? 1 : 0) +
        (eastBridge ? 1 : 0) +
        (southBridge ? 1 : 0) +
        (westBridge ? 1 : 0);

    if ((northApproach && southBridge) || (westApproach && eastBridge))
        return "threshold_a";
    if ((southApproach && northBridge) || (eastApproach && westBridge))
        return "threshold_b";
    if (bridgeNeighborCount >= 2)
        return ((context.tileX + context.tileY) % 2 == 0) ? "mid_01" : "mid_02";
    if ((northApproach && southApproach) || (eastApproach && westApproach))
        return ((context.tileX + context.tileY) % 2 == 0) ? "mid_01" : "mid_02";
    if (northApproach || westApproach)
        return "threshold_a";
    if (southApproach || eastApproach)
        return "threshold_b";
    if (bridgeNeighborCount == 1)
        return (northBridge || westBridge) ? "threshold_b" : "threshold_a";
    return {};
}

inline bool mapStyleWayTerrainCanHostBridgeApproach(MapTileKind terrainKind)
{
    return terrainKind != MapTileKind::Water &&
           terrainKind != MapTileKind::Swamp &&
           terrainKind != MapTileKind::Chasm &&
           terrainKind != MapTileKind::Bridge;
}

inline const MapWayNeighborInfo *mapStyleBridgeApproachNeighbor(const MapWayRenderContext &context)
{
    constexpr MapTileNeighborSlot cardinalSlots[] = {
        MapTileNeighborSlot::North,
        MapTileNeighborSlot::East,
        MapTileNeighborSlot::South,
        MapTileNeighborSlot::West};
    for (MapTileNeighborSlot slot : cardinalSlots)
    {
        const MapWayNeighborInfo &neighbor = context.neighbors[static_cast<size_t>(slot)];
        if (mapStyleWayNeighborRendersAsBridgeSpan(neighbor))
            return &neighbor;
    }
    return nullptr;
}

inline std::string mapStyleBridgeApproachPositionKey(const MapWayRenderContext &context)
{
    const bool northBridge = mapStyleWayNeighborRendersAsBridgeSpan(context.neighbors[static_cast<size_t>(MapTileNeighborSlot::North)]);
    const bool eastBridge = mapStyleWayNeighborRendersAsBridgeSpan(context.neighbors[static_cast<size_t>(MapTileNeighborSlot::East)]);
    const bool southBridge = mapStyleWayNeighborRendersAsBridgeSpan(context.neighbors[static_cast<size_t>(MapTileNeighborSlot::South)]);
    const bool westBridge = mapStyleWayNeighborRendersAsBridgeSpan(context.neighbors[static_cast<size_t>(MapTileNeighborSlot::West)]);

    if (southBridge || eastBridge)
        return "approach_a";
    if (northBridge || westBridge)
        return "approach_b";
    return {};
}

inline bool mapStyleBridgeSequenceContinuesThroughNeighbor(const MapWayRenderContext &context, MapTileNeighborSlot slot)
{
    const MapWayNeighborInfo &neighbor = context.neighbors[static_cast<size_t>(slot)];
    return mapStyleWayNeighborRendersAsBridgeSpan(neighbor) || mapStyleWayNeighborIsBridgeApproach(neighbor);
}

inline int mapStyleBridgeSequenceRotationQuarterTurns(const MapWayRenderContext &context)
{
    const bool horizontal =
        mapStyleBridgeSequenceContinuesThroughNeighbor(context, MapTileNeighborSlot::East) ||
        mapStyleBridgeSequenceContinuesThroughNeighbor(context, MapTileNeighborSlot::West);
    const bool vertical =
        mapStyleBridgeSequenceContinuesThroughNeighbor(context, MapTileNeighborSlot::North) ||
        mapStyleBridgeSequenceContinuesThroughNeighbor(context, MapTileNeighborSlot::South);
    if (horizontal && !vertical)
        return 1;

    const int mask = mapWayCardinalMask(context);
    const bool horizontalWay = (mask & (2 | 8)) != 0;
    const bool verticalWay = (mask & (1 | 4)) != 0;
    return horizontalWay && !verticalWay ? 1 : 0;
}

