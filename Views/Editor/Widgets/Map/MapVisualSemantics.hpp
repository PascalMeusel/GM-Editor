#pragma once

#include "Views/Editor/Widgets/Map/MapVisualAssets.hpp"

inline void drawMapCellResolvedSemanticLayers(
    ImDrawList *drawList,
    ImVec2 min,
    ImVec2 max,
    const MapCellModel *cell,
    int x,
    int y,
    const MapCellModel *north,
    const MapCellModel *east,
    const MapCellModel *south,
    const MapCellModel *west,
    const MapCellModel *northEast,
    const MapCellModel *southEast,
    const MapCellModel *southWest,
    const MapCellModel *northWest,
    bool showCode,
    bool allowCandidateContextualAssets = false,
    bool suppressWayRendering = false)
{
    const MapTileRenderContext renderContext = makeMapTileRenderContext(
        cell,
        x,
        y,
        north,
        east,
        south,
        west,
        northEast,
        southEast,
        southWest,
        northWest);

    const MapWayRenderContext wayContext = makeMapWayRenderContext(
        cell,
        x,
        y,
        north,
        east,
        south,
        west,
        northEast,
        southEast,
        southWest,
        northWest);

    MapTileRenderContext baseRenderContext = renderContext;
    if (mapWayRendersAsBridge(wayContext))
        baseRenderContext.kind = MapTileKind::Water;

    if(!drawList)return;
    (void)showCode;
    (void)allowCandidateContextualAssets;
    // Classic maps and battle grids use exactly the same vector fallback as
    // their exported PNG. Old atlas IDs remain loadable compatibility data,
    // but never reappear as noisy tiles through a painting or preview toggle.
    const float size=std::min(max.x-min.x,max.y-min.y);
    const ImVec2 origin(min.x-x*size,min.y-y*size);
    drawMapIllustratedAtlasInk(drawList,origin,size,gm::map_render::classicCellInk(baseRenderContext));
    if(!suppressWayRendering)
        drawMapIllustratedAtlasInk(drawList,origin,size,gm::map_render::classicWayInk(wayContext,renderContext));
}

inline void drawMapCellSemanticLayers(ImDrawList *drawList, ImVec2 min, ImVec2 max, const MapModel &map, int x, int y, bool showCode, bool allowCandidateContextualAssets = false, bool suppressWayRendering = false)
{
    drawMapCellResolvedSemanticLayers(
        drawList,
        min,
        max,
        mapCellAt(map, x, y),
        x,
        y,
        mapCellAt(map, x, y - 1),
        mapCellAt(map, x + 1, y),
        mapCellAt(map, x, y + 1),
        mapCellAt(map, x - 1, y),
        mapCellAt(map, x + 1, y - 1),
        mapCellAt(map, x + 1, y + 1),
        mapCellAt(map, x - 1, y + 1),
        mapCellAt(map, x - 1, y - 1),
        showCode,
        allowCandidateContextualAssets,
        suppressWayRendering);
}

inline std::vector<ImVec2> mapSmoothFeaturePoints(const std::vector<ImVec2> &source)
{
    if (source.size() < 3)
        return source;

    // A single shared curve is used by every visual pass. Three restrained
    // Chaikin passes soften grid-derived corners without moving atlas anchors.
    std::vector<ImVec2> result = source;
    for (int pass = 0; pass < 3; ++pass)
    {
        std::vector<ImVec2> softened;
        softened.reserve(result.size() * 2);
        softened.push_back(result.front());
        for (std::size_t index = 1; index < result.size(); ++index)
        {
            const ImVec2 &a = result[index - 1];
            const ImVec2 &b = result[index];
            softened.emplace_back(a.x * 0.75f + b.x * 0.25f, a.y * 0.75f + b.y * 0.25f);
            softened.emplace_back(a.x * 0.25f + b.x * 0.75f, a.y * 0.25f + b.y * 0.75f);
        }
        softened.push_back(result.back());
        result = std::move(softened);
    }
    return result;
}

