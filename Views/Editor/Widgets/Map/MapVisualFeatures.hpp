#pragma once

#include "Views/Editor/Widgets/Map/MapVisualBridges.hpp"
#include "Views/Editor/Widgets/Map/MapTypographyFont.hpp"
#include "Models/Map/MapGouacheAtlasInk.hpp"

inline void drawMapContinuousFeatureGraph(ImDrawList *drawList, ImVec2 origin, float cellSize, const MapModel &map,
                                          bool playerSafe, const MapGeneratedFeatureVisibilityFilter &featureFilter,
                                          bool showPoliticalOverlay = false, bool showLabels = true,
                                          std::string_view locale = {},
                                          const MapGeneratedFeatureVisibilityFilter &geometryFilter = {},
                                          const MapRenderPlan *resolvedPlan = nullptr, std::vector<ImVec4> *sharedLabelBounds = nullptr)
{
    if (!map.generation.continuousRendering && map.generation.graph.features.empty())
        return;

    std::vector<ImVec4> localLabelBounds;
    auto &occupiedLabels = sharedLabelBounds ? *sharedLabelBounds : localLabelBounds;
    const MapVisibleCellBounds visible = mapVisibleCellBounds(drawList, origin, cellSize, map, 3);
    const auto featureIsVisible = [&](const MapGeneratedFeature &feature) {
        if (!mapGeneratedFeatureVisibleForRender(feature, playerSafe, featureFilter))
            return false;
        if (feature.points.empty())
            return false;
        float minimumX = feature.points.front().x;
        float maximumX = minimumX;
        float minimumY = feature.points.front().y;
        float maximumY = minimumY;
        for (const MapFeaturePoint &point : feature.points)
        {
            minimumX = std::min(minimumX, point.x);
            maximumX = std::max(maximumX, point.x);
            minimumY = std::min(minimumY, point.y);
            maximumY = std::max(maximumY, point.y);
        }
        return maximumX >= visible.minX && minimumX <= visible.maxX + 1 &&
               maximumY >= visible.minY && minimumY <= visible.maxY + 1;
    };

    auto mapPointsToScreen = [&](const std::vector<MapFeaturePoint> &source) {
        std::vector<ImVec2> points;
        points.reserve(source.size());
        for (const MapFeaturePoint &point : source)
            points.emplace_back(origin.x + point.x * cellSize, origin.y + point.y * cellSize);
        return points;
    };
    auto screenPoints = [&](const MapGeneratedFeature &feature) {
        return mapPointsToScreen(feature.points);
    };
    const auto geometryIsVisible = [&](const MapGeneratedFeature &feature) {
        return featureIsVisible(feature) && (!geometryFilter || geometryFilter(feature));
    };

    // Semantics traced over a managed map image use the same feature graph as
    // generated maps. Keep their restrained overlays in the shared renderer so
    // Editor, Player Display and PNG never invent separate trace geometry.
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if (!geometryIsVisible(feature))
            continue;
        if (feature.kind == MapGeneratedFeatureKind::Coastline && feature.points.size() >= 2 &&
            feature.materialId == "trace.coastline")
        {
            const std::vector<ImVec2> points = mapSmoothFeaturePoints(screenPoints(feature));
            const ImDrawFlags flags = feature.closed ? ImDrawFlags_Closed : ImDrawFlags_None;
            drawList->AddPolyline(points.data(), static_cast<int>(points.size()),
                                  IM_COL32(51, 171, 192, 230), flags,
                                  std::max(2.0f, cellSize * 0.10f));
        }
        else if (feature.kind == MapGeneratedFeatureKind::TerrainRegion && feature.closed &&
                 feature.points.size() >= 3 && feature.materialId == "trace.terrain_area")
        {
            std::vector<ImVec2> points = screenPoints(feature);
            // Users can trace concave regions in either direction. Normalize
            // winding for ImGui's anti-alias fringe and triangulate the actual
            // polygon, rather than filling a fan across bays and notches.
            const auto samePoint = [](const ImVec2 &a, const ImVec2 &b) {
                return a.x == b.x && a.y == b.y;
            };
            points.erase(std::unique(points.begin(), points.end(), samePoint), points.end());
            if (points.size() > 1 && samePoint(points.front(), points.back()))
                points.pop_back();
            if (points.size() < 3)
                continue;
            float signedArea = 0.0f;
            for (std::size_t index = 0; index < points.size(); ++index)
            {
                const ImVec2 &a = points[index];
                const ImVec2 &b = points[(index + 1) % points.size()];
                signedArea += a.x * b.y - b.x * a.y;
            }
            if (signedArea < 0.0f)
                std::reverse(points.begin(), points.end());
            if (std::abs(signedArea) > 0.001f)
                drawList->AddConcavePolyFilled(points.data(), static_cast<int>(points.size()),
                                              IM_COL32(222, 174, 76, 28));
            drawList->AddPolyline(points.data(), static_cast<int>(points.size()),
                                  IM_COL32(235, 190, 88, 210), ImDrawFlags_Closed,
                                  std::max(1.5f, cellSize * 0.075f));
        }
    }

    // Build political borders once from changes in territory ownership. This
    // excludes coastline by construction and prevents adjacent polygons from
    // drawing the same division twice with conflicting geometry.
    if (showPoliticalOverlay)
        drawMapSharedPoliticalBorders(drawList, origin, cellSize, map);
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if (feature.kind != MapGeneratedFeatureKind::TerrainRegion || !feature.closed || feature.points.size() < 3 ||
            feature.materialId.find("territory.") == std::string::npos || !featureIsVisible(feature))
            continue;
        if (showLabels && !feature.name.empty())
        {
            const MapFeaturePoint anchor = mapTerritoryLandLabelPoint(map, feature);
            const ImVec2 center(origin.x + anchor.x * cellSize, origin.y + anchor.y * cellSize);
            std::string localizedFeatureName = localizedGeneratedMapName(feature.name, locale);
            if (map.atlasLevel == MapLevel::World)
                std::transform(localizedFeatureName.begin(), localizedFeatureName.end(),
                               localizedFeatureName.begin(), [](unsigned char character) {
                                   return static_cast<char>(std::toupper(character));
                               });
            float fontSize = std::clamp(std::max(
                                                  ImGui::GetFontSize() *
                                                      (map.atlasLevel == MapLevel::World ? 1.42f : 0.94f),
                                                  cellSize * (map.atlasLevel == MapLevel::World ? 1.30f : 0.68f)),
                                              map.atlasLevel == MapLevel::World ? 17.0f : 12.0f,
                                              map.atlasLevel == MapLevel::World ? 30.0f : 20.0f);
            ImFont *labelFont=mapLabelFont(map);
            ImVec2 textSize = labelFont->CalcTextSizeA(
                fontSize, 10000.0f, 0.0f, localizedFeatureName.c_str());
            fontSize=gm::map_render::overviewTerritoryLabelFontSize(map,cellSize,fontSize,textSize.x);
            if(fontSize<=0.f)continue;
            textSize=labelFont->CalcTextSizeA(fontSize,10000.f,0.f,localizedFeatureName.c_str());
            ImVec2 textPosition(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f);
            if(map.generation.recipe.generatorVersion>=11)
            {
                if(textSize.x+8.f>map.width*cellSize || textSize.y+6.f>map.height*cellSize)continue;
                textPosition.x=std::clamp(textPosition.x,origin.x+4.f,origin.x+map.width*cellSize-textSize.x-4.f);
                textPosition.y=std::clamp(textPosition.y,origin.y+3.f,origin.y+map.height*cellSize-textSize.y-3.f);
            }
            if (playerSafe &&
                !mapPlayerSafeCellAreaIsRevealed(
                    map,
                    (textPosition.x - origin.x) / cellSize,
                    (textPosition.y - origin.y) / cellSize,
                    (textPosition.x + textSize.x - origin.x) / cellSize,
                    (textPosition.y + textSize.y - origin.y) / cellSize))
                continue;
            const ImVec4 labelBounds(textPosition.x-4, textPosition.y-3,
                                     textPosition.x+textSize.x+4, textPosition.y+textSize.y+3);
            if(!gm::map_render::overviewLabelBoxFitsMap(map,
                (labelBounds.x-origin.x)/cellSize,(labelBounds.y-origin.y)/cellSize,
                (labelBounds.z-origin.x)/cellSize,(labelBounds.w-origin.y)/cellSize))continue;
            if (std::any_of(occupiedLabels.begin(), occupiedLabels.end(), [&](const auto &other) {
                return labelBounds.x < other.z && labelBounds.z > other.x && labelBounds.y < other.w && labelBounds.w > other.y;
            })) continue;
            occupiedLabels.push_back(labelBounds);
            drawMapTextWithHalo(drawList, labelFont, fontSize, textPosition,
                                IM_COL32(51, 71, 61, 240), IM_COL32(224, 221, 178, 190),
                                std::clamp(fontSize * 0.09f, 1.1f, 1.7f),
                                localizedFeatureName.c_str());
        }
    }

    // District geometry is an editing aid, not a second road grid.  The old
    // bright rectangular outlines looked like tactical deployment boxes and
    // obscured the town composition.  Keep only a quiet tint and, at useful
    // site zooms, a named centre label.
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if (feature.kind != MapGeneratedFeatureKind::District || !feature.closed ||
            feature.points.size() < 3 || !featureIsVisible(feature))
            continue;
        const std::vector<ImVec2> points = screenPoints(feature);
        if (geometryIsVisible(feature))
            drawList->AddConvexPolyFilled(points.data(), static_cast<int>(points.size()), IM_COL32(186, 151, 94, 10));
        if (!showLabels || map.atlasLevel != MapLevel::Site || cellSize < 13.0f || feature.name.empty())
            continue;
        ImVec2 centre{};
        for (const ImVec2 &point : points)
        {
            centre.x += point.x;
            centre.y += point.y;
        }
        centre.x /= static_cast<float>(points.size());
        centre.y /= static_cast<float>(points.size());
        const std::string label = localizedGeneratedMapName(feature.name, locale);
        const float fontSize = std::clamp(ImGui::GetFontSize() * 0.70f, 10.0f, 13.0f);
        const ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(fontSize, 10000.0f, 0.0f, label.c_str());
        const ImVec2 textPosition(centre.x - textSize.x * 0.5f, centre.y - textSize.y * 0.5f);
        if (playerSafe &&
            !mapPlayerSafeCellAreaIsRevealed(
                map,
                (textPosition.x - origin.x) / cellSize,
                (textPosition.y - origin.y) / cellSize,
                (textPosition.x + textSize.x - origin.x) / cellSize,
                (textPosition.y + textSize.y - origin.y) / cellSize))
            continue;
        drawList->AddRectFilled(ImVec2(textPosition.x - 4.0f, textPosition.y - 2.0f),
                                ImVec2(textPosition.x + textSize.x + 4.0f, textPosition.y + textSize.y + 2.0f),
                                IM_COL32(35, 31, 25, 150), 3.0f);
        drawList->AddText(ImGui::GetFont(), fontSize, textPosition, IM_COL32(225, 213, 187, 205), label.c_str());
    }

    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if (feature.kind != MapGeneratedFeatureKind::TerrainRegion || !feature.closed || feature.points.size() < 3 || feature.materialId.find("water") == std::string::npos || !geometryIsVisible(feature))
            continue;
        const std::vector<ImVec2> points = screenPoints(feature);
        drawList->AddPolyline(points.data(), static_cast<int>(points.size()), IM_COL32(112, 91, 56, 185), ImDrawFlags_Closed, cellSize * 0.34f);
        drawList->AddConvexPolyFilled(points.data(), static_cast<int>(points.size()), IM_COL32(41, 126, 151, 255));
        drawList->AddPolyline(points.data(), static_cast<int>(points.size()), IM_COL32(111, 194, 207, 145), ImDrawFlags_Closed, std::max(1.2f, cellSize * 0.08f));
    }

    // Water sits below roads so bridge approaches remain legible. Layered
    // strokes provide soft banks and depth without exposing the cell lattice.
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if (feature.kind != MapGeneratedFeatureKind::River || feature.points.size() < 2 || !geometryIsVisible(feature))
            continue;
        if (gm::map_render::usesIllustratedAtlasRiverMesh(map))
        {
            if (resolvedPlan) {
                for (const auto &primitive:resolvedPlan->primitives)
                    if (primitive.sourceId==feature.id && primitive.kind==MapRenderPrimitiveKind::GeneratedGeometry)
                        drawMapIllustratedAtlasInk(drawList,origin,cellSize,primitive.detailInk);
            } else drawMapIllustratedAtlasInk(drawList,origin,cellSize,gm::map_render::buildIllustratedAtlasRiverInk(map,feature));
            continue;
        }
        const std::vector<MapFeaturePoint> cartographicControls =
            gm::map_render::overviewCartographicCurvePoints(map, feature);
        const auto *plannedRiver = resolvedPlan ? mapRenderPlanGeneratedGeometry(*resolvedPlan, feature.id) : nullptr;
        const std::vector<ImVec2> points = (gm::map_render::usesIllustratedAtlasDetailed(map) || gm::map_render::usesTacticalRiverDetail(map))
            ? mapPointsToScreen(plannedRiver ? *plannedRiver : gm::map_render::usesTacticalRiverDetail(map)
                ? gm::map_render::tacticalRiverDetailPoints(map,feature) : gm::map_render::illustratedAtlasFinalRiverPoints(map, feature))
            : mapSmoothFeaturePoints(mapPointsToScreen(cartographicControls));
        const MapRenderScaleProfile &profile = gm::map_render::atlasPresentationProfile(map);
        const float width = std::max(cellSize * profile.riverWidthCells, feature.width * cellSize * profile.riverWidthCells);
        const bool worldRiver = map.atlasLevel == MapLevel::World || gm::map_render::usesIllustratedAtlas(map);
        if (worldRiver)
            drawMapTaperedOverviewRiver(drawList, points,
                IM_COL32(gm::map_render::kOverviewRiverBank.r,
                         gm::map_render::kOverviewRiverBank.g,
                         gm::map_render::kOverviewRiverBank.b,
                         gm::map_render::kOverviewRiverBank.a),
                width + cellSize * 0.070f);
        else
            drawMapFeaturePolyline(drawList, points,
                IM_COL32(gm::map_render::kLocalRiverBank.r,
                         gm::map_render::kLocalRiverBank.g,
                         gm::map_render::kLocalRiverBank.b,
                         gm::map_render::kLocalRiverBank.a),
                width + cellSize * 0.22f);
        if (worldRiver)
        {
            drawMapTaperedOverviewRiver(drawList, points,
                IM_COL32(gm::map_render::kOverviewRiverWater.r,
                         gm::map_render::kOverviewRiverWater.g,
                         gm::map_render::kOverviewRiverWater.b,
                         gm::map_render::kOverviewRiverWater.a), width);
            drawMapTaperedOverviewRiver(drawList, points,
                IM_COL32(gm::map_render::kOverviewRiverGlint.r,
                         gm::map_render::kOverviewRiverGlint.g,
                         gm::map_render::kOverviewRiverGlint.b,
                         gm::map_render::kOverviewRiverGlint.a),
                std::max(1.0f, width * 0.16f));
        }
        else
        {
            drawMapFeaturePolyline(drawList, points,
                IM_COL32(gm::map_render::kLocalRiverWater.r,
                         gm::map_render::kLocalRiverWater.g,
                         gm::map_render::kLocalRiverWater.b,
                         gm::map_render::kLocalRiverWater.a), width);
        }
        if (gm::map_render::usesTacticalRiverDetail(map) || gm::map_render::usesIllustratedAtlasV5(map))
        {
            if (resolvedPlan)
            {
                for (const auto &primitive:resolvedPlan->primitives)
                    if (primitive.sourceId==feature.id && primitive.kind==MapRenderPrimitiveKind::GeneratedGeometry)
                        drawMapIllustratedAtlasInk(drawList,origin,cellSize,primitive.detailInk);
            }
            else if (gm::map_render::usesTacticalRiverDetail(map))
                drawMapIllustratedAtlasInk(drawList,origin,cellSize,gm::map_render::buildTacticalRiverDetailInk(
                    gm::map_render::tacticalRiverDetailPoints(map,feature),width/cellSize,map.generation.recipe.generatorVersion >= 9));
            else
                drawMapIllustratedAtlasInk(drawList,origin,cellSize,gm::map_render::buildIllustratedAtlasRiverMouthInk(map,feature));
        }
        drawMapCurveProfileStamps(drawList, points, origin, cellSize, width, map, feature);
    }

    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if ((feature.kind != MapGeneratedFeatureKind::Road && feature.kind != MapGeneratedFeatureKind::Trail) || feature.points.size() < 2 || !geometryIsVisible(feature))
            continue;
        if(gm::map_render::usesNaturalLocalRoadSurface(map) || gm::map_render::usesGouacheAtlasArtwork(map)) {
            if(resolvedPlan) {
                for(const auto &primitive:resolvedPlan->primitives)
                    if(primitive.sourceId==feature.id && primitive.kind==MapRenderPrimitiveKind::GeneratedGeometry)
                        drawMapIllustratedAtlasInk(drawList,origin,cellSize,primitive.detailInk);
            } else drawMapIllustratedAtlasInk(drawList,origin,cellSize,
                gm::map_render::usesGouacheAtlasArtwork(map)
                    ? gm::map_render::buildGouacheAtlasRoadInk(map,feature)
                    : gm::map_render::buildIllustratedLocalRoadDetailInk(map,feature,mapFeaturePresentationPoints(map,feature)));
            continue;
        }
        const std::vector<MapFeaturePoint> cartographicControls =
            gm::map_render::overviewCartographicCurvePoints(map, feature);
        const bool resolvedRoad = gm::map_render::usesIllustratedAtlas(map) ||
            ((map.atlasLevel == MapLevel::World || map.atlasLevel == MapLevel::Region) && map.generation.recipe.generatorVersion >= 6);
        const auto *plannedRoad = resolvedPlan ? mapRenderPlanGeneratedGeometry(*resolvedPlan, feature.id) : nullptr;
        const std::vector<ImVec2> points = resolvedRoad
            ? mapPointsToScreen(plannedRoad ? *plannedRoad : gm::map_render::overviewFinalRoadCurvePoints(map, feature))
            : mapSmoothFeaturePoints(mapPointsToScreen(cartographicControls));
        const MapRenderScaleProfile &profile = gm::map_render::atlasPresentationProfile(map);
        const bool localRoad = feature.kind == MapGeneratedFeatureKind::Road &&
                               (map.atlasLevel == MapLevel::Site || map.atlasLevel == MapLevel::Tactical);
        const float width = gm::map_render::usesIllustratedAtlasDetailed(map)
            ? gm::map_render::illustratedAtlasV2RoadWidth(feature,cellSize)
            : std::max(feature.kind == MapGeneratedFeatureKind::Trail ? 2.0f : 3.0f,
                                     feature.width * cellSize * profile.roadWidthCells *
                                         (localRoad ? 1.14f : 1.0f) *
                                         ((map.atlasLevel == MapLevel::World || gm::map_render::usesIllustratedAtlas(map))
                                              ? gm::map_render::kOverviewRoadWidthScale
                                              : 1.0f));
        const bool overview = map.atlasLevel == MapLevel::World || map.atlasLevel == MapLevel::Region;
        const bool worldRoad = map.atlasLevel == MapLevel::World || gm::map_render::usesIllustratedAtlas(map);
        const bool majorRoad = feature.kind == MapGeneratedFeatureKind::Road && feature.width >= 0.88f;
        const bool integratedRoad = gm::map_render::usesIntegratedTacticalRoadInk(map);
        const ImU32 shoulder = worldRoad
                                   ? IM_COL32(gm::map_render::kOverviewRoadShoulder.r,
                                              gm::map_render::kOverviewRoadShoulder.g,
                                              gm::map_render::kOverviewRoadShoulder.b,
                                              gm::map_render::kOverviewRoadShoulder.a)
                                   : IM_COL32(gm::map_render::kLocalRoadShoulder.r,
                                              gm::map_render::kLocalRoadShoulder.g,
                                              gm::map_render::kLocalRoadShoulder.b,
                                              integratedRoad ? (map.generation.recipe.generatorVersion >= 8 ? 12 : 30) : gm::map_render::kLocalRoadShoulder.a);
        const ImU32 surface = feature.kind == MapGeneratedFeatureKind::Trail
                                  ? IM_COL32(gm::map_render::kLocalTrailSurface.r,
                                             gm::map_render::kLocalTrailSurface.g,
                                             gm::map_render::kLocalTrailSurface.b,
                                             integratedRoad ? (map.generation.recipe.generatorVersion >= 8 ? 18 : 50) : gm::map_render::kLocalTrailSurface.a)
                                  : (worldRoad ? IM_COL32(gm::map_render::kOverviewRoadSurface.r,
                                                         gm::map_render::kOverviewRoadSurface.g,
                                                         gm::map_render::kOverviewRoadSurface.b,
                                                         gm::map_render::kOverviewRoadSurface.a)
                                               : IM_COL32(gm::map_render::kLocalRoadSurface.r,
                                                          gm::map_render::kLocalRoadSurface.g,
                                                          gm::map_render::kLocalRoadSurface.b,
                                                          integratedRoad ? (map.generation.recipe.generatorVersion >= 8 ? 18 : 60) : gm::map_render::kLocalRoadSurface.a));
        if (gm::map_render::usesIllustratedLocalV6(map)) {
            drawMapFeaturePolyline(drawList,points,IM_COL32(149,128,87,255),width+std::max(1.4f,cellSize/16.f));
            drawMapFeaturePolyline(drawList,points,IM_COL32(196,174,125,255),width);
        } else {
            drawMapFeaturePolyline(drawList, points, shoulder,
                               width + cellSize * (worldRoad ? 0.055f : (majorRoad ? 0.20f : 0.16f)));
            drawMapFeaturePolyline(drawList, points, surface, width);
        }
        if (cellSize >= 8.f && map.generation.recipe.recipeVersion >= 5 && gm::map_render::usesIllustratedLocalV6(map)) {
            if (resolvedPlan) {
                for (const auto &primitive:resolvedPlan->primitives)
                    if (primitive.sourceId==feature.id && primitive.kind==MapRenderPrimitiveKind::GeneratedGeometry)
                        drawMapIllustratedAtlasInk(drawList,origin,cellSize,primitive.detailInk);
            } else {
                drawMapIllustratedAtlasInk(drawList,origin,cellSize,gm::map_render::buildIllustratedLocalRoadDetailInk(
                    map,feature,mapFeaturePresentationPoints(map,feature)));
            }
        }
        if (feature.kind == MapGeneratedFeatureKind::Road && !overview && width >= 4.0f && !gm::map_render::usesIllustratedLocalV6(map))
        {
            // Local roads read as travelled routes rather than a single flat
            // stripe. Two quiet wheel tracks preserve the centre line and avoid
            // competing with junctions or buildings.
            const std::vector<ImVec2> leftTrack = mapOffsetFeatureLine(points, width * 0.22f);
            const std::vector<ImVec2> rightTrack = mapOffsetFeatureLine(points, -width * 0.22f);
            const float trackWidth = std::clamp(width * 0.085f, 1.0f, 2.2f);
            const ImU32 track = IM_COL32(gm::map_render::kLocalRoadTrack.r,
                                         gm::map_render::kLocalRoadTrack.g,
                                         gm::map_render::kLocalRoadTrack.b,
                                         integratedRoad ? 80 : 150);
            drawMapFeaturePolyline(drawList, leftTrack, track, trackWidth);
            drawMapFeaturePolyline(drawList, rightTrack, track, trackWidth);
        }
        drawMapCurveProfileStamps(drawList, points, origin, cellSize, width, map, feature);
    }

    drawMapContinuousBridges(drawList, origin, cellSize, map, geometryIsVisible);
}
