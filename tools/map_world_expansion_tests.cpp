#include "Models/Map/MapWorldExpansion.hpp"

#include <cmath>
#include <iostream>

namespace
{
bool close(float actual, float expected)
{
    return std::abs(actual - expected) < 0.0001f;
}

bool require(bool condition, const char *message)
{
    if (!condition)
        std::cerr << message << '\n';
    return condition;
}
}

int main()
{
    Campaign campaign;
    MapModel world;
    world.metadata.id = "world";
    world.atlasLevel = MapLevel::World;
    world.isMainMap = true;
    world.width = 4;
    world.height = 4;
    world.revision = 5;
    world.cells.resize(16);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x)
            world.cells[static_cast<std::size_t>(y * 4 + x)].elevation = y * 10 + x;
    world.generation.graph.features.push_back({});
    world.generation.graph.features.back().points.push_back({50.0f, 50.0f});
    world.generation.recipe.constraints.push_back({});
    world.generation.recipe.constraints.back().x = 75.0f;
    world.generation.recipe.constraints.back().y = 25.0f;
    world.spatialReference.calibrated = true;
    world.spatialReference.extentWidth = 40.0f;
    world.spatialReference.extentHeight = 80.0f;
    campaign.maps.push_back(world);

    MapModel child;
    child.metadata.id = "region";
    child.atlasLevel = MapLevel::Region;
    child.parentMapId = "world";
    child.atlasPlacement.parentId = "world";
    child.atlasPlacement.parentRect = {0.25f, 0.25f, 0.5f, 0.5f};
    child.atlasPlacement.parentRevision = 5;
    campaign.maps.push_back(child);
    MapModel staleChild = child;
    staleChild.metadata.id = "stale-region";
    staleChild.atlasPlacement.parentRevision = 3;
    campaign.maps.push_back(staleChild);

    WorldPlaceModel place;
    place.linkedMapId = "world";
    place.hasMapPosition = true;
    place.mapPosition = {0.5, 0.5};
    campaign.worldPlaces.push_back(place);
    NationModel nation;
    NationTerritoryModel territory;
    territory.polygon.push_back({0.75, 0.25});
    nation.territories.push_back(territory);
    campaign.nations.push_back(nation);
    campaign.localPlayerDisplay.publicMapId = "world";
    campaign.localPlayerDisplay.publicCamera.normalizedCenterX = 0.5f;
    campaign.localPlayerDisplay.publicCamera.normalizedCenterY = 0.5f;

    bool ok = true;
    ok &= require(!MapWorldExpansion::apply(campaign, "missing", 2, 2), "Missing world must not grow");
    ok &= require(!MapWorldExpansion::apply(campaign, "world", 117, 0), "Oversized expansion must be rejected");
    ok &= require(campaign.maps.front().width == 4, "Rejected expansion mutated the map");
    ok &= require(MapWorldExpansion::apply(campaign, "world", 2, 4), "World expansion failed");
    const MapModel &expanded = campaign.maps.front();
    ok &= require(expanded.width == 6 && expanded.height == 8 && expanded.cells.size() == 48,
                  "Expanded dimensions or cell count are wrong");
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x)
            ok &= require(expanded.cells[static_cast<std::size_t>(y * 6 + x)].elevation == y * 10 + x,
                          "Existing raster cell shifted from its coordinates");
    ok &= require(expanded.cells[5].fogged && expanded.cells[static_cast<std::size_t>(7 * 6)].fogged,
                  "New world area must start hidden from players");
    ok &= require(close(expanded.generation.graph.features.front().points.front().x, 100.0f / 3.0f) &&
                      close(expanded.generation.graph.features.front().points.front().y, 25.0f),
                  "Generated geography moved in cell-space");
    ok &= require(close(expanded.generation.recipe.constraints.front().x, 50.0f) &&
                      close(expanded.generation.recipe.constraints.front().y, 12.5f),
                  "Generation anchors moved in cell-space");
    ok &= require(close(expanded.spatialReference.extentWidth, 60.0f) &&
                      close(expanded.spatialReference.extentHeight, 160.0f),
                  "Calibrated world extent did not grow at the original scale");
    const MapModel &placedChild = campaign.maps[1];
    ok &= require(close(placedChild.atlasPlacement.parentRect.x, 1.0f / 6.0f) &&
                      close(placedChild.atlasPlacement.parentRect.y, 0.125f) &&
                      close(placedChild.atlasPlacement.parentRect.width, 1.0f / 3.0f) &&
                      close(placedChild.atlasPlacement.parentRect.height, 0.25f) &&
                      placedChild.atlasPlacement.parentRevision == expanded.revision,
                  "Child footprint moved on the expanded world");
    ok &= require(campaign.maps[2].atlasPlacement.parentRevision == 3 &&
                      close(campaign.maps[2].atlasPlacement.parentRect.x, 1.0f / 6.0f),
                  "A pre-existing stale child should retain stale status and physical location");
    ok &= require(close(static_cast<float>(campaign.worldPlaces.front().mapPosition.x), 1.0f / 3.0f) &&
                      close(static_cast<float>(campaign.worldPlaces.front().mapPosition.y), 0.25f),
                  "World place anchor moved");
    ok &= require(close(static_cast<float>(campaign.nations.front().territories.front().polygon.front().x), 0.5f) &&
                      close(static_cast<float>(campaign.nations.front().territories.front().polygon.front().y), 0.125f),
                  "Nation border moved");
    ok &= require(close(campaign.localPlayerDisplay.publicCamera.normalizedCenterX, 1.0f / 3.0f) &&
                      close(campaign.localPlayerDisplay.publicCamera.normalizedCenterY, 0.25f),
                  "The player camera moved from its previous world location");

    MapModel imported = world;
    imported.importedReference.enabled = true;
    ok &= require(!MapWorldExpansion::available(imported, 1, 0), "Imported art should require a separate expansion workflow");
    Campaign legacyCampaign;
    world.isMainMap = false;
    legacyCampaign.maps.push_back(world);
    legacyCampaign.nations.push_back(nation);
    ok &= require(MapWorldExpansion::apply(legacyCampaign, "world", 4, 0) &&
                      close(static_cast<float>(legacyCampaign.nations.front().territories.front().polygon.front().x), 0.375f),
                  "A legacy world root must keep nation borders in place without a main-map marker");
    return ok ? 0 : 1;
}