inline void drawMapFeaturePolyline(ImDrawList *drawList, const std::vector<ImVec2> &renderedPoints, ImU32 color, float thickness)
{
    if (!drawList || renderedPoints.size() < 2 || thickness <= 0.0f)
        return;

    drawList->AddPolyline(renderedPoints.data(), static_cast<int>(renderedPoints.size()), color,
                          ImDrawFlags_None, thickness);

    // ImDrawList polylines do not guarantee round open ends. Explicit caps
    // keep layered river banks and roads from ending as conspicuous flat bars.
    const float capRadius = std::max(0.5f, thickness * 0.5f);
    drawList->AddCircleFilled(renderedPoints.front(), capRadius, color, 12);
    drawList->AddCircleFilled(renderedPoints.back(), capRadius, color, 12);
}

inline void drawMapTaperedOverviewRiver(ImDrawList *drawList,
                                        const std::vector<ImVec2> &renderedPoints,
                                        ImU32 color,
                                        float mouthThickness)
{
    if (!drawList || renderedPoints.size() < 2 || mouthThickness <= 0.0f)
        return;
    for (std::size_t index = 1; index < renderedPoints.size(); ++index)
    {
        const float factor = gm::map_render::overviewRiverWidthFactor(
            index, renderedPoints.size());
        drawList->AddLine(renderedPoints[index - 1], renderedPoints[index], color,
                          std::max(1.0f, mouthThickness * factor));
    }
    drawList->AddCircleFilled(
        renderedPoints.front(),
        std::max(0.5f, mouthThickness *
                           gm::map_render::overviewRiverWidthFactor(0u, renderedPoints.size()) * 0.5f),
        color, 10);
    drawList->AddCircleFilled(renderedPoints.back(),
                              std::max(0.5f, mouthThickness * 0.5f), color, 12);
}

// Small previews and compatibility callers can supply control points directly.
// The main map renderer precomputes the curve once and uses
// drawMapFeaturePolyline for all layered passes.
inline void drawMapSmoothFeatureLine(ImDrawList *drawList, const std::vector<ImVec2> &points, ImU32 color, float thickness)
{
    drawMapFeaturePolyline(drawList, mapSmoothFeaturePoints(points), color, thickness);
}

inline std::vector<ImVec2> mapOffsetFeatureLine(const std::vector<ImVec2> &points, float offset)
{
    if (points.size() < 2 || std::abs(offset) < 0.01f)
        return points;
    std::vector<ImVec2> result;
    result.reserve(points.size());
    for (std::size_t index = 0; index < points.size(); ++index)
    {
        const ImVec2 &previous = points[index == 0 ? 0 : index - 1];
        const ImVec2 &next = points[index + 1 < points.size() ? index + 1 : index];
        const float dx = next.x - previous.x;
        const float dy = next.y - previous.y;
        const float length = std::sqrt(dx * dx + dy * dy);
        if (length <= 0.001f)
            result.push_back(points[index]);
        else
            result.emplace_back(points[index].x - dy / length * offset,
                                points[index].y + dx / length * offset);
    }
    return result;
}

