#include "Models/Map/MapPngExportPlanning.hpp"

#include <cmath>
#include <iostream>
#include <string>

namespace
{
bool require(bool condition, const std::string &message)
{
    if (!condition)
        std::cerr << message << '\n';
    return condition;
}

bool nearlyEqual(float left, float right)
{
    return std::abs(left - right) < 0.0001f;
}
}

int main()
{
    bool ok = true;
    MapModel map;
    map.width = 10;
    map.height = 8;
    resizeMapCells(map);

    ok &= require(calculateMapPngExportSizing(map, 32).valid,
                  "A bounded export size should be valid.");
    ok &= require(!calculateMapPngExportSizing(map, 7).valid,
                  "Export detail below the supported range must fail.");
    ok &= require(mapPngPixelsPerCellForPreset(
                      map, MapPngExportSizePreset::Tactical100PixelsPerCell) == 100,
                  "The tactical preset must retain its documented scale.");

    const MapPngTiledExportPlan tiled = planMapPngTiledExport(
        map, 32, "campaign_map", 160, 1);
    ok &= require(tiled.valid && tiled.columns == 4 && tiled.rows == 3 &&
                      tiled.tiles.size() == 12,
                  "Tiled planning should partition content with bounded overlap.");
    ok &= require(!planMapPngTiledExport(map, 32, "../escape", 160, 1).valid,
                  "Tiled planning must reject unsafe file stems.");

    const MapPngExportRect rect{2, 1, 4, 3};
    ok &= require(pngRectIsValidForMap(rect, map),
                  "A rectangle inside the map should be accepted.");
    ok &= require(!pngRectIsValidForMap({8, 1, 4, 3}, map),
                  "A rectangle beyond the map edge should be rejected.");
    const MapNormalizedRect viewport = pngNormalizedViewport(map, rect);
    ok &= require(nearlyEqual(viewport.x, 0.2f) && nearlyEqual(viewport.y, 0.125f) &&
                      nearlyEqual(viewport.width, 0.4f) && nearlyEqual(viewport.height, 0.375f),
                  "Viewport normalization should retain full-map coordinates.");

    MapTokenModel insideToken;
    insideToken.name = "inside";
    insideToken.x = 3.0f;
    insideToken.y = 2.0f;
    MapTokenModel outsideToken;
    outsideToken.name = "outside";
    outsideToken.x = 9.0f;
    outsideToken.y = 7.0f;
    map.tokens = {insideToken, outsideToken};

    MapRegionModel region;
    region.x = 1;
    region.y = 0;
    region.width = 4;
    region.height = 4;
    map.regions.push_back(region);

    MapRouteModel route;
    route.startX = 0;
    route.startY = 2;
    route.endX = 9;
    route.endY = 2;
    map.routes.push_back(route);

    map.spatialReference.calibrated = true;
    map.spatialReference.originX = 100.0f;
    map.spatialReference.originY = 200.0f;
    map.spatialReference.extentWidth = 50.0f;
    map.spatialReference.extentHeight = 40.0f;

    const MapModel cropped = cropMapForPngExport(map, rect);
    ok &= require(cropped.width == 4 && cropped.height == 3 && cropped.cells.size() == 12,
                  "A crop should own only its selected cells.");
    ok &= require(cropped.tokens.size() == 1 && cropped.tokens.front().name == "inside" &&
                      nearlyEqual(cropped.tokens.front().x, 1.0f) &&
                      nearlyEqual(cropped.tokens.front().y, 1.0f),
                  "A crop should filter and translate tokens.");
    ok &= require(cropped.regions.size() == 1 && cropped.regions.front().x == 0 &&
                      cropped.regions.front().y == 0 && cropped.regions.front().width == 3 &&
                      cropped.regions.front().height == 3,
                  "A crop should intersect regions with its bounds.");
    ok &= require(cropped.routes.size() == 1 && cropped.routes.front().startX == -2 &&
                      cropped.routes.front().endX == 7,
                  "A crop should preserve crossing routes for raster clipping.");
    ok &= require(nearlyEqual(cropped.spatialReference.originX, 110.0f) &&
                      nearlyEqual(cropped.spatialReference.originY, 205.0f) &&
                      nearlyEqual(cropped.spatialReference.extentWidth, 20.0f) &&
                      nearlyEqual(cropped.spatialReference.extentHeight, 15.0f),
                  "A crop should retain calibrated world-space extents.");

    std::string error;
    map.visibility = MapVisibility::GameMasterOnly;
    ok &= require(!mapPermitsPngExport(map, MapRenderAudience::Player, &error) &&
                      !error.empty(),
                  "Player export must fail closed for a private map.");
    map.visibility = MapVisibility::PlayersVisible;
    error.clear();
    ok &= require(mapPermitsPngExport(map, MapRenderAudience::Player, &error) && error.empty(),
                  "An explicitly visible map should permit reviewed player export.");

    return ok ? 0 : 1;
}
