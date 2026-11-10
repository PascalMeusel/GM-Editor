#pragma once

#include "Views/Editor/Widgets/Map/MapTileAtlasSelection.hpp"

namespace
{
MapTileRenderContext contextFromLegacyMask(MapTileKind kind, bool sameNorth, bool sameEast, bool sameSouth, bool sameWest, int tileX, int tileY)
{
    MapTileRenderContext context;
    context.kind = kind;
    context.tileX = tileX;
    context.tileY = tileY;
    auto setSame = [&context, kind](MapTileNeighborSlot slot, bool same) {
        MapTileNeighborInfo &neighbor = context.neighbors[static_cast<size_t>(slot)];
        neighbor.exists = same;
        neighbor.kind = kind;
    };
    setSame(MapTileNeighborSlot::North, sameNorth);
    setSame(MapTileNeighborSlot::East, sameEast);
    setSame(MapTileNeighborSlot::South, sameSouth);
    setSame(MapTileNeighborSlot::West, sameWest);
    return context;
}

AtlasCrop cropFromPixels(TilesetKind tileset, int x, int y, int width, int height)
{
    constexpr float atlasSize = 1024.0f;
    return {
        tileset,
        static_cast<float>(x) / atlasSize,
        static_cast<float>(y) / atlasSize,
        static_cast<float>(x + width) / atlasSize,
        static_cast<float>(y + height) / atlasSize};
}

AtlasCrop cropFromCell(TilesetKind tileset, int column, int row)
{
    return cropFromPixels(tileset, column * 128, row * 128, 128, 128);
}

int cardinalConnectionCount(int mask)
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

bool wayContextIsOutdoorRoadPrototype(const MapWayRenderContext &context)
{
    if (!mapWayKindIsRoadLike(context.wayKind))
        return false;

    switch (context.terrainKind)
    {
    case MapTileKind::Road:
    case MapTileKind::Bridge:
        return true;
    default:
        return false;
    }
}

int integratedWayPrototypeColumn(const MapWayRenderContext &context)
{
    const int cardinalMask = mapWayCardinalMask(context);
    const int eightMask = mapWayEightNeighborMask(context);
    const int count = cardinalConnectionCount(cardinalMask);

    if (count >= 4)
        return 4;
    if (count == 3)
        return 3;
    if (count == 2)
    {
        const bool vertical = (cardinalMask & (1 | 4)) == (1 | 4);
        const bool horizontal = (cardinalMask & (2 | 8)) == (2 | 8);
        if (vertical)
            return deterministicMapTileIndex(context.tileX, context.tileY, 101, 2) == 0 ? 0 : 2;
        if (horizontal)
            return deterministicMapTileIndex(context.tileX, context.tileY, 102, 2) == 0 ? 2 : 5;
        return deterministicMapTileIndex(context.tileX, context.tileY, 103, 2) == 0 ? 1 : 3;
    }
    if (count == 1)
        return deterministicMapTileIndex(context.tileX, context.tileY, 104, 3);
    if ((eightMask & (2 | 8 | 32 | 128)) != 0)
        return deterministicMapTileIndex(context.tileX, context.tileY, 105, 2) == 0 ? 1 : 2;
    return deterministicMapTileIndex(context.tileX, context.tileY, 106, 6);
}

int integratedWayPrototypeRow(const MapWayRenderContext &context)
{
    if (mapWayRendersAsBridge(context))
    {
        const int variant = deterministicMapTileIndex(context.tileX, context.tileY, 171, 4);
        if (context.terrainKind == MapTileKind::Bridge)
            return variant == 0 ? 4 : 5;
        return variant <= 1 ? 6 : 7;
    }

    switch (context.wayKind)
    {
    case MapWayKind::Trail:
        return deterministicMapTileIndex(context.tileX, context.tileY, 172, 2) == 0 ? 0 : 2;
    case MapWayKind::StoneRoad:
        return deterministicMapTileIndex(context.tileX, context.tileY, 173, 2) == 0 ? 1 : 3;
    case MapWayKind::DirtRoad:
        return deterministicMapTileIndex(context.tileX, context.tileY, 174, 2) == 0 ? 0 : 2;
    default:
        return 0;
    }
}

AtlasCrop mountainPrototypeCrop(const MapTileRenderContext &context)
{
    const int mask = connectedMapTileMask(context);
    const int count = cardinalConnectionCount(mask);
    int row = 0;
    if (context.symbolVariantMode == MapSymbolVariantMode::Individual || mask == 0)
        row = deterministicMapTileIndex(context.tileX, context.tileY, 201, 2);
    else if (count == 1)
        row = 5;
    else if (count == 2)
    {
        const bool straight = (mask == (1 | 4)) || (mask == (2 | 8));
        row = straight ? 2 : 3;
    }
    else if (count == 3)
        row = 4;
    else
        row = 2;

    const int column = deterministicMapTileIndex(context.tileX, context.tileY, 202 + mask, 6);
    return cropFromCell(TilesetKind::MountainRangePrototype, column, row);
}

int structurePrototypeRow(const MapTileRenderContext &context)
{
    int waterLike = 0;
    int snowLike = 0;
    int desertLike = 0;
    int caveLike = 0;
    int pavedLike = 0;
    int rockyLike = 0;
    int grassLike = 0;

    for (const MapTileNeighborInfo &neighbor : context.neighbors)
    {
        if (!neighbor.exists)
            continue;
        switch (neighbor.kind)
        {
        case MapTileKind::Water:
        case MapTileKind::Swamp:
            ++waterLike;
            break;
        case MapTileKind::Snow:
        case MapTileKind::Ice:
            ++snowLike;
            break;
        case MapTileKind::Desert:
            ++desertLike;
            break;
        case MapTileKind::Cave:
        case MapTileKind::Chasm:
        case MapTileKind::Room:
        case MapTileKind::Wall:
            ++caveLike;
            break;
        case MapTileKind::Stone:
        case MapTileKind::Road:
        case MapTileKind::Bridge:
            ++pavedLike;
            break;
        case MapTileKind::Mountain:
        case MapTileKind::Hill:
        case MapTileKind::Rubble:
            ++rockyLike;
            break;
        default:
            ++grassLike;
            break;
        }
    }

    if (waterLike > 0)
        return 5;
    if (snowLike > 0)
        return 3;
    if (desertLike > 0)
        return 4;
    if (caveLike > 0)
        return 2;
    if (pavedLike > grassLike && pavedLike >= rockyLike)
        return deterministicMapTileIndex(context.tileX, context.tileY, 211, 2) == 0 ? 1 : 7;
    if (rockyLike > grassLike)
        return 6;
    return 0;
}

int structurePrototypeColumn(const MapTileRenderContext &context)
{
    bool hasBridgeEntrance = false;
    int roadEntrances = 0;
    int slotIndex = 0;
    int strongestSlot = 0;
    for (const MapTileNeighborInfo &neighbor : context.neighbors)
    {
        if (neighbor.exists && (neighbor.kind == MapTileKind::Bridge || (mapWayKindIsRoadLike(neighbor.wayKind) && (neighbor.kind == MapTileKind::Water || neighbor.kind == MapTileKind::Swamp))))
        {
            hasBridgeEntrance = true;
            strongestSlot = slotIndex;
        }
        else if (neighbor.exists && (neighbor.kind == MapTileKind::Road || mapWayKindIsRoadLike(neighbor.wayKind)))
        {
            ++roadEntrances;
            strongestSlot = slotIndex;
        }
        ++slotIndex;
    }

    if (hasBridgeEntrance)
        return 6 + deterministicMapTileIndex(context.tileX, context.tileY, 221 + strongestSlot, 2);
    if (roadEntrances > 0)
        return 1 + deterministicMapTileIndex(context.tileX, context.tileY, 222 + strongestSlot, 5);
    return 0;
}

AtlasCrop structurePrototypeCrop(const MapTileRenderContext &context)
{
    return cropFromCell(
        TilesetKind::StructureContextPrototype,
        structurePrototypeColumn(context),
        structurePrototypeRow(context));
}

bool cropForTile(MapTileKind kind, AtlasCrop &crop)
{
    switch (kind)
    {
    case MapTileKind::Empty:
        crop = cropFromAutoCell(TilesetKind::GrassAuto, 15, 0);
        return true;
    case MapTileKind::Water:
        crop = cropFromAutoCell(TilesetKind::WaterAuto, 15, 0);
        return true;
    case MapTileKind::Forest:
        crop = cropFromCell(TilesetKind::Nature, 0, 0);
        return true;
    case MapTileKind::Mountain:
        crop = cropFromCell(TilesetKind::Nature, 0, 1);
        return true;
    case MapTileKind::Hill:
        crop = cropFromCell(TilesetKind::Nature, 6, 0);
        return true;
    case MapTileKind::Road:
        crop = cropFromAutoCell(TilesetKind::RoadAuto, 15, 0);
        return true;
    case MapTileKind::Settlement:
        crop = cropFromCell(TilesetKind::Settlement, 1, 0);
        return true;
    case MapTileKind::Building:
        crop = cropFromCell(TilesetKind::Settlement, 0, 0);
        return true;
    case MapTileKind::Room:
        crop = cropFromAutoCell(TilesetKind::DungeonFloorAuto, 15, 0);
        return true;
    case MapTileKind::Wall:
        crop = cropFromAutoCell(TilesetKind::DungeonWallAuto, 15, 0);
        return true;
    case MapTileKind::Door:
        crop = cropFromCell(TilesetKind::Dungeon, 2, 0);
        return true;
    case MapTileKind::Cave:
        crop = cropFromCell(TilesetKind::Nature, 4, 1);
        return true;
    case MapTileKind::Stairs:
        crop = cropFromCell(TilesetKind::Travel, 1, 1);
        return true;
    case MapTileKind::Hazard:
        crop = cropFromAutoCell(TilesetKind::HazardAuto, 15, 0);
        return true;
    case MapTileKind::Landmark:
        crop = cropFromCell(TilesetKind::Settlement, 5, 0);
        return true;
    case MapTileKind::Dirt:
        crop = cropFromAutoCell(TilesetKind::DirtAuto, 15, 0);
        return true;
    case MapTileKind::Stone:
        crop = cropFromAutoCell(TilesetKind::StoneAuto, 15, 0);
        return true;
    case MapTileKind::Snow:
        crop = cropFromAutoCell(TilesetKind::SnowAuto, 15, 0);
        return true;
    case MapTileKind::Desert:
        crop = cropFromAutoCell(TilesetKind::DesertAuto, 15, 0);
        return true;
    case MapTileKind::Swamp:
        crop = cropFromAutoCell(TilesetKind::SwampAuto, 15, 0);
        return true;
    case MapTileKind::Lava:
        crop = cropFromAutoCell(TilesetKind::LavaAuto, 15, 0);
        return true;
    case MapTileKind::Ice:
        crop = cropFromAutoCell(TilesetKind::IceAuto, 15, 0);
        return true;
    case MapTileKind::Pit:
        crop = cropFromAutoCell(TilesetKind::PitAuto, 15, 0);
        return true;
    case MapTileKind::Rubble:
        crop = cropFromCell(TilesetKind::Battlefield, 0, 0);
        return true;
    case MapTileKind::Cover:
        crop = cropFromCell(TilesetKind::Battlefield, 1, 0);
        return true;
    case MapTileKind::Chasm:
        crop = cropFromAutoCell(TilesetKind::ChasmAuto, 15, 0);
        return true;
    case MapTileKind::Bridge:
        crop = cropFromCell(TilesetKind::Battlefield, 2, 0);
        return true;
    }
    return false;
}
} // namespace