inline ImU32 mapContinuousTerrainColor(MapTileKind kind, int variation)
{
    const int delta = (variation % 5 - 2) * 4;
    auto color = [&](int r, int g, int b, int a = 255) {
        return IM_COL32(std::clamp(r + delta, 0, 255), std::clamp(g + delta, 0, 255), std::clamp(b + delta, 0, 255), a);
    };
    switch (kind)
    {
    case MapTileKind::Forest: return color(45, 73, 55);
    case MapTileKind::Mountain: return color(91, 91, 78);
    case MapTileKind::Hill: return color(105, 106, 65);
    case MapTileKind::Stone: return color(111, 104, 87);
    case MapTileKind::Dirt: return color(135, 105, 67);
    case MapTileKind::Swamp: return color(61, 84, 64);
    case MapTileKind::Desert: return color(161, 137, 83);
    case MapTileKind::Snow: return color(184, 190, 178);
    case MapTileKind::Ice: return color(128, 171, 177);
    case MapTileKind::Water: return color(38, 112, 137);
    case MapTileKind::Room: return color(104, 96, 80);
    case MapTileKind::Cave: return color(87, 78, 63);
    case MapTileKind::Wall: return color(42, 40, 35);
    case MapTileKind::Chasm:
    case MapTileKind::Pit: return color(24, 25, 24);
    default: return color(78, 101, 55);
    }
}

