#pragma once

#include "Models/Map/MapLabels.hpp"

inline std::vector<MapTileKind> paletteForMapScope(MapScope scope)
{
    switch (scope)
    {
    case MapScope::World:
        return {MapTileKind::Empty, MapTileKind::Dirt, MapTileKind::Water, MapTileKind::Forest, MapTileKind::Mountain, MapTileKind::Hill, MapTileKind::Snow, MapTileKind::Desert, MapTileKind::Swamp, MapTileKind::Road, MapTileKind::Bridge, MapTileKind::Settlement, MapTileKind::Building, MapTileKind::Cave, MapTileKind::Landmark};
    case MapScope::Local:
        return {MapTileKind::Empty, MapTileKind::Dirt, MapTileKind::Stone, MapTileKind::Water, MapTileKind::Forest, MapTileKind::Hill, MapTileKind::Snow, MapTileKind::Desert, MapTileKind::Swamp, MapTileKind::Road, MapTileKind::Bridge, MapTileKind::Settlement, MapTileKind::Building, MapTileKind::Cave, MapTileKind::Hazard, MapTileKind::Pit, MapTileKind::Rubble, MapTileKind::Cover, MapTileKind::Landmark};
    case MapScope::Dungeon:
        return {MapTileKind::Empty, MapTileKind::Stone, MapTileKind::Room, MapTileKind::Wall, MapTileKind::Road, MapTileKind::Door, MapTileKind::Cave, MapTileKind::Stairs, MapTileKind::Hazard, MapTileKind::Lava, MapTileKind::Water, MapTileKind::Ice, MapTileKind::Pit, MapTileKind::Chasm, MapTileKind::Rubble, MapTileKind::Cover, MapTileKind::Bridge, MapTileKind::Landmark};
    }
    return {MapTileKind::Empty};
}

inline int normalizeMapRotation(int rotationQuarterTurns)
{
    rotationQuarterTurns %= 4;
    if (rotationQuarterTurns < 0)
        rotationQuarterTurns += 4;
    return rotationQuarterTurns;
}

inline MapSymbolVariantMode normalizeMapSymbolVariantMode(int value)
{
    return static_cast<MapSymbolVariantMode>(std::clamp(value, 0, static_cast<int>(MapSymbolVariantMode::Individual)));
}

inline MapWayKind normalizeMapWayKind(int value)
{
    return static_cast<MapWayKind>(std::clamp(value, 0, static_cast<int>(MapWayKind::CaveTunnel)));
}

inline int normalizeMapEdgeMask(int edgeMask)
{
    return edgeMask & 0x0f;
}

inline int mapCardinalEdgeMask(int directionQuarterTurns)
{
    return 1 << normalizeMapRotation(directionQuarterTurns);
}

inline int oppositeMapCardinalEdgeMask(int directionQuarterTurns)
{
    return mapCardinalEdgeMask(directionQuarterTurns + 2);
}

inline int mapCardinalEdgeDeltaX(int directionQuarterTurns)
{
    switch (normalizeMapRotation(directionQuarterTurns))
    {
    case 1:
        return 1;
    case 3:
        return -1;
    default:
        return 0;
    }
}

inline int mapCardinalEdgeDeltaY(int directionQuarterTurns)
{
    switch (normalizeMapRotation(directionQuarterTurns))
    {
    case 0:
        return -1;
    case 2:
        return 1;
    default:
        return 0;
    }
}

inline const char *mapTacticalPresetLabel(MapTacticalPreset preset)
{
    switch (preset)
    {
    case MapTacticalPreset::CustomBlocker:
        return "Custom Blocker";
    case MapTacticalPreset::Wall:
        return "Wall";
    case MapTacticalPreset::Ledge:
        return "Ledge";
    case MapTacticalPreset::StairsUp:
        return "Stairs Up";
    case MapTacticalPreset::Window:
        return "Window";
    case MapTacticalPreset::LowCover:
        return "Low Cover";
    case MapTacticalPreset::ClosedDoor:
        return "Closed Door";
    case MapTacticalPreset::OpenDoor:
        return "Open Door";
    case MapTacticalPreset::RampUp:
        return "Ramp Up";
    case MapTacticalPreset::TreeTrunk:
        return "Tree Trunk";
    case MapTacticalPreset::ClearEdge:
        return "Clear Edge";
    }
    return "Custom Blocker";
}

