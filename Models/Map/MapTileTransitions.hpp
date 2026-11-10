#pragma once

#include "Models/Map/MapWaySemantics.hpp"

inline bool mapTileKindInRelationGroup(MapTileKind kind, MapTileRelationGroup group)
{
    switch (group)
    {
    case MapTileRelationGroup::Water:
        return kind == MapTileKind::Water;
    case MapTileRelationGroup::Sand:
        return kind == MapTileKind::Desert || kind == MapTileKind::Dirt;
    case MapTileRelationGroup::Snow:
        return kind == MapTileKind::Snow || kind == MapTileKind::Ice;
    case MapTileRelationGroup::Rock:
        return kind == MapTileKind::Mountain || kind == MapTileKind::Hill || kind == MapTileKind::Stone || kind == MapTileKind::Rubble || kind == MapTileKind::Cave || kind == MapTileKind::Chasm;
    case MapTileRelationGroup::Swamp:
        return kind == MapTileKind::Swamp;
    case MapTileRelationGroup::DungeonWall:
        return kind == MapTileKind::Wall || kind == MapTileKind::Door;
    }
    return false;
}

inline int mapTileRelationMask(const MapTileRenderContext &context, MapTileRelationGroup group)
{
    auto matches = [group](const MapTileNeighborInfo &neighbor) {
        return neighbor.exists && mapTileKindInRelationGroup(neighbor.kind, group);
    };

    return mapTileCardinalMask(
        matches(mapTileNeighborAt(context, MapTileNeighborSlot::North)),
        matches(mapTileNeighborAt(context, MapTileNeighborSlot::East)),
        matches(mapTileNeighborAt(context, MapTileNeighborSlot::South)),
        matches(mapTileNeighborAt(context, MapTileNeighborSlot::West)));
}

inline const std::vector<MapTileTransitionRule> &mapTileTransitionRules()
{
    static const std::vector<MapTileTransitionRule> rules = {
        {MapTileKind::Empty, MapTileRelationGroup::Water, "grass_open_to_water", 101},
        {MapTileKind::Empty, MapTileRelationGroup::Sand, "grass_open_to_sand", 102},
        {MapTileKind::Empty, MapTileRelationGroup::Snow, "grass_open_to_snow", 103},
        {MapTileKind::Empty, MapTileRelationGroup::Rock, "grass_open_to_rock", 104},
        {MapTileKind::Empty, MapTileRelationGroup::Swamp, "grass_open_to_swamp", 105},
        {MapTileKind::Water, MapTileRelationGroup::Sand, "water_to_sand", 106},
        {MapTileKind::Snow, MapTileRelationGroup::Rock, "snow_to_rock", 107},
        {MapTileKind::Room, MapTileRelationGroup::DungeonWall, "dungeon_floor_to_wall", 108},
    };
    return rules;
}

inline std::vector<MapTileNeighborRelationship> resolveMapTileNeighborRelationships(const MapTileRenderContext &context)
{
    std::vector<MapTileNeighborRelationship> relationships;
    const std::array<MapTileNeighborSlot, 4> slots = {
        MapTileNeighborSlot::North,
        MapTileNeighborSlot::East,
        MapTileNeighborSlot::South,
        MapTileNeighborSlot::West};
    const std::array<int, 4> bits = {1, 2, 4, 8};

    for (size_t index = 0; index < slots.size(); ++index)
    {
        const MapTileNeighborInfo &neighbor = mapTileNeighborAt(context, slots[index]);
        if (!neighbor.exists || mapTileConnectsToNeighborInfo(context.kind, neighbor))
            continue;

        auto found = std::find_if(relationships.begin(), relationships.end(), [&](const MapTileNeighborRelationship &relationship) {
            return relationship.neighborKind == neighbor.kind;
        });
        if (found == relationships.end())
            relationships.push_back({neighbor.kind, bits[index]});
        else
            found->mask |= bits[index];
    }

    return relationships;
}

inline std::vector<MapTileResolvedTransitionOverlay> resolveMapTileTransitionOverlays(const MapTileRenderContext &context)
{
    std::vector<MapTileResolvedTransitionOverlay> overlays;
    for (const MapTileTransitionRule &rule : mapTileTransitionRules())
    {
        if (context.kind != rule.owner)
            continue;

        const int mask = mapTileRelationMask(context, rule.neighborGroup);
        if (mask == 0)
            continue;

        overlays.push_back({
            rule.owner,
            rule.neighborGroup,
            rule.transitionKey,
            mask,
            deterministicMapTileVariant(context.tileX, context.tileY, rule.salt),
            rule.salt});
    }
    return overlays;
}

inline MapTileSemanticResolution resolveMapTileSemantics(const MapTileRenderContext &context)
{
    MapTileSemanticResolution resolution;
    resolution.owner = context.kind;
    resolution.sameFamilyMask = connectedMapTileMask(context);
    resolution.sameFamilyVariant = deterministicMapTileVariant(context.tileX, context.tileY, static_cast<int>(context.kind) + 1);
    resolution.contextualSymbolVariant = resolveMapTileContextualSymbolVariant(context, mapTileContextualSymbolVariantCount(context.kind));
    resolution.neighborRelationships = resolveMapTileNeighborRelationships(context);
    resolution.transitionOverlays = resolveMapTileTransitionOverlays(context);
    return resolution;
}

