#include "Models/Map/PngExport/PngRenderSession.hpp"
#include "Models/Map/PngExport/PngAtlasInk.hpp"
#include "Models/Map/MapAtlasWaterInk.hpp"
#include "Models/Map/MapIllustratedAtlas.hpp"
#include "Models/Map/MapLocalIllustration.hpp"
#include "Models/Map/MapGouacheAtlasInk.hpp"
#include "Models/Map/MapOverviewRenderPolicy.hpp"
#include "Models/Map/MapRoutePresentation.hpp"
#include "Models/Map/MapStyleRegistry.hpp"
#include "Models/Map/PngExport/PngRasterAssets.hpp"
#include "Models/Map/PngExport/PngRasterCanvas.hpp"
#include "Models/Map/PngExport/PngSemanticOverlays.hpp"
#include "Models/Map/PngExport/PngVisualPolicy.hpp"

#include <algorithm>
#include <cmath>

namespace MapPngExportRaster
{
void PngRenderSession::drawFeatureLayers()
{
        // Traces are semantic content over either imported images or generated
        // terrain. Keep their colors, smoothing and widths aligned with the
        // interactive trace pass, below water/ways and the final fog mask.
        for (const MapGeneratedFeature &feature : map.generation.graph.features)
        {
            const bool coastline = feature.kind == MapGeneratedFeatureKind::Coastline &&
                                   feature.materialId == "trace.coastline" && feature.points.size() >= 2;
            const bool terrainArea = feature.kind == MapGeneratedFeatureKind::TerrainRegion &&
                                     feature.materialId == "trace.terrain_area" && feature.closed && feature.points.size() >= 3;
            if ((!coastline && !terrainArea) ||
                (coastline && !showWater) || (terrainArea && !showTerrain) ||
                !mapRenderPlanContainsGeneratedGeometry(plan, feature.id) ||
                (playerSafe && (feature.gmOnly || !feature.discovered)))
                continue;

            const std::vector<MapFeaturePoint> points = coastline
                ? smoothFeaturePoints(feature.points) : feature.points;
            if (std::any_of(points.begin(), points.end(), [](const MapFeaturePoint &point) {
                    return !std::isfinite(point.x) || !std::isfinite(point.y);
                }))
                continue;

            if (terrainArea)
            {
                // Even-odd scanlines preserve concave bays/notches and blend
                // once per pixel. A triangle fan would fill outside the trace.
                std::vector<float> crossings;
                crossings.reserve(points.size());
                for (int y = 0; y < height; ++y)
                {
                    crossings.clear();
                    const float sampleY = (static_cast<float>(y) + 0.5f) / pixelsPerCell;
                    for (std::size_t current = 0, previous = points.size() - 1;
                         current < points.size(); previous = current++)
                    {
                        const auto &a = points[previous];
                        const auto &b = points[current];
                        if ((a.y > sampleY) != (b.y > sampleY))
                            crossings.push_back(a.x + (sampleY - a.y) * (b.x - a.x) / (b.y - a.y));
                    }
                    std::sort(crossings.begin(), crossings.end());
                    for (std::size_t index = 1; index < crossings.size(); index += 2)
                    {
                        const int firstX = static_cast<int>(std::clamp(std::ceil(crossings[index - 1] * pixelsPerCell - 0.5f), 0.0f, static_cast<float>(width)));
                        const int lastX = static_cast<int>(std::clamp(std::ceil(crossings[index] * pixelsPerCell - 0.5f), 0.0f, static_cast<float>(width)));
                        for (int x = firstX; x < lastX; ++x)
                        {
                            Pixel &destination = pixels[static_cast<std::size_t>(y * width + x)];
                            destination = alphaBlend(destination, {222, 174, 76, 28}, 255);
                        }
                    }
                }
            }
            const Pixel ink = coastline ? Pixel{51, 171, 192, 230} : Pixel{235, 190, 88, 210};
            const int thickness = static_cast<int>(std::lround(coastline
                ? std::max(2.0f, pixelsPerCell * 0.10f)
                : std::max(1.5f, pixelsPerCell * 0.075f)));
            const std::size_t segmentCount = points.size() - (feature.closed ? 0 : 1);
            for (std::size_t index = 0; index < segmentCount; ++index)
            {
                const auto &a = points[index];
                const auto &b = points[(index + 1) % points.size()];
                // floor(x + .5) keeps rounding invariant under the integer
                // map-space offsets used by cropped/tiled output, including
                // trace vertices just outside the left/top crop boundary.
                drawCartographicLine(pixels, width, height,
                                     static_cast<int>(std::floor(a.x * pixelsPerCell + 0.5f)),
                                     static_cast<int>(std::floor(a.y * pixelsPerCell + 0.5f)),
                                     static_cast<int>(std::floor(b.x * pixelsPerCell + 0.5f)),
                                     static_cast<int>(std::floor(b.y * pixelsPerCell + 0.5f)), thickness, ink);
            }
        }

        for (const MapGeneratedFeature &feature : map.generation.graph.features)
        {
            if (playerSafe && (feature.gmOnly || !feature.discovered))
                continue;
            if (feature.points.size() < 2 || (feature.kind != MapGeneratedFeatureKind::River && feature.kind != MapGeneratedFeatureKind::Road && feature.kind != MapGeneratedFeatureKind::Trail))
                continue;
            if ((feature.kind == MapGeneratedFeatureKind::River && !showWater) ||
                (feature.kind != MapGeneratedFeatureKind::River && !showWays))
                continue;
            if(feature.kind!=MapGeneratedFeatureKind::River &&
               (gm::map_render::usesNaturalLocalRoadSurface(map) || gm::map_render::usesGouacheAtlasArtwork(map)))
            {
                for(const auto &primitive:plan.primitives)
                    if(primitive.sourceId==feature.id && primitive.kind==MapRenderPrimitiveKind::GeneratedGeometry)
                        drawAtlasInk(pixels,width,height,primitive.detailInk,pixelsPerCell,mapOffsetX,mapOffsetY);
                continue;
            }
            if (feature.kind==MapGeneratedFeatureKind::River && gm::map_render::usesIllustratedAtlasRiverMesh(map))
            {
                for (const auto &primitive:plan.primitives)
                    if (primitive.sourceId==feature.id && primitive.kind==MapRenderPrimitiveKind::GeneratedGeometry)
                        drawAtlasInk(pixels,width,height,primitive.detailInk,pixelsPerCell,mapOffsetX,mapOffsetY);
                continue;
            }
            const bool majorRoad = feature.kind == MapGeneratedFeatureKind::Road && feature.width >= 0.88f;
            const bool worldRoad = feature.kind != MapGeneratedFeatureKind::River &&
                                   (map.atlasLevel == MapLevel::World || gm::map_render::usesIllustratedAtlas(map));
            Pixel outer{static_cast<std::uint8_t>(majorRoad ? 127 : 116),
                        static_cast<std::uint8_t>(majorRoad ? 104 : 94),
                        static_cast<std::uint8_t>(majorRoad ? 68 : 62), 255};
            Pixel inner{static_cast<std::uint8_t>(majorRoad ? 174 : 153),
                        static_cast<std::uint8_t>(majorRoad ? 137 : 116),
                        static_cast<std::uint8_t>(majorRoad ? 84 : 70), 255};
            if (worldRoad)
            {
                outer = {gm::map_render::kOverviewRoadShoulder.r,
                         gm::map_render::kOverviewRoadShoulder.g,
                         gm::map_render::kOverviewRoadShoulder.b,
                         gm::map_render::kOverviewRoadShoulder.a};
                inner = {gm::map_render::kOverviewRoadSurface.r,
                         gm::map_render::kOverviewRoadSurface.g,
                         gm::map_render::kOverviewRoadSurface.b,
                         gm::map_render::kOverviewRoadSurface.a};
            }
            else if (feature.kind != MapGeneratedFeatureKind::River)
            {
                outer = {gm::map_render::kLocalRoadShoulder.r,
                         gm::map_render::kLocalRoadShoulder.g,
                         gm::map_render::kLocalRoadShoulder.b,
                         gm::map_render::kLocalRoadShoulder.a};
                const gm::map_render::OverviewRenderColor localSurface =
                    feature.kind == MapGeneratedFeatureKind::Trail
                        ? gm::map_render::kLocalTrailSurface
                        : gm::map_render::kLocalRoadSurface;
                inner = {localSurface.r, localSurface.g, localSurface.b,
                         localSurface.a};
            }
            const MapRenderScaleProfile &scaleProfile = gm::map_render::atlasPresentationProfile(map);
            const bool integratedRoad = feature.kind != MapGeneratedFeatureKind::River &&
                                        gm::map_render::usesIntegratedTacticalRoadInk(map);
            if (integratedRoad)
            {
                // The semantic terrain already paints the travelled ground.
                // Let its texture show through instead of covering it with an
                // opaque capsule and two heavy cartographic border strokes.
                outer.a = map.generation.recipe.generatorVersion >= 8 ? 12 : 30;
                inner.a = map.generation.recipe.generatorVersion >= 8 ? 18 : feature.kind == MapGeneratedFeatureKind::Trail ? 50 : 60;
            }
            const bool localRoad = feature.kind == MapGeneratedFeatureKind::Road &&
                                   (map.atlasLevel == MapLevel::Site || map.atlasLevel == MapLevel::Tactical);
            const float localWidthBoost = localRoad ? 1.14f : 1.0f;
            int thickness = std::max(2, static_cast<int>(std::lround(
                feature.width * pixelsPerCell * scaleProfile.roadWidthCells * localWidthBoost *
                (worldRoad ? gm::map_render::kOverviewRoadWidthScale : 1.0f))));
            if (worldRoad && gm::map_render::usesIllustratedAtlasDetailed(map))
                thickness = std::max(1,static_cast<int>(std::lround(gm::map_render::illustratedAtlasV2RoadWidth(feature,static_cast<float>(pixelsPerCell)))));
            if (feature.kind == MapGeneratedFeatureKind::River)
            {
                outer = {gm::map_render::kLocalRiverBank.r,
                         gm::map_render::kLocalRiverBank.g,
                         gm::map_render::kLocalRiverBank.b,
                         gm::map_render::kLocalRiverBank.a};
                inner = {gm::map_render::kLocalRiverWater.r,
                         gm::map_render::kLocalRiverWater.g,
                         gm::map_render::kLocalRiverWater.b,
                         gm::map_render::kLocalRiverWater.a};
                thickness = std::max(static_cast<int>(pixelsPerCell * scaleProfile.riverWidthCells),
                                     static_cast<int>(feature.width * pixelsPerCell * scaleProfile.riverWidthCells));
            }
            const std::vector<MapFeaturePoint> cartographicControls =
                gm::map_render::overviewCartographicCurvePoints(map, feature);
            const bool resolvedRoad = feature.kind != MapGeneratedFeatureKind::River &&
                    (gm::map_render::usesIllustratedAtlas(map) || gm::map_render::usesIllustratedLocalV6(map) ||
                     ((map.atlasLevel == MapLevel::World || map.atlasLevel == MapLevel::Region) && map.generation.recipe.generatorVersion >= 6));
            const bool resolvedRiver = feature.kind == MapGeneratedFeatureKind::River && (gm::map_render::usesIllustratedAtlasDetailed(map) || gm::map_render::usesTacticalRiverDetail(map));
            const bool resolvedCurve = resolvedRoad || resolvedRiver;
            std::vector<MapFeaturePoint> renderedPoints = smoothFeaturePoints(cartographicControls);
            if (resolvedCurve)
            {
                if (const auto *geometry = mapRenderPlanGeneratedGeometry(plan, feature.id))
                {
                    renderedPoints = *geometry;
                    for (auto &point : renderedPoints) { point.x -= mapOffsetX; point.y -= mapOffsetY; }
                }
                else renderedPoints = resolvedRiver ? gm::map_render::illustratedAtlasFinalRiverPoints(map, feature)
                                                     : gm::map_render::overviewFinalRoadCurvePoints(map, feature);
            }
            const bool worldRiver = feature.kind == MapGeneratedFeatureKind::River &&
                                    (map.atlasLevel == MapLevel::World || gm::map_render::usesIllustratedAtlas(map));
            auto drawFeaturePass = [&](int passThickness, Pixel color) {
                if (integratedRoad)
                {
                    std::vector<std::pair<int, int>> pixelPoints;
                    pixelPoints.reserve(renderedPoints.size());
                    for (const auto &point : renderedPoints)
                        pixelPoints.emplace_back(
                            static_cast<int>(std::floor((point.x + mapOffsetX) * pixelsPerCell)) - static_cast<int>(std::lround(mapOffsetX * pixelsPerCell)),
                            static_cast<int>(std::floor((point.y + mapOffsetY) * pixelsPerCell)) - static_cast<int>(std::lround(mapOffsetY * pixelsPerCell)));
                    drawCartographicPolylineOnce(pixels, width, height, pixelPoints, passThickness, color);
                    return;
                }
                for (std::size_t index = 1; index < renderedPoints.size(); ++index)
                {
                    const MapFeaturePoint &a = renderedPoints[index - 1];
                    const MapFeaturePoint &b = renderedPoints[index];
                    const int renderedThickness = worldRiver
                        ? std::max(1, static_cast<int>(std::lround(
                              passThickness * gm::map_render::overviewRiverWidthFactor(
                                                  index, renderedPoints.size()))))
                        : passThickness;
                    const auto px = [&](float x) { return resolvedCurve
                        ? static_cast<int>(std::floor((x+mapOffsetX)*pixelsPerCell))-static_cast<int>(std::lround(mapOffsetX*pixelsPerCell))
                        : static_cast<int>(x*pixelsPerCell); };
                    const auto py = [&](float y) { return resolvedCurve
                        ? static_cast<int>(std::floor((y+mapOffsetY)*pixelsPerCell))-static_cast<int>(std::lround(mapOffsetY*pixelsPerCell))
                        : static_cast<int>(y*pixelsPerCell); };
                    drawCartographicLine(pixels, width, height, px(a.x), py(a.y), px(b.x), py(b.y), renderedThickness, color);
                }
            };
            const int shoulder = std::max(worldRiver || worldRoad ? 1 : 2,
                static_cast<int>(std::lround(pixelsPerCell *
                    (worldRiver ? 0.070f : (worldRoad ? 0.055f : (majorRoad ? 0.20f : 0.16f))))));
            if (worldRiver)
                outer = {gm::map_render::kOverviewRiverBank.r,
                         gm::map_render::kOverviewRiverBank.g,
                         gm::map_render::kOverviewRiverBank.b,
                         gm::map_render::kOverviewRiverBank.a};
            if (gm::map_render::usesIllustratedLocalV6(map) && feature.kind != MapGeneratedFeatureKind::River) {
                outer={149,128,87,255};inner={196,174,125,255};
                // The integer rasterizer halves stroke diameters. A +1
                // diameter can round to the same radius and erase the bank.
                drawFeaturePass(thickness+2*std::max(1,pixelsPerCell/32),outer);
            } else drawFeaturePass(thickness + shoulder, outer);
            if (worldRiver)
                inner = {gm::map_render::kOverviewRiverWater.r,
                         gm::map_render::kOverviewRiverWater.g,
                         gm::map_render::kOverviewRiverWater.b,
                         gm::map_render::kOverviewRiverWater.a};
            drawFeaturePass(thickness, inner);
            if (worldRiver)
                drawFeaturePass(std::max(1, thickness / 6),
                    {gm::map_render::kOverviewRiverGlint.r,
                     gm::map_render::kOverviewRiverGlint.g,
                     gm::map_render::kOverviewRiverGlint.b,
                     gm::map_render::kOverviewRiverGlint.a});
            if (feature.kind == MapGeneratedFeatureKind::River ||
                (pixelsPerCell >= 8 && map.generation.recipe.recipeVersion >= 5 && gm::map_render::usesIllustratedLocalV6(map)))
                for (const auto &primitive:plan.primitives)
                    if (primitive.sourceId==feature.id && primitive.kind==MapRenderPrimitiveKind::GeneratedGeometry)
                        drawAtlasInk(pixels,width,height,primitive.detailInk,pixelsPerCell,mapOffsetX,mapOffsetY);
            if (feature.kind == MapGeneratedFeatureKind::Road)
            {
                const bool overview = map.atlasLevel == MapLevel::World || map.atlasLevel == MapLevel::Region;
                if (!overview && thickness >= 4 && !gm::map_render::usesIllustratedLocalV6(map))
                {
                    const float offsetCells = static_cast<float>(thickness) / static_cast<float>(pixelsPerCell) * 0.22f;
                    const std::vector<MapFeaturePoint> leftTrack = offsetFeaturePoints(renderedPoints, offsetCells);
                    const std::vector<MapFeaturePoint> rightTrack = offsetFeaturePoints(renderedPoints, -offsetCells);
                    const int trackThickness = std::clamp(thickness / 11, 1, 2);
                    auto drawTrack = [&](const std::vector<MapFeaturePoint> &track) {
                        if (integratedRoad)
                        {
                            std::vector<std::pair<int, int>> pixelPoints;
                            pixelPoints.reserve(track.size());
                            for (const auto &point : track)
                                pixelPoints.emplace_back(
                                    static_cast<int>(std::floor((point.x + mapOffsetX) * pixelsPerCell)) - static_cast<int>(std::lround(mapOffsetX * pixelsPerCell)),
                                    static_cast<int>(std::floor((point.y + mapOffsetY) * pixelsPerCell)) - static_cast<int>(std::lround(mapOffsetY * pixelsPerCell)));
                            drawCartographicPolylineOnce(pixels, width, height, pixelPoints, trackThickness,
                                {gm::map_render::kLocalRoadTrack.r, gm::map_render::kLocalRoadTrack.g,
                                 gm::map_render::kLocalRoadTrack.b, 80});
                            return;
                        }
                        for (std::size_t index = 1; index < track.size(); ++index)
                            drawCartographicLine(pixels, width, height,
                                     static_cast<int>(track[index - 1].x * pixelsPerCell),
                                     static_cast<int>(track[index - 1].y * pixelsPerCell),
                                     static_cast<int>(track[index].x * pixelsPerCell),
                                     static_cast<int>(track[index].y * pixelsPerCell),
                                     trackThickness,
                                     {gm::map_render::kLocalRoadTrack.r,
                                      gm::map_render::kLocalRoadTrack.g,
                                      gm::map_render::kLocalRoadTrack.b,
                                      static_cast<std::uint8_t>(integratedRoad ? 80 : 150)});
                    };
                    drawTrack(leftTrack);
                    drawTrack(rightTrack);
                }
            }

            // Production curve assets add painterly banks, shoulders and
            // surface detail while the continuous vector stroke remains the
            // authoritative, gap-free geometry.  Sampling by travelled
            // distance avoids a stamp at every Chaikin subdivision.
            if (map.generation.recipe.styleVersion >= 3 &&
                gm::map_render::overviewUsesCurveProfileAccents(map) &&
                mapStyleProductionSnapshotAvailable(map.generation.recipe.styleSnapshotId))
            {
                const std::string semantic = feature.kind == MapGeneratedFeatureKind::River
                    ? (feature.width >= 1.35f ? "river_deep" : "river")
                    : (feature.kind == MapGeneratedFeatureKind::Trail
                           ? "path"
                           : (feature.materialId.find("stone") != std::string::npos ? "road_stone" : "road_dirt"));
                const float stampSpacingCells = std::clamp(scaleProfile.materialCellsPerRepeat * 1.15f, 6.0f, 12.0f);
                const float stampLengthCells = std::clamp(stampSpacingCells * 0.42f, 2.5f, 4.5f);
                MapFeaturePoint stampStart = renderedPoints.front();
                float travelled = 0.0f;
                int stampIndex = 0;
                for (std::size_t index = 1; index < renderedPoints.size(); ++index)
                {
                    const float segmentX = renderedPoints[index].x - renderedPoints[index - 1].x;
                    const float segmentY = renderedPoints[index].y - renderedPoints[index - 1].y;
                    travelled += std::sqrt(segmentX * segmentX + segmentY * segmentY);
                    const bool atEnd = index + 1 == renderedPoints.size();
                    if (travelled < stampSpacingCells && !atEnd)
                        continue;
                    const MapFeaturePoint &stampEnd = renderedPoints[index];
                    const float dx = (stampEnd.x - stampStart.x) * pixelsPerCell;
                    const float dy = (stampEnd.y - stampStart.y) * pixelsPerCell;
                    const float length = std::sqrt(dx * dx + dy * dy);
                    if (length > 0.5f)
                    {
                        const float centerX = (stampStart.x + stampEnd.x) * pixelsPerCell * 0.5f;
                        const float centerY = (stampStart.y + stampEnd.y) * pixelsPerCell * 0.5f;
                        const MapStylePackedRuntimeAsset *asset = resolveMapStyleProductionAssetForSnapshot(
                            map.generation.recipe.styleSnapshotId, map.atlasLevel, "curve_profile", semantic,
                            static_cast<int>(std::floor(centerX / pixelsPerCell)),
                            static_cast<int>(std::floor(centerY / pixelsPerCell)), 1200 + stampIndex++);
                        drawRotatedRasterAsset(pixels, width, height, centerX, centerY,
                                               std::min(length + pixelsPerCell * 0.2f, stampLengthCells * pixelsPerCell),
                                               std::max(pixelsPerCell * 1.05f, thickness * 2.15f),
                                               std::atan2(dy, dx), asset,
                                               feature.kind == MapGeneratedFeatureKind::River ? 48 : 38);
                    }
                    stampStart = stampEnd;
                    travelled = 0.0f;
                }
            }
        }

}
}