inline const char *mapTacticalPresetDescription(MapTacticalPreset preset)
{
    switch (preset)
    {
    case MapTacticalPreset::CustomBlocker:
        return "Paint movement, line-of-sight, or combined edge blockers.";
    case MapTacticalPreset::Wall:
        return "Blocks movement and sight in both directions.";
    case MapTacticalPreset::Ledge:
        return "Raises the far cell and blocks only uphill movement.";
    case MapTacticalPreset::StairsUp:
        return "Raises the far cell and leaves the edge open.";
    case MapTacticalPreset::Window:
        return "Blocks movement but permits line of sight.";
    case MapTacticalPreset::LowCover:
        return "Blocks movement but permits line of sight.";
    case MapTacticalPreset::ClosedDoor:
        return "Blocks movement and sight in both directions.";
    case MapTacticalPreset::OpenDoor:
        return "Clears blockers from this edge.";
    case MapTacticalPreset::RampUp:
        return "Raises the far cell and leaves the edge open.";
    case MapTacticalPreset::TreeTrunk:
        return "Blocks movement and sight across the painted trunk edge.";
    case MapTacticalPreset::ClearEdge:
        return "Clears symmetric and one-way blockers from this edge.";
    }
    return "Paint tactical edge metadata.";
}

inline int mapTileKindDefaultElevation(MapTileKind kind)
{
    switch (kind)
    {
    case MapTileKind::Mountain:
        return 3;
    case MapTileKind::Hill:
    case MapTileKind::Bridge:
    case MapTileKind::Stairs:
    case MapTileKind::Building:
        return 1;
    case MapTileKind::Pit:
    case MapTileKind::Chasm:
        return -2;
    default:
        return 0;
    }
}

inline bool mapTileKindDefaultsToDifficultTerrain(MapTileKind kind)
{
    switch (kind)
    {
    case MapTileKind::Water:
    case MapTileKind::Forest:
    case MapTileKind::Hill:
    case MapTileKind::Dirt:
    case MapTileKind::Snow:
    case MapTileKind::Desert:
    case MapTileKind::Swamp:
    case MapTileKind::Hazard:
    case MapTileKind::Lava:
    case MapTileKind::Ice:
    case MapTileKind::Pit:
    case MapTileKind::Rubble:
    case MapTileKind::Cover:
        return true;
    default:
        return false;
    }
}

inline bool mapTileKindDefaultsToLineOfSightBlock(MapTileKind kind)
{
    switch (kind)
    {
    case MapTileKind::Mountain:
    case MapTileKind::Building:
    case MapTileKind::Wall:
    case MapTileKind::Chasm:
        return true;
    default:
        return false;
    }
}

inline int mapTileKindDefaultMovementCost(MapTileKind kind)
{
    switch (kind)
    {
    case MapTileKind::Building:
    case MapTileKind::Wall:
    case MapTileKind::Mountain:
    case MapTileKind::Lava:
    case MapTileKind::Pit:
    case MapTileKind::Chasm:
        return 99;
    case MapTileKind::Water:
        return 3;
    case MapTileKind::Forest:
    case MapTileKind::Hill:
    case MapTileKind::Dirt:
    case MapTileKind::Snow:
    case MapTileKind::Desert:
    case MapTileKind::Swamp:
    case MapTileKind::Hazard:
    case MapTileKind::Ice:
    case MapTileKind::Rubble:
    case MapTileKind::Cover:
        return 2;
    default:
        return 1;
    }
}

inline int mapTileKindDefaultMovementBlockedMask(MapTileKind kind)
{
    switch (kind)
    {
    case MapTileKind::Building:
    case MapTileKind::Wall:
    case MapTileKind::Mountain:
    case MapTileKind::Lava:
    case MapTileKind::Pit:
    case MapTileKind::Chasm:
        return 0x0f;
    default:
        return 0;
    }
}

inline int mapTileKindDefaultLineOfSightBlockedMask(MapTileKind kind)
{
    return mapTileKindDefaultsToLineOfSightBlock(kind) ? 0x0f : 0;
}

inline void applyMapCellTerrainDefaults(MapCellModel &cell, MapTileKind kind, int rotationQuarterTurns = 0)
{
    cell.kind = kind;
    cell.rotationQuarterTurns = normalizeMapRotation(rotationQuarterTurns);
    cell.rotationLocked = false;
    cell.movementCost = mapTileKindDefaultMovementCost(kind);
    cell.difficultTerrain = mapTileKindDefaultsToDifficultTerrain(kind);
    cell.blocksLineOfSight = mapTileKindDefaultsToLineOfSightBlock(kind);
    cell.elevation = mapTileKindDefaultElevation(kind);
    cell.movementBlockedMask = mapTileKindDefaultMovementBlockedMask(kind);
    cell.lineOfSightBlockedMask = mapTileKindDefaultLineOfSightBlockedMask(kind);
    cell.movementOneWayBlockedMask = 0;
    cell.lineOfSightOneWayBlockedMask = 0;
}

inline void resizeMapCells(MapModel &map)
{
    map.width = std::max(4, map.width);
    map.height = std::max(4, map.height);
    map.cells.resize(static_cast<size_t>(map.width * map.height));
}

