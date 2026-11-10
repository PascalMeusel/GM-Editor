#include "Models/Map/MapPngExport.hpp"
#include "Models/Map/MapFogTexture.hpp"
#include "Models/Map/MapAtlasWaterInk.hpp"
#include "Models/Map/MapClassicIllustration.hpp"
#include "Models/Media/RgbaImageLoader.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
void require(bool condition, const std::string &message)
{
    if (!condition)
        throw std::runtime_error(message);
}

struct Fixture
{
    std::filesystem::path root = std::filesystem::temp_directory_path() /
        ("gm-map-trace-export-tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Fixture() { std::filesystem::create_directories(root); }
    ~Fixture() { std::error_code error; std::filesystem::remove_all(root, error); }
};

LoadedRgbaImage read(const std::filesystem::path &path)
{
    LoadedRgbaImage image;
    require(loadRgbaImage(path, image), "Could not decode " + path.string());
    return image;
}

std::array<unsigned char, 4> pixel(const LoadedRgbaImage &image, int x, int y)
{
    require(x >= 0 && y >= 0 && x < image.width && y < image.height, "Invalid test pixel.");
    const std::size_t offset = static_cast<std::size_t>((y * image.width + x) * 4);
    return {image.pixels[offset], image.pixels[offset + 1], image.pixels[offset + 2], image.pixels[offset + 3]};
}

LoadedRgbaImage render(const MapModel &map, const MapRenderPlanRequest &request,
                       const std::filesystem::path &path, int pixelsPerCell = 16)
{
    std::string error;
    require(exportMapPng(map, path, compileMapRenderPlan(map, request), pixelsPerCell, &error), error);
    return read(path);
}

MapModel traceMap()
{
    MapModel map;
    map.metadata.id = "trace_export_fixture";
    map.width = 12;
    map.height = 12;
    map.atlasLevel = MapLevel::Site;
    map.visibility = MapVisibility::PlayersVisible;
    map.generation.continuousRendering = false;
    resizeMapCells(map);
    for (auto &cell : map.cells) { cell.kind = MapTileKind::Dirt; cell.fogged = false; }
    MapGeneratedFeature area;
    area.id = "concave_terrain_trace";
    area.kind = MapGeneratedFeatureKind::TerrainRegion;
    area.materialId = "trace.terrain_area";
    area.closed = true;
    area.gmOnly = false;
    area.discovered = true;
    area.points = {{1, 1}, {10, 1}, {10, 8}, {7, 8}, {7, 4}, {4, 4}, {4, 8}, {1, 8}};
    MapGeneratedFeature coast;
    coast.id = "coastline_trace";
    coast.kind = MapGeneratedFeatureKind::Coastline;
    coast.materialId = "trace.coastline";
    coast.gmOnly = false;
    coast.discovered = true;
    coast.points = {{1, 9}, {5, 9}, {10, 9}};
    map.generation.graph.features = {area, coast};
    return map;
}

void testTraceFidelityAndPolicy(const std::filesystem::path &root)
{
    MapModel map = traceMap();
    MapPngExportOptions options;
    options.pixelsPerCell = 16;
    options.applyFog = false;
    options.showLabels = false;
    options.showGrid = false;
    const auto gmRequest = mapPngExportRenderPlanRequest(options);
    MapModel baseline = map;
    baseline.generation.graph.features.clear();
    const auto empty = render(baseline, gmRequest, root / "empty.png");
    const auto gm = render(map, gmRequest, root / "gm.png");
    require(pixel(gm, 40, 104) != pixel(empty, 40, 104), "PNG omitted the traced terrain fill.");
    require(pixel(gm, 88, 104) == pixel(empty, 88, 104), "PNG filled across the concave trace notch.");
    const auto coast = pixel(gm, 80, 144);
    require(coast[2] > coast[0] && coast != pixel(empty, 80, 144), "PNG omitted the cyan coastline trace.");

    std::string error;
    require(exportMapPng(map, root / "options.png", options, &error), error);
    require(read(root / "options.png").pixels == gm.pixels, "Options and reviewed-plan PNG paths disagree for traces.");
    const MapPngExportRect crop{3, 2, 7, 8};
    require(exportMapPngRegion(map, root / "crop.png", compileMapRenderPlan(map, gmRequest), crop, 16, &error), error);
    const auto cropped = read(root / "crop.png");
    for (int y = 0; y < cropped.height; ++y)
        for (int x = 0; x < cropped.width; ++x)
            require(pixel(cropped, x, y) == pixel(gm, x + crop.x * 16, y + crop.y * 16),
                    "Current-view export changed full-map trace geometry or fill.");

    auto playerRequest = gmRequest;
    playerRequest.audience = MapRenderAudience::Player;
    const auto publicTrace = render(map, playerRequest, root / "public.png");
    require(publicTrace.pixels == gm.pixels, "Public traces should remain visible with all cells revealed.");
    map.generation.graph.features[0].gmOnly = true;
    map.generation.graph.features[1].discovered = false;
    require(render(map, playerRequest, root / "private.png").pixels == empty.pixels,
            "GM-only or undiscovered traces leaked into player PNG.");
    map = traceMap();
    for (auto &cell : map.cells) cell.fogged = true;
    const auto fogged = render(map, playerRequest, root / "fogged.png");
    require(pixel(fogged,40,104)==gm::map_render::fogTextureAt(40.5f/16,104.5f/16) &&
                pixel(fogged,80,144)==gm::map_render::fogTextureAt(80.5f/16,144.5f/16),
            "Mandatory player fog must cover traces as well as base terrain.");
    auto hiddenEmpty=map;hiddenEmpty.generation.graph.features.clear();
    for(auto &cell:hiddenEmpty.cells)cell.kind=MapTileKind::Water;
    require(render(hiddenEmpty,playerRequest,root/"fogged-other-content.png").pixels==fogged.pixels,
            "Cloud fog must not reveal underlying terrain or traces through color or alpha.");
    MapPngExportRect fogCrop;fogCrop.x=2;fogCrop.y=3;fogCrop.width=4;fogCrop.height=4;
    std::string fogError;
    require(exportMapPngRegion(map,root/"fog-crop.png",compileMapRenderPlan(map,playerRequest),fogCrop,16,&fogError),fogError);
    LoadedRgbaImage fogCropImage;require(loadRgbaImage(root/"fog-crop.png",fogCropImage),"Could not read fog crop");
    for(int y=0;y<fogCropImage.height;++y)for(int x=0;x<fogCropImage.width;++x)
        require(pixel(fogCropImage,x,y)==pixel(fogged,x+32,y+48),"Cloud texture moved when cropping an export.");
    require(render(map, gmRequest, root / "gm-fog-off.png").pixels == gm.pixels,
            "An explicit GM fog-off export should retain traced content.");

    map = traceMap();
    auto waterOnly = gmRequest;
    waterOnly.layers = MapRenderPlanWater | MapRenderPlanLabels;
    map.generation.graph.features[0].name = "Named terrain trace";
    const auto noTerrain = render(map, waterOnly, root / "no-terrain.png");
    const auto emptyNoTerrain = render(baseline, waterOnly, root / "empty-no-terrain.png");
    require(pixel(noTerrain, 40, 104) == pixel(emptyNoTerrain, 40, 104),
            "Labels retained a traced fill after Terrain was disabled.");
    require(pixel(noTerrain, 80, 144) != pixel(emptyNoTerrain, 80, 144),
            "Disabling Terrain must not disable the Water coastline trace.");
    auto noFeatures = playerRequest;
    noFeatures.additionalGeneratedFeatureVisibility = [](const MapGeneratedFeature &) { return false; };
    require(render(map, noFeatures, root / "external-filter.png").pixels == empty.pixels,
            "Export trace geometry bypassed the reviewed feature visibility predicate.");
}

void testTracesAcrossRealTileBoundary(const std::filesystem::path &root)
{
    MapModel map = traceMap();
    map.width = 2050; // 16400 pixels at 8px/cell crosses the real 16384px limit.
    map.height = 8;
    resizeMapCells(map);
    for (auto &cell : map.cells) { cell.kind = MapTileKind::Dirt; cell.fogged = false; }
    map.generation.graph.features[0].points = {{2044, 1}, {2050, 1}, {2050, 7}, {2048, 7}, {2048, 4}, {2046, 4}, {2046, 7}, {2044, 7}};
    map.generation.graph.features[1].points = {{2044, 3}, {2047, 3}, {2050, 3}};
    MapPngExportOptions options;
    options.pixelsPerCell = 8;
    options.playerSafe = true;
    options.applyFog = false; // Player path must still apply its mandatory mask.
    options.showLabels = false;
    const auto plan = compileMapRenderPlan(map, mapPngExportRenderPlanRequest(options));
    std::string error;
    MapPngTiledExportPlan tiled;
    require(exportMapPngTiled(map, root / "tiles", "trace", plan, 8, false, &tiled, &error), error);
    require(tiled.tiles.size() == 2, "Test did not cross a real tile boundary.");
    for (std::size_t index = 0; index < tiled.tiles.size(); ++index)
    {
        const auto &tile = tiled.tiles[index];
        const auto regionPath = root / ("region-" + std::to_string(index) + ".png");
        require(exportMapPngRegion(map, regionPath, plan, tile.render, 8, &error), error);
        const auto image = read(root / "tiles" / tile.fileName);
        require(image.pixels == read(regionPath).pixels, "Tile and current-view trace geometry disagree.");
        const int traceX = (2046 - tile.render.x) * 8;
        if (traceX >= 0 && traceX < image.width)
        {
            const auto coast = pixel(image, traceX, 3 * 8);
            require(coast[2] > coast[0], "A coastline crossing the tile edge disappeared.");
        }
    }
    MapPngTiledExportPlan optionsTiles;
    require(exportMapPngTiled(map, root / "options-tiles", "trace", options, false, &optionsTiles, &error), error);
    require(optionsTiles.tiles.size() == tiled.tiles.size(), "Tiled options and plan paths disagree.");
    for (std::size_t index = 0; index < tiled.tiles.size(); ++index)
        require(read(root / "tiles" / tiled.tiles[index].fileName).pixels ==
                    read(root / "options-tiles" / optionsTiles.tiles[index].fileName).pixels,
                "Tiled options and reviewed-plan paths changed trace output.");
}

void requireSubimage(const LoadedRgbaImage &whole, const LoadedRgbaImage &part,
                     const MapPngExportRect &rect, int pixelsPerCell, const char *description)
{
    require(part.width == rect.width * pixelsPerCell && part.height == rect.height * pixelsPerCell,
            std::string(description) + " has incorrect dimensions.");
    for (int y = 0; y < part.height; ++y)
        for (int x = 0; x < part.width; ++x)
            if (pixel(part, x, y) != pixel(whole, x + rect.x * pixelsPerCell, y + rect.y * pixelsPerCell))
                throw std::runtime_error(std::string(description) + " changed shared artwork/road pixels at local " +
                                         std::to_string(x) + "," + std::to_string(y) + ".");
}

void testIllustratedPlanGeometryAcrossCropAndTiles(const std::filesystem::path &root, int styleVersion)
{
    std::filesystem::create_directories(root);
    MapModel map;
    map.metadata.id = "illustrated_geometry_offsets";
    // This width fits a whole 16376px PNG, while the real tiled planner's
    // overlap allowance creates a second tile with a nonzero source origin.
    map.width = 2047;
    map.height = 16;
    map.atlasLevel = MapLevel::World;
    map.scope = MapScope::World;
    map.visibility = MapVisibility::PlayersVisible;
    map.generation.continuousRendering = true;
    map.generation.recipe.styleId = gm::map_render::kIllustratedAtlasStyleId;
    map.generation.recipe.styleSnapshotId = styleVersion == 3 ? "illustrated_atlas_v3" :
        styleVersion == 1 ? gm::map_render::kIllustratedAtlasSnapshotId : gm::map_render::kIllustratedAtlasV2SnapshotId;
    map.generation.recipe.styleVersion = styleVersion;
    map.generation.recipe.generatorVersion = styleVersion == 3 ? 8 : 6;
    map.generation.recipe.seed = 7391;
    resizeMapCells(map);
    for (auto &cell : map.cells) { cell.kind = MapTileKind::Dirt; cell.fogged = false; }
    MapGeneratedFeature mountain;
    mountain.id = "offset_mountain";
    mountain.kind = MapGeneratedFeatureKind::MountainRange;
    mountain.gmOnly = false;
    mountain.discovered = true;
    mountain.points = {{2038.0f, 7.0f}, {2042.0f, 6.6f}, {2046.8f, 7.0f}};
    MapGeneratedFeature road;
    road.id = "offset_road";
    road.kind = MapGeneratedFeatureKind::Road;
    road.width = 0.9f;
    road.gmOnly = false;
    road.discovered = true;
    // Both ends extend beyond the dedicated crop. Recomputing smoothing
    // against cropped cells rejects out-of-bounds controls and changes it.
    road.points = {{2034.5f, 12.5f}, {2040.5f, 12.5f}, {2043.5f, 11.5f}, {2046.5f, 12.5f}};
    map.generation.graph.features = {mountain, road};

    constexpr int pixelsPerCell = 8;
    MapPngExportOptions options;
    options.pixelsPerCell = pixelsPerCell;
    options.applyFog = false;
    options.showLabels = false;
    options.showGrid = false;
    const auto plan = compileMapRenderPlan(map, mapPngExportRenderPlanRequest(options));
    require(gm::map_render::usesIllustratedAtlas(map) &&
                std::any_of(plan.atlasInk.begin(), plan.atlasInk.end(), [](const auto &ink) { return ink.filled; }),
            "Illustrated crop fixture must contain actual mountain-face primitives.");
    const auto roadGeometry = std::find_if(plan.primitives.begin(), plan.primitives.end(), [&](const auto &primitive) {
        return primitive.sourceId == road.id && primitive.kind == MapRenderPrimitiveKind::GeneratedGeometry;
    });
    require(roadGeometry != plan.primitives.end() &&
                (styleVersion == 3 ? roadGeometry->points.size() == road.points.size()
                                   : roadGeometry->points.size() > road.points.size()),
            "Crop fixture must use the versioned road contract: V8 keeps fitted controls; older renderers resolve extra points.");

    std::string error;
    require(exportMapPng(map, root / "atlas-whole.png", plan, pixelsPerCell, &error), error);
    const auto whole = read(root / "atlas-whole.png");
    int facePixels = 0, facePixelsInLastTile = 0;
    for (int y = 3 * pixelsPerCell; y < 10 * pixelsPerCell; ++y)
        for (int x = 2038 * pixelsPerCell; x < whole.width; ++x)
        {
            const auto color = pixel(whole, x, y);
            const bool mountainFace = std::any_of(plan.atlasInk.begin(), plan.atlasInk.end(), [&](const auto &ink) {
                return ink.filled && ink.color.a == 255 &&
                       color == std::array<unsigned char, 4>{ink.color.r, ink.color.g, ink.color.b, 255};
            });
            if (mountainFace)
            {
                ++facePixels;
                if (x >= 2045 * pixelsPerCell) ++facePixelsInLastTile;
            }
        }
    require(facePixels > 50 && facePixelsInLastTile > 4,
            "Illustrated geometry comparison requires real mountain-face pixels, including the nonzero-origin tile.");
    const auto paper = pixel(whole, 8, 8);
    int roadPixels = 0;
    for (const auto &point : roadGeometry->points)
    {
        const auto color = pixel(whole, static_cast<int>(std::lround(point.x * pixelsPerCell)),
                                 static_cast<int>(std::lround(point.y * pixelsPerCell)));
        if (static_cast<int>(color[0]) + 20 < paper[0]) ++roadPixels;
    }
    require(roadPixels > 3, "Illustrated geometry comparison requires a visibly rendered road.");

    const MapPngExportRect crop{2039, 2, 7, 13};
    require(exportMapPngRegion(map, root / "atlas-crop.png", plan, crop, pixelsPerCell, &error), error);
    requireSubimage(whole, read(root / "atlas-crop.png"), crop, pixelsPerCell, "Illustrated current-view PNG");
    MapPngTiledExportPlan tiled;
    require(exportMapPngTiled(map, root / "atlas-tiles", "atlas", plan, pixelsPerCell, false, &tiled, &error), error);
    require(tiled.tiles.size() == 2 && tiled.tiles.back().render.x > 0,
            "Illustrated geometry fixture did not exercise a nonzero-origin tile.");
    for (const auto &tile : tiled.tiles)
        requireSubimage(whole, read(root / "atlas-tiles" / tile.fileName), tile.render, pixelsPerCell,
                        "Illustrated tiled PNG");
}

void testRoadShoulderEnteringCropWithoutControls(const std::filesystem::path &root)
{
    MapModel map = traceMap();
    map.atlasLevel = MapLevel::World;
    map.generation.recipe.generatorVersion = 6;
    map.playerRelease.waysAndRoutes = true;
    MapGeneratedFeature road;
    road.id = "road_outside_crop";
    road.kind = MapGeneratedFeatureKind::Road;
    road.locked = true;
    road.discovered = true;
    road.gmOnly = false;
    road.points = {{10.02f, 2.5f}, {10.02f, 9.5f}};
    map.generation.graph.features = {road};
    constexpr int pixelsPerCell = 16;
    const MapPngExportRect crop{0, 1, 10, 10};
    MapPngExportOptions options;
    options.pixelsPerCell = pixelsPerCell;
    options.applyFog = false;
    options.showLabels = false;
    for (bool player : {false, true})
    {
        options.playerSafe = player;
        const auto plan = compileMapRenderPlan(map, mapPngExportRenderPlanRequest(options));
        const std::string audience = player ? "player" : "gm";
        std::string error;
        const auto wholePath = root / (audience + "-edge-whole.png");
        require(exportMapPng(map, wholePath, plan, pixelsPerCell, &error), error);
        const auto whole = read(wholePath);
        require(pixel(whole, 159, 80) != pixel(whole, 0, 80),
                "Crop-edge fixture must contain visible road shoulder pixels left of the outside centerline.");
        const auto cropPath = root / (audience + "-edge-crop.png");
        require(exportMapPngRegion(map, cropPath, plan, crop, pixelsPerCell, &error), error);
        requireSubimage(whole, read(cropPath), crop, pixelsPerCell,
                        "Road shoulder entering a crop without authored controls");
    }
    road.gmOnly = true;
    map.generation.graph.features = {road};
    options.playerSafe = true;
    const auto privatePlan = compileMapRenderPlan(map, mapPngExportRenderPlanRequest(options));
    std::string error;
    require(exportMapPngRegion(map, root / "private-edge-crop.png", privatePlan, crop, pixelsPerCell, &error), error);
    const auto privateCrop = read(root / "private-edge-crop.png");
    require(pixel(privateCrop, 159, 64) == pixel(privateCrop, 0, 64),
            "Keeping crop-crossing line geometry must not restore a filtered GM-only road.");
}

void testV2AtlasWaterAndTerrainOffsets(const std::filesystem::path &root)
{
    std::filesystem::create_directories(root);
    MapModel map;
    map.metadata.id = "atlas_water_fixture";
    map.width = 36; map.height = 24;
    map.atlasLevel = MapLevel::Region; map.scope = MapScope::Local;
    map.visibility = MapVisibility::PlayersVisible;
    map.generation.continuousRendering = true;
    map.generation.recipe.styleId = gm::map_render::kIllustratedAtlasStyleId;
    map.generation.recipe.styleVersion = 2;
    map.generation.recipe.styleSnapshotId = gm::map_render::kIllustratedAtlasV2SnapshotId;
    map.generation.recipe.generatorVersion = 7;
    map.generation.recipe.seed = 39247;
    resizeMapCells(map);
    for (int y = 0; y < map.height; ++y)
        for (int x = 0; x < map.width; ++x)
        {
            auto &cell = *mapCellAt(map, x, y);
            cell.kind = x < 12 || (x >= 22 && x < 29 && y >= 6 && y < 18)
                ? MapTileKind::Water : MapTileKind::Dirt;
            cell.fogged = false;
        }
    MapPngExportOptions options;
    options.applyFog = false; options.showLabels = false; options.showGrid = false;
    const auto request = mapPngExportRenderPlanRequest(options);
    auto plan = compileMapRenderPlan(map, request);
    require(!plan.atlasWaterInk.empty(), "V2 Region must resolve shoreline geometry through the shared plan.");
    require(plan.contentHash == compileMapRenderPlan(map, request).contentHash,
            "Seeded water artwork must be deterministic across repeated plan compilation.");
    int waves = 0, coastFills = 0;
    for (const auto &ink : plan.atlasWaterInk)
    {
        if (ink.filled) ++coastFills;
        if (!ink.filled && ink.color.a == 165)
        {
            ++waves;
            for (const auto &p : ink.points)
                require(p.x > 1 && p.x < 11 && p.y > 1 && p.y < 23,
                        "Ocean wave groups entered an enclosed lake, shore, or unknown map edge.");
        }
    }
    require(waves > 0 && coastFills > 0, "Water fixture must exercise actual waves and the nearshore shelf.");
    const auto wholeRegion = render(map, request, root / "region-water.png", 16);
    int coastPixels = 0;
    int coastSampleX = -1, coastSampleY = -1;
    for (int y = 0; y < wholeRegion.height; ++y)
        for (int x = 0; x < wholeRegion.width; ++x)
            if (pixel(wholeRegion, x, y) == std::array<unsigned char, 4>{75, 113, 116, 255})
            { ++coastPixels; coastSampleX = x; coastSampleY = y; }
    require(coastPixels > 40, "Region PNG omitted the shared V2 shoreline ink.");
    auto noWater = request;
    noWater.layers &= ~MapRenderPlanWater;
    require(compileMapRenderPlan(map, noWater).atlasWaterInk.empty(),
            "Disabling Water must remove every V2 shelf, shore, and ocean wave primitive.");
    auto waterOnly = request; waterOnly.layers = MapRenderPlanWater;
    require(!compileMapRenderPlan(map, waterOnly).atlasWaterInk.empty(),
            "The V2 Water pass must not depend on enabling the Terrain pass.");
    require(pixel(render(map, waterOnly, root / "region-water-only.png", 16), coastSampleX, coastSampleY) ==
                pixel(wholeRegion, coastSampleX, coastSampleY),
            "PNG omitted V2 shoreline ink when only Water was enabled.");
    require(pixel(render(map, noWater, root / "region-water-disabled.png", 16), coastSampleX, coastSampleY) !=
                pixel(wholeRegion, coastSampleX, coastSampleY),
            "PNG retained the V2 shoreline after Water was disabled.");
    auto playerRequest = request; playerRequest.audience = MapRenderAudience::Player;
    require(render(map, playerRequest, root / "region-public.png", 16).pixels == wholeRegion.pixels,
            "Revealed public coastline differs between GM and player output.");
    for (auto &cell : map.cells) cell.fogged = true;
    const auto fogged = render(map, playerRequest, root / "region-fogged.png", 16);
    require(pixel(fogged,5*16,8*16)==gm::map_render::fogTextureAt(5+.5f/16,8+.5f/16) &&
                pixel(fogged,16*16,8*16)==gm::map_render::fogTextureAt(16+.5f/16,8+.5f/16),
            "Mandatory player fog must conceal water artwork as well as land.");
    for (const auto visibility : {MapVisibility::GameMasterOnly, MapVisibility::PlayersKnown})
    {
        map.visibility = visibility;
        const auto unreleased = compileMapRenderPlan(map, playerRequest);
        require(unreleased.layers == MapRenderPlanFog && unreleased.atlasWaterInk.empty(),
                "Private and merely known maps must not compile player drawing layers.");
    }
    map.generation.recipe.styleVersion = 1;
    map.generation.recipe.styleSnapshotId = gm::map_render::kIllustratedAtlasSnapshotId;
    require(compileMapRenderPlan(map, request).atlasWaterInk.empty(),
            "Saved V1 maps must keep the legacy coastline renderer.");

    // A real nonzero-origin tile, with coastal water plus a nonuniform forest
    // edge and land wash. Rebuilding the projection in crop coordinates changes
    // these pixels even when all vector strokes have correct offsets.
    map.metadata.id = "atlas_water_tile_fixture";
    map.width = 2047; map.height = 16;
    map.visibility = MapVisibility::PlayersVisible;
    map.generation.recipe.styleVersion = 2;
    map.generation.recipe.styleSnapshotId = gm::map_render::kIllustratedAtlasV2SnapshotId;
    resizeMapCells(map);
    for (int y = 0; y < map.height; ++y)
        for (int x = 0; x < map.width; ++x)
        {
            auto &cell = *mapCellAt(map, x, y);
            const float coast = 7.5f + std::sin((x - 2025) * .43f) * 1.3f;
            cell.kind = x >= 2025 && y < coast ? MapTileKind::Water
                : x >= 2038 && x < 2044 && y >= 10 ? MapTileKind::Forest : MapTileKind::Dirt;
            cell.fogged = false; cell.wayKind = MapWayKind::None;
        }
    MapGeneratedFeature river;
    river.id = "crop_river"; river.kind = MapGeneratedFeatureKind::River;
    river.gmOnly = false; river.discovered = true;
    river.points = {{2034.5f, 11.5f}, {2039.5f, 13.5f}, {2044.5f, 11.5f}, {2046.5f, 9.5f}};
    map.generation.graph.features = {river};
    constexpr int pixelsPerCell = 8;
    plan = compileMapRenderPlan(map, request);
    const auto *riverInk = mapRenderPlanGeneratedGeometry(plan, river.id);
    require(riverInk && riverInk->size() > river.points.size(),
            "V2 river fixture must resolve its final curve once in the shared plan.");
    std::string error;
    require(exportMapPng(map, root / "water-whole.png", plan, pixelsPerCell, &error), error);
    const auto whole = read(root / "water-whole.png");
    const MapPngExportRect crop{2039, 2, 7, 13};
    require(exportMapPngRegion(map, root / "water-crop.png", plan, crop, pixelsPerCell, &error), error);
    requireSubimage(whole, read(root / "water-crop.png"), crop, pixelsPerCell,
                    "V2 coastline, river and full-source terrain crop");
    MapPngTiledExportPlan tiled;
    require(exportMapPngTiled(map, root / "water-tiles", "water", plan, pixelsPerCell, false, &tiled, &error), error);
    require(tiled.tiles.size() == 2 && tiled.tiles.back().render.x > 0,
            "V2 water fixture must cross a real nonzero-origin tile boundary.");
    for (const auto &tile : tiled.tiles)
        requireSubimage(whole, read(root / "water-tiles" / tile.fileName), tile.render, pixelsPerCell,
                        "V2 coastline, river and full-source terrain tile");
}

void testDirectArtworkHelperUsesBentTravelRoute()
{
    MapModel map = traceMap();
    map.atlasLevel = MapLevel::World;
    MapGeneratedFeature road;
    road.id = "bent_network";
    road.kind = MapGeneratedFeatureKind::Road;
    road.locked = true;
    road.points = {{2.5f, 3.5f}, {2.5f, 8.5f}, {9.5f, 8.5f}, {9.5f, 3.5f}};
    map.generation.graph.features = {road};
    MapRouteModel route;
    route.metadata.id = "bent_travel_route";
    route.startX = 2; route.startY = 3; route.endX = 9; route.endY = 3;
    map.routes = {route};
    const auto path = gm::map_route_presentation::build(map, route);
    require(path.followsRoadNetwork() &&
                std::any_of(path.points.begin(), path.points.end(), [](const auto &point) { return point.y >= 8.0f; }),
            "Travel-route fixture must follow its bent road network.");
    const auto corridors = gm::map_render::illustratedAtlasRoadCorridors(map);
    const auto found = std::find_if(corridors.begin(), corridors.end(), [&](const auto &corridor) {
        return corridor.featureId == route.metadata.id;
    });
    require(found != corridors.end() &&
                gm::map_geometry::roadCorridorContainsPoint(*found, {6.0f, 8.5f}) &&
                !gm::map_geometry::roadCorridorContainsPoint(*found, {6.0f, 3.5f}),
            "Direct artwork clearance must follow the visible travel route, not clear its unused straight chord.");
}

void testContinuousRiverMesh(const std::filesystem::path &root)
{
    MapModel map;
    map.width=20;map.height=16;map.atlasLevel=MapLevel::World;
    map.generation.continuousRendering=true;
    map.generation.recipe.styleId="illustrated_atlas";
    map.generation.recipe.styleVersion=5;map.generation.recipe.styleSnapshotId="illustrated_atlas_v5";
    map.generation.recipe.generatorVersion=11;
    resizeMapCells(map);
    for(int y=0;y<map.height;++y)for(int x=0;x<map.width;++x)
        map.cells[y*map.width+x].kind=x>=15?MapTileKind::Water:MapTileKind::Empty;
    MapGeneratedFeature river;
    river.id="continuous_river";river.kind=MapGeneratedFeatureKind::River;
    river.points={{2,4},{5,4},{9,8},{14,10},{17,10}};river.width=2.f;
    map.generation.graph.features={river};
    MapRenderPlanRequest request;request.applyFog=false;
    request.layers=MapRenderPlanTerrain|MapRenderPlanWater;
    const auto plan=compileMapRenderPlan(map,request);
    const auto points=gm::map_render::illustratedAtlasFinalRiverPoints(map,river);
    const float radius=river.width*gm::map_render::atlasPresentationProfile(map).riverWidthCells*.5f;
    for(int ppc:{32,96})
    {
        const auto path=root/("river-mesh-"+std::to_string(ppc)+".png");
        const auto image=render(map,request,path,ppc);
        // Sample the centre and both inner banks between every cross section.
        // No pale background crack may cross the continuous upstream ribbon.
        for(std::size_t i=2;i+2<points.size();++i)
        {
            const auto a=points[i-1],b=points[i];
            if(b.x>=13.5f)continue;
            const float dx=b.x-a.x,dy=b.y-a.y,len=std::hypot(dx,dy);
            if(len<.002f)continue;
            for(float t:{.25f,.5f,.75f})for(float side:{-.45f,0.f,.45f})
            {
                // The first cell deliberately tapers to a subpixel spring
                // at overview scale; its antialiased pixels mix with land.
                // Native geometry tests check continuity through that tip.
                // Keep this opaque-pixel check on the full-width ribbon.
                const MapFeaturePoint centre{a.x+dx*t,a.y+dy*t};
                if(std::hypot(centre.x-points.front().x,centre.y-points.front().y)<1.f)continue;
                const bool endReach=
                    std::hypot(centre.x-points.back().x,centre.y-points.back().y)<1.f;
                if(endReach && side!=0.f)continue;
                const float x=a.x+dx*t-dy/len*radius*side,y=a.y+dy*t+dx/len*radius*side;
                const auto c=pixel(image,static_cast<int>(x*ppc),static_cast<int>(y*ppc));
                require(static_cast<int>(c[2])-c[0]>35,"Continuous river mesh contains a pale transverse seam at "+
                    std::to_string(x)+","+std::to_string(y)+" (side "+std::to_string(side)+", scale "+std::to_string(ppc)+")");
            }
        }
        const MapPngExportRect crop{8,3,10,10};
        const auto cropped=root/("river-mesh-crop-"+std::to_string(ppc)+".png");
        std::string error;
        require(exportMapPngRegion(map,cropped,plan,crop,ppc,&error),error);
        requireSubimage(image,read(cropped),crop,ppc,"Continuous gradient river crop");
    }
}
void testClassicCellIllustration(const std::filesystem::path& root)
{
    MapModel map;map.metadata.id="classic_illustration_fixture";
    map.width=14;map.height=10;map.atlasLevel=MapLevel::Site;
    map.generation.continuousRendering=false;resizeMapCells(map);
    for(auto& cell:map.cells){applyMapCellTerrainDefaults(cell,MapTileKind::Empty);cell.fogged=false;}
    for(int y=1;y<4;++y)for(int x=1;x<5;++x)applyMapCellTerrainDefaults(*mapCellAt(map,x,y),MapTileKind::Forest);
    for(int x=7;x<12;++x)applyMapCellTerrainDefaults(*mapCellAt(map,x,2),MapTileKind::Mountain);
    for(int x=0;x<map.width;++x)mapCellAt(map,x,5)->wayKind=MapWayKind::DirtRoad;
    mapCellAt(map,7,7)->kind=MapTileKind::Building;
    mapCellAt(map,9,7)->kind=MapTileKind::Building;
    const auto before=map.cells;
    MapRenderPlanRequest request;request.applyFog=false;request.showGrid=false;
    request.layers=MapRenderPlanTerrain|MapRenderPlanWater|MapRenderPlanWays|MapRenderPlanSymbols;
    const auto image=render(map,request,root/"classic-modern.png",32);
    const auto paper=pixel(image,16,16),road=pixel(image,16,5*32+16);
    require(road!=paper,"Classic export omitted the painted road");
    for(int y=5*32+11;y<=5*32+21;++y)
        for(int x=64;x<image.width-64;++x)
            require(pixel(image,x,y)==pixel(image,70,y),"Connected classic road has repeated cell-cap seams");
    require(pixel(image,32+16,32+16)!=paper,"Classic export omitted illustrated woodland");
    const MapPngExportRect crop{2,1,10,8};std::string error;
    require(exportMapPngRegion(map,root/"classic-modern-crop.png",compileMapRenderPlan(map,request),crop,32,&error),error);
    requireSubimage(image,read(root/"classic-modern-crop.png"),crop,32,"Classic shared illustration crop");
    for(std::size_t i=0;i<before.size();++i)
        require(map.cells[i].kind==before[i].kind&&map.cells[i].wayKind==before[i].wayKind,"Classic export modified editable cells");
    auto hidden=request;hidden.layers=MapRenderPlanTerrain|MapRenderPlanWater;
    const auto withoutWays=render(map,hidden,root/"classic-no-ways.png",32);
    require(pixel(withoutWays,16,5*32+16)==paper,"Hidden classic Ways layer still draws the road");
}
} // namespace

int main()
{
    try
    {
        Fixture fixture;
        testTraceFidelityAndPolicy(fixture.root);
        testTracesAcrossRealTileBoundary(fixture.root);
        testIllustratedPlanGeometryAcrossCropAndTiles(fixture.root / "atlas-v1", 1);
        testIllustratedPlanGeometryAcrossCropAndTiles(fixture.root / "atlas-v2", 2);
        testIllustratedPlanGeometryAcrossCropAndTiles(fixture.root / "atlas-v3", 3);
        testRoadShoulderEnteringCropWithoutControls(fixture.root);
        testV2AtlasWaterAndTerrainOffsets(fixture.root / "atlas-water");
        testDirectArtworkHelperUsesBentTravelRoute();
        testContinuousRiverMesh(fixture.root);
        testClassicCellIllustration(fixture.root);
        std::cout << "Map trace export tests passed.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
