#pragma once

#include "Views/Editor/Widgets/Map/MapTileAtlasResources.hpp"

namespace
{
AtlasCrop cropFromAutoCell(TilesetKind tileset, int mask, int variant)
{
    return cropFromCell(tileset, mask % 8, (mask / 8) * 4 + variant);
}

TilesetKind tilesetForTransitionKey(const std::string &transitionKey)
{
    if (transitionKey == "grass_open_to_water")
        return TilesetKind::TransitionGrassWater;
    if (transitionKey == "grass_open_to_sand")
        return TilesetKind::TransitionGrassDesert;
    if (transitionKey == "grass_open_to_snow")
        return TilesetKind::TransitionGrassSnow;
    if (transitionKey == "grass_open_to_rock")
        return TilesetKind::TransitionGrassRock;
    if (transitionKey == "grass_open_to_swamp")
        return TilesetKind::TransitionGrassSwamp;
    if (transitionKey == "water_to_sand")
        return TilesetKind::TransitionWaterSand;
    if (transitionKey == "snow_to_rock")
        return TilesetKind::TransitionSnowRock;
    if (transitionKey == "dungeon_floor_to_wall")
        return TilesetKind::TransitionDungeonFloorWall;
    return TilesetKind::TransitionGrassWater;
}

std::vector<AtlasCrop> transitionOverlayCrops(const MapTileRenderContext &context)
{
    std::vector<AtlasCrop> crops;
    for (const MapTileResolvedTransitionOverlay &overlay : resolveMapTileTransitionOverlays(context))
    {
        AtlasCrop crop = cropFromAutoCell(tilesetForTransitionKey(overlay.transitionKey), overlay.mask, overlay.variant);
        if (atlasForTileset(crop.tileset).loaded)
            crops.push_back(crop);
    }
    return crops;
}

bool autoCropForTile(const MapTileRenderContext &context, AtlasCrop &crop)
{
    const int mask = connectedMapTileMask(context);
    switch (context.kind)
    {
    case MapTileKind::Empty:
        crop = cropFromAutoCell(TilesetKind::GrassAuto, mask, deterministicMapTileVariant(context.tileX, context.tileY, 1));
        return true;
    case MapTileKind::Dirt:
        crop = cropFromAutoCell(TilesetKind::DirtAuto, mask, deterministicMapTileVariant(context.tileX, context.tileY, 7));
        return true;
    case MapTileKind::Water:
        crop = cropFromAutoCell(TilesetKind::WaterAuto, mask, deterministicMapTileVariant(context.tileX, context.tileY, 2));
        return true;
    case MapTileKind::Road:
        crop = cropFromAutoCell(
            atlasForTileset(TilesetKind::RoadMaskPrototype).loaded ? TilesetKind::RoadMaskPrototype : TilesetKind::RoadAuto,
            mask,
            deterministicMapTileVariant(context.tileX, context.tileY, 3));
        return true;
    case MapTileKind::Stone:
        crop = cropFromAutoCell(TilesetKind::StoneAuto, mask, deterministicMapTileVariant(context.tileX, context.tileY, 8));
        return true;
    case MapTileKind::Snow:
        crop = cropFromAutoCell(TilesetKind::SnowAuto, mask, deterministicMapTileVariant(context.tileX, context.tileY, 9));
        return true;
    case MapTileKind::Desert:
        crop = cropFromAutoCell(TilesetKind::DesertAuto, mask, deterministicMapTileVariant(context.tileX, context.tileY, 10));
        return true;
    case MapTileKind::Swamp:
        crop = cropFromAutoCell(TilesetKind::SwampAuto, mask, deterministicMapTileVariant(context.tileX, context.tileY, 11));
        return true;
    case MapTileKind::Ice:
        crop = cropFromAutoCell(TilesetKind::IceAuto, mask, deterministicMapTileVariant(context.tileX, context.tileY, 12));
        return true;
    case MapTileKind::Room:
        crop = cropFromAutoCell(TilesetKind::DungeonFloorAuto, mask, deterministicMapTileVariant(context.tileX, context.tileY, 4));
        return true;
    case MapTileKind::Wall:
        crop = cropFromAutoCell(TilesetKind::DungeonWallAuto, mask, deterministicMapTileVariant(context.tileX, context.tileY, 5));
        return true;
    case MapTileKind::Hazard:
        crop = cropFromAutoCell(TilesetKind::HazardAuto, mask, deterministicMapTileVariant(context.tileX, context.tileY, 6));
        return true;
    case MapTileKind::Lava:
        crop = cropFromAutoCell(TilesetKind::LavaAuto, mask, deterministicMapTileVariant(context.tileX, context.tileY, 13));
        return true;
    case MapTileKind::Pit:
        crop = cropFromAutoCell(TilesetKind::PitAuto, mask, deterministicMapTileVariant(context.tileX, context.tileY, 14));
        return true;
    case MapTileKind::Chasm:
        crop = cropFromAutoCell(TilesetKind::ChasmAuto, mask, deterministicMapTileVariant(context.tileX, context.tileY, 15));
        return true;
    case MapTileKind::Forest:
    {
        static const std::array<std::pair<int, int>, 2> cells = {{{0, 0}, {1, 0}}};
        const auto [column, row] = cells[contextualSymbolCellIndex(context, 21, static_cast<int>(cells.size()))];
        crop = cropFromCell(TilesetKind::Nature, column, row);
        return true;
    }
    case MapTileKind::Mountain:
    {
        if (atlasForTileset(TilesetKind::MountainRangePrototype).loaded)
        {
            crop = mountainPrototypeCrop(context);
            return true;
        }
        static const std::array<std::pair<int, int>, 2> cells = {{{0, 1}, {1, 1}}};
        const auto [column, row] = cells[contextualSymbolCellIndex(context, 22, static_cast<int>(cells.size()))];
        crop = cropFromCell(TilesetKind::Nature, column, row);
        return true;
    }
    case MapTileKind::Hill:
    {
        static const std::array<std::pair<int, int>, 2> cells = {{{6, 0}, {7, 0}}};
        const auto [column, row] = cells[contextualSymbolCellIndex(context, 23, static_cast<int>(cells.size()))];
        crop = cropFromCell(TilesetKind::Nature, column, row);
        return true;
    }
    case MapTileKind::Settlement:
    {
        static const std::array<std::pair<int, int>, 7> cells = {{{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 0}, {6, 0}}};
        const auto [column, row] = cells[deterministicMapTileIndex(context.tileX, context.tileY, 24, static_cast<int>(cells.size()))];
        crop = cropFromCell(TilesetKind::Settlement, column, row);
        return true;
    }
    case MapTileKind::Building:
    {
        static const std::array<std::pair<int, int>, 4> cells = {{{0, 0}, {1, 0}, {2, 0}, {3, 0}}};
        const auto [column, row] = cells[deterministicMapTileIndex(context.tileX, context.tileY, 25, static_cast<int>(cells.size()))];
        crop = cropFromCell(TilesetKind::Settlement, column, row);
        return true;
    }
    case MapTileKind::Cave:
    {
        static const std::array<std::pair<int, int>, 2> cells = {{{4, 1}, {5, 1}}};
        const auto [column, row] = cells[deterministicMapTileIndex(context.tileX, context.tileY, 26, static_cast<int>(cells.size()))];
        crop = cropFromCell(TilesetKind::Nature, column, row);
        return true;
    }
    case MapTileKind::Landmark:
    {
        static const std::array<std::pair<int, int>, 6> cells = {{{5, 0}, {6, 0}, {7, 0}, {0, 3}, {1, 3}, {3, 3}}};
        const auto [column, row] = cells[deterministicMapTileIndex(context.tileX, context.tileY, 27, static_cast<int>(cells.size()))];
        crop = cropFromCell(TilesetKind::Settlement, column, row);
        return true;
    }
    case MapTileKind::Rubble:
    {
        static const std::array<std::pair<int, int>, 2> cells = {{{0, 0}, {7, 2}}};
        const auto [column, row] = cells[deterministicMapTileIndex(context.tileX, context.tileY, 28, static_cast<int>(cells.size()))];
        crop = cropFromCell(TilesetKind::Battlefield, column, row);
        return true;
    }
    case MapTileKind::Cover:
    {
        static const std::array<std::pair<int, int>, 5> cells = {{{1, 0}, {5, 2}, {4, 4}, {5, 4}, {7, 4}}};
        const auto [column, row] = cells[deterministicMapTileIndex(context.tileX, context.tileY, 29, static_cast<int>(cells.size()))];
        crop = cropFromCell(TilesetKind::Battlefield, column, row);
        return true;
    }
    default:
        return false;
    }
}

} // namespace