inline void ensureDefaultMapLayer(MapModel &map)
{
    if (!map.layers.empty())
        return;

    MapLayerModel layer;
    layer.name = "Base";
    layer.visible = true;
    layer.locked = false;
    layer.sortOrder = 0;
    map.layers.push_back(layer);
}

inline int mapCellIndex(const MapModel &map, int x, int y)
{
    return y * map.width + x;
}

inline bool isMapCellInside(const MapModel &map, int x, int y)
{
    return x >= 0 && y >= 0 && x < map.width && y < map.height;
}

inline MapCellModel *mapCellAt(MapModel &map, int x, int y)
{
    if (!isMapCellInside(map, x, y))
        return nullptr;
    resizeMapCells(map);
    return &map.cells[static_cast<size_t>(mapCellIndex(map, x, y))];
}

inline const MapCellModel *mapCellAt(const MapModel &map, int x, int y)
{
    if (!isMapCellInside(map, x, y))
        return nullptr;
    const size_t index = static_cast<size_t>(mapCellIndex(map, x, y));
    if (index >= map.cells.size())
        return nullptr;
    return &map.cells[index];
}

// Structure and marker cells replace a semantic grid value, but they do not
// replace the ground underneath the illustration.  Renderers used to treat
// those cells as bare grass, which produced conspicuous green square holes in
// dirt districts and water crossings.  Resolve a stable local ground kind from
// neighbouring terrain so Editor, preview and PNG output can share the same
// visual contract without changing persisted map data.
inline bool mapTileUsesInferredVisualGround(MapTileKind kind, MapWayKind wayKind = MapWayKind::None)
{
    return kind == MapTileKind::Settlement || kind == MapTileKind::Building ||
           kind == MapTileKind::Landmark || kind == MapTileKind::Bridge ||
           kind == MapTileKind::Door || kind == MapTileKind::Stairs ||
           kind == MapTileKind::Hazard || kind == MapTileKind::Rubble ||
           kind == MapTileKind::Cover ||
           (kind == MapTileKind::Water && wayKind == MapWayKind::River);
}

inline MapTileKind mapVisualGroundKind(const MapModel &map, int x, int y)
{
    const MapCellModel *cell = mapCellAt(map, x, y);
    if (!cell)
        return MapTileKind::Empty;
    if (!mapTileUsesInferredVisualGround(cell->kind, cell->wayKind))
        return cell->kind;

    constexpr int kindCount = static_cast<int>(MapTileKind::Bridge) + 1;
    int scores[kindCount]{};
    constexpr int offsets[][3] = {
        {0, -1, 3}, {1, 0, 3}, {0, 1, 3}, {-1, 0, 3},
        {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}, {-1, -1, 1}};
    for (const auto &offset : offsets)
    {
        const MapCellModel *neighbor = mapCellAt(map, x + offset[0], y + offset[1]);
        if (!neighbor || mapTileUsesInferredVisualGround(neighbor->kind, neighbor->wayKind))
            continue;
        const int index = static_cast<int>(neighbor->kind);
        if (index >= 0 && index < kindCount)
            scores[index] += offset[2];
    }

    // A bridge visually crosses water even when the road projection owns the
    // cell's current way kind.  Prefer adjacent water over a narrow land-bank
    // majority so no grass square appears below the bridge stamp.
    if (cell->kind == MapTileKind::Bridge && scores[static_cast<int>(MapTileKind::Water)] > 0)
        return MapTileKind::Water;

    auto visualPriority = [](MapTileKind kind) {
        switch (kind)
        {
        case MapTileKind::Room:
        case MapTileKind::Cave:
        case MapTileKind::Stone:
        case MapTileKind::Dirt:
            return 2;
        case MapTileKind::Empty:
            return 0;
        default:
            return 1;
        }
    };
    MapTileKind best = MapTileKind::Empty;
    int bestScore = -1;
    for (int index = 0; index < kindCount; ++index)
    {
        const MapTileKind candidate = static_cast<MapTileKind>(index);
        if (scores[index] > bestScore ||
            (scores[index] == bestScore && visualPriority(candidate) > visualPriority(best)))
        {
            best = candidate;
            bestScore = scores[index];
        }
    }
    return bestScore > 0 ? best : MapTileKind::Empty;
}

// Local settlement art follows the generated place archetype. Keep this role
// decision beside the shared map model so Editor, PNG output and QA evidence
// cannot silently test different asset families.
inline const char *mapSettlementVisualSemanticRole(const MapModel &map, MapTileKind kind)
{
    if (kind == MapTileKind::Building)
        return "town_service";
    if (map.generation.recipe.recipeId == "settlement_fortified")
        return "fortification";
    if (map.generation.recipe.recipeId == "settlement_coastal")
        return "waterfront";
    return "town_service";
}