inline ImU32 mapAverageContinuousTerrainVertexColor(
    const MapModel &map,
    float vertexX,
    float vertexY,
    const std::vector<MapTileKind> *overviewGroundProjection = nullptr)
{
    if (gm::map_render::usesIllustratedLocalV6(map)) {
        const auto c=gm::map_render::illustratedLocalTerrainColor(map,vertexX,vertexY);
        return IM_COL32(c.r,c.g,c.b,c.a);
    }
    if (gm::map_render::usesIllustratedAtlas(map))
    {
        const auto c = gm::map_render::illustratedAtlasTerrainColor(map, vertexX, vertexY, overviewGroundProjection);
        return IM_COL32(c.r, c.g, c.b, c.a);
    }
    float landRed = 0.0f;
    float landGreen = 0.0f;
    float landBlue = 0.0f;
    float landWeight = 0.0f;
    float waterRed = 0.0f;
    float waterGreen = 0.0f;
    float waterBlue = 0.0f;
    float waterWeight = 0.0f;
    float domainLandWeight = 0.0f;
    float domainTotalWeight = 0.0f;
    const bool world = gm::map_render::overviewUsesAtlasTerrain(map);
    const gm::map_render::OverviewSamplePoint organicSample = world
        ? gm::map_render::overviewOrganicTerrainSample(
              map, static_cast<float>(vertexX), static_cast<float>(vertexY))
        : gm::map_render::OverviewSamplePoint{static_cast<float>(vertexX),
                                              static_cast<float>(vertexY)};
    const float sampleX = organicSample.x;
    const float sampleY = organicSample.y;
    const bool soft = gm::map_render::usesTacticalSoftTerrainBlend(map);
    const bool filtered = world || soft;
    const int radius = filtered ? 2 : 1;
    const int firstX = filtered ? static_cast<int>(std::floor(sampleX)) - radius
                             : static_cast<int>(vertexX) - 1;
    const int lastX = filtered ? static_cast<int>(std::floor(sampleX)) + radius
                            : static_cast<int>(vertexX);
    const int firstY = filtered ? static_cast<int>(std::floor(sampleY)) - radius
                             : static_cast<int>(vertexY) - 1;
    const int lastY = filtered ? static_cast<int>(std::floor(sampleY)) + radius
                            : static_cast<int>(vertexY);
    const auto axisWeight = [world](float distance) {
        if (!world)
            return 1.0f;
        const float normalized = std::clamp(
            1.0f - std::abs(distance) / gm::map_render::kOverviewTerrainBlendRadiusCells,
            0.0f, 1.0f);
        return normalized * normalized * (3.0f - 2.0f * normalized);
    };
    const auto domainAxisWeight = [world](float distance) {
        if (!world)
            return 1.0f;
        const float normalized = std::clamp(
            1.0f - std::abs(distance) / gm::map_render::kOverviewCoastBlendRadiusCells,
            0.0f, 1.0f);
        return normalized * normalized * (3.0f - 2.0f * normalized);
    };
    for (int y = firstY; y <= lastY; ++y)
    {
        for (int x = firstX; x <= lastX; ++x)
        {
            if (!isMapCellInside(map, x, y))
                continue;
            const float distanceX = static_cast<float>(x) + 0.5f - sampleX;
            const float distanceY = static_cast<float>(y) + 0.5f - sampleY;
            const float weight = soft ? gm::map_render::tacticalTerrainBlendWeight(distanceX,distanceY) : axisWeight(distanceX) * axisWeight(distanceY);
            const float domainWeight = domainAxisWeight(distanceX) * domainAxisWeight(distanceY);
            if (weight <= 0.0001f && domainWeight <= 0.0001f)
                continue;
            const MapTileKind kind = world
                                         ? (overviewGroundProjection
                                                ? gm::map_render::overviewGroundKindFromProjection(
                                                      map, *overviewGroundProjection, x, y)
                                                : gm::map_render::overviewGroundKindForRender(map, x, y))
                                         : mapVisualGroundKind(map, x, y);
            const bool land = !world || gm::map_render::overviewGroundKindIsLand(kind);
            if (domainWeight > 0.0001f)
            {
                domainTotalWeight += domainWeight;
                if (land)
                    domainLandWeight += domainWeight;
            }
            if (weight <= 0.0001f)
                continue;
            // Variation changes slowly in world space. This keeps broad fields
            // painterly without reintroducing a checkerboard at every cell.
            const int variation = deterministicMapTileIndex(x / 3, y / 3,
                static_cast<int>(kind) * 37 + static_cast<int>(map.generation.recipe.seed & 0x7fffu), 5);
            // Match the CPU RenderPlan/export treatment: a continental forest
            // is a readable map biome, not the near-black close-range forest
            // floor used on site and tactical maps.
            ImU32 terrainColor = mapContinuousTerrainColor(kind, variation);
            if (gm::map_render::overviewUsesAtlasTerrain(map))
            {
                const int delta = (variation % 5 - 2) * 4;
                if (kind == MapTileKind::Water || kind == MapTileKind::Ice)
                    terrainColor = IM_COL32(
                        std::clamp(static_cast<int>(gm::map_render::kOverviewOceanBase.r) + delta / 3, 0, 255),
                        std::clamp(static_cast<int>(gm::map_render::kOverviewOceanBase.g) + delta / 3, 0, 255),
                        std::clamp(static_cast<int>(gm::map_render::kOverviewOceanBase.b) + delta / 3, 0, 255), 255);
                else if (kind == MapTileKind::Empty)
                    terrainColor = IM_COL32(
                        std::clamp(static_cast<int>(gm::map_render::kOverviewPlainBase.r) + delta, 0, 255),
                        std::clamp(static_cast<int>(gm::map_render::kOverviewPlainBase.g) + delta, 0, 255),
                        std::clamp(static_cast<int>(gm::map_render::kOverviewPlainBase.b) + delta, 0, 255), 255);
                else if (kind == MapTileKind::Forest)
                    terrainColor = IM_COL32(
                        std::clamp(static_cast<int>(gm::map_render::kOverviewForestBase.r) + delta, 0, 255),
                        std::clamp(static_cast<int>(gm::map_render::kOverviewForestBase.g) + delta, 0, 255),
                        std::clamp(static_cast<int>(gm::map_render::kOverviewForestBase.b) + delta, 0, 255), 255);
                else if (kind == MapTileKind::Mountain || kind == MapTileKind::Hill)
                    // A mountain cell is the muted ground under a macro ridge,
                    // not a separate grey political-looking region.
                    terrainColor = IM_COL32(
                        std::clamp(static_cast<int>(gm::map_render::kOverviewMountainGround.r) + delta, 0, 255),
                        std::clamp(static_cast<int>(gm::map_render::kOverviewMountainGround.g) + delta, 0, 255),
                        std::clamp(static_cast<int>(gm::map_render::kOverviewMountainGround.b) + delta, 0, 255), 255);
            }
            const ImVec4 color = ImGui::ColorConvertU32ToFloat4(terrainColor);
            if (land)
            {
                landRed += color.x * 255.0f * weight;
                landGreen += color.y * 255.0f * weight;
                landBlue += color.z * 255.0f * weight;
                landWeight += weight;
            }
            else
            {
                waterRed += color.x * 255.0f * weight;
                waterGreen += color.y * 255.0f * weight;
                waterBlue += color.z * 255.0f * weight;
                waterWeight += weight;
            }
        }
    }
    const ImVec4 fallbackLand = world
                                    ? ImVec4(gm::map_render::kOverviewPlainBase.r / 255.0f,
                                             gm::map_render::kOverviewPlainBase.g / 255.0f,
                                             gm::map_render::kOverviewPlainBase.b / 255.0f, 1.0f)
                                    : ImGui::ColorConvertU32ToFloat4(
                                          mapContinuousTerrainColor(MapTileKind::Empty, 2));
    const ImVec4 fallbackWater = world
                                     ? ImVec4(gm::map_render::kOverviewOceanBase.r / 255.0f,
                                              gm::map_render::kOverviewOceanBase.g / 255.0f,
                                              gm::map_render::kOverviewOceanBase.b / 255.0f, 1.0f)
                                     : ImGui::ColorConvertU32ToFloat4(
                                           mapContinuousTerrainColor(MapTileKind::Water, 2));
    const float averageLandRed = landWeight > 0.0001f ? landRed / landWeight : fallbackLand.x * 255.0f;
    const float averageLandGreen = landWeight > 0.0001f ? landGreen / landWeight : fallbackLand.y * 255.0f;
    const float averageLandBlue = landWeight > 0.0001f ? landBlue / landWeight : fallbackLand.z * 255.0f;
    const float averageWaterRed = waterWeight > 0.0001f ? waterRed / waterWeight : fallbackWater.x * 255.0f;
    const float averageWaterGreen = waterWeight > 0.0001f ? waterGreen / waterWeight : fallbackWater.y * 255.0f;
    const float averageWaterBlue = waterWeight > 0.0001f ? waterBlue / waterWeight : fallbackWater.z * 255.0f;
    const float rawLandCoverage = !world
                                      ? 1.0f
                                      : (domainTotalWeight > 0.0001f
                                             ? std::clamp(domainLandWeight / domainTotalWeight, 0.0f, 1.0f)
                                             : (landWeight >= waterWeight ? 1.0f : 0.0f));
    const float landCoverage = world
                                   ? gm::map_render::overviewCartographicLandCoverage(
                                         map, sampleX, sampleY, rawLandCoverage)
                                   : rawLandCoverage;
    const float red = averageWaterRed + (averageLandRed - averageWaterRed) * landCoverage;
    const float green = averageWaterGreen + (averageLandGreen - averageWaterGreen) * landCoverage;
    const float blue = averageWaterBlue + (averageLandBlue - averageWaterBlue) * landCoverage;
    // Ocean needs quiet depth variation, not the same high-contrast diagonal
    // pattern as land relief. Blend the amplitude across the soft coast so no
    // abrupt texture change appears at the shoreline.
    const float tonalAmplitude = 2.2f + landCoverage * 9.3f;
    const int tonalOffset = world
                                ? static_cast<int>(std::lround(
                                      gm::map_render::overviewTerrainTone(
                                          map, sampleX, sampleY) * tonalAmplitude +
                                      gm::map_render::overviewTerrainGrain(
                                          map, sampleX, sampleY) *
                                          (1.2f + landCoverage * 2.3f)))
                                : 0;
    return IM_COL32(
        std::clamp(static_cast<int>(std::lround(red)) + tonalOffset, 0, 255),
        std::clamp(static_cast<int>(std::lround(green)) + tonalOffset, 0, 255),
        std::clamp(static_cast<int>(std::lround(blue)) + tonalOffset / 2, 0, 255), 255);
}
