#include "Models/Map/MapWorldExpansion.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace MapWorldExpansion
{
bool available(const MapModel &world, int eastCells, int southCells)
{
    return world.atlasLevel == MapLevel::World && world.width >= 4 && world.height >= 4 &&
           world.width <= maximumCellsPerSide && world.height <= maximumCellsPerSide &&
           eastCells >= 0 && southCells >= 0 && (eastCells > 0 || southCells > 0) &&
           eastCells <= maximumCellsPerSide - world.width &&
           southCells <= maximumCellsPerSide - world.height &&
           !world.importedReference.enabled && world.backgroundImagePath.empty();
}

bool apply(Campaign &campaign, const EntityId &worldId, int eastCells, int southCells)
{
    const auto found = std::find_if(campaign.maps.begin(), campaign.maps.end(), [&](const MapModel &candidate) {
        return candidate.metadata.id == worldId;
    });
    if (found == campaign.maps.end() || !available(*found, eastCells, southCells))
        return false;

    auto atlasRoot = std::find_if(campaign.maps.begin(), campaign.maps.end(), [](const MapModel &map) {
        return map.isMainMap && map.atlasLevel == MapLevel::World;
    });
    if (atlasRoot == campaign.maps.end())
        atlasRoot = std::find_if(campaign.maps.begin(), campaign.maps.end(), [](const MapModel &map) {
            return map.atlasLevel == MapLevel::World && map.parentMapId.empty() &&
                   map.atlasPlacement.parentId.empty();
        });
    const bool ownsCampaignTerritories = atlasRoot != campaign.maps.end() && atlasRoot == found;

    MapModel &world = *found;
    const int oldWidth = world.width;
    const int oldHeight = world.height;
    const std::uint64_t oldRevision = world.revision;
    const int newWidth = oldWidth + eastCells;
    const int newHeight = oldHeight + southCells;
    const float xRatio = static_cast<float>(oldWidth) / static_cast<float>(newWidth);
    const float yRatio = static_cast<float>(oldHeight) / static_cast<float>(newHeight);

    // resizeMapCells() cannot be used after changing width: it preserves the
    // flat vector, which would shift every old row into different coordinates.
    std::vector<MapCellModel> cells(static_cast<std::size_t>(newWidth * newHeight));
    for (MapCellModel &cell : cells)
        cell.fogged = true;
    for (int y = 0; y < oldHeight; ++y)
        for (int x = 0; x < oldWidth; ++x)
        {
            const std::size_t oldIndex = static_cast<std::size_t>(y * oldWidth + x);
            if (oldIndex < world.cells.size())
                cells[static_cast<std::size_t>(y * newWidth + x)] = world.cells[oldIndex];
        }
    world.cells = std::move(cells);
    world.width = newWidth;
    world.height = newHeight;
    world.sizePreset = MapSizePreset::Custom;
    world.generation.recipe.sizePreset = MapSizePreset::Custom;

    // Generated graph points and constraints use a 0..100 map coordinate
    // space. Keep their previous cell-space locations when the canvas grows.
    for (MapGeneratedFeature &feature : world.generation.graph.features)
        for (MapFeaturePoint &point : feature.points)
        {
            point.x *= xRatio;
            point.y *= yRatio;
        }
    for (MapGenerationConstraint &constraint : world.generation.recipe.constraints)
    {
        constraint.x *= xRatio;
        constraint.y *= yRatio;
    }
    if (world.spatialReference.calibrated)
    {
        world.spatialReference.extentWidth /= xRatio;
        world.spatialReference.extentHeight /= yRatio;
    }
    world.metadata.touch();
    ++world.revision;

    for (MapModel &child : campaign.maps)
        if (child.atlasPlacement.parentId == worldId)
        {
            MapNormalizedRect &rect = child.atlasPlacement.parentRect;
            rect.x *= xRatio;
            rect.width *= xRatio;
            rect.y *= yRatio;
            rect.height *= yRatio;
            // A pre-existing stale child must stay stale; enlargement is not
            // permission to silently accept unrelated parent edits.
            if (child.atlasPlacement.parentRevision == oldRevision)
                child.atlasPlacement.parentRevision = world.revision;
            child.metadata.touch();
        }
    for (WorldPlaceModel &place : campaign.worldPlaces)
        if (place.linkedMapId == worldId && place.hasMapPosition)
        {
            place.mapPosition.x *= xRatio;
            place.mapPosition.y *= yRatio;
        }
    if (campaign.localPlayerDisplay.publicMapId == worldId)
    {
        campaign.localPlayerDisplay.publicCamera.normalizedCenterX *= xRatio;
        campaign.localPlayerDisplay.publicCamera.normalizedCenterY *= yRatio;
    }
    if (ownsCampaignTerritories)
    {
        for (NationModel &nation : campaign.nations)
            for (NationTerritoryModel &territory : nation.territories)
                for (NormalizedWorldPointModel &point : territory.polygon)
                {
                    point.x *= xRatio;
                    point.y *= yRatio;
                }
    }
    campaign.metadata.touch();
    return true;
}
}
