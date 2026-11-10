#pragma once

#include "Views/Editor/Widgets/Map/MapVisualProduction.hpp"

struct MapAtlasTerrainCache
{
    std::uint64_t key = 0;
    std::uint64_t lastUse = 0;
    std::vector<MapTileKind> ground;
    std::vector<ImU32> colors;
    int fineSubdivisions = 0;
    std::unordered_map<std::uint64_t,ImU32> fineColors;
    std::vector<gm::map_render::OverviewCoastSegment> coast;
    bool coastReady = false;
};

inline int mapAtlasCoastSubdivisions(float cellSize)
{
    int subdivisions=4;
    while(subdivisions<32 && cellSize/subdivisions>4.f) subdivisions*=2;
    return subdivisions;
}

inline MapAtlasTerrainCache &mapAtlasTerrainCache(const MapModel &map)
{
    // Four bounded entries cover the editor and its three candidate previews.
    // Hash semantic terrain, including unsaved brush edits and filtered player
    // cells, rather than trusting revision counters or a map's address.
    static thread_local std::array<MapAtlasTerrainCache, 4> entries;
    static thread_local std::uint64_t use = 0;
    const std::uint64_t key = gm::map_render::overviewTerrainContentHash(map);
    for (auto &entry : entries)
        if (entry.lastUse != 0 && entry.key == key)
        {
            entry.lastUse = ++use;
            return entry;
        }
    auto &entry = *std::min_element(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
        return a.lastUse < b.lastUse;
    });
    entry = {};
    entry.key = key;
    entry.lastUse = ++use;
    entry.ground = gm::map_render::buildOverviewGroundProjection(map);
    return entry;
}

inline void drawMapOverviewCoastTreatment(ImDrawList *drawList,
                                          ImVec2 origin,
                                          float cellSize,
                                          const MapModel &map)
{
    if (!drawList || (map.atlasLevel != MapLevel::World && !gm::map_render::usesIllustratedAtlas(map)) || cellSize <= 0.0f)
        return;
    if (gm::map_render::kOverviewCoastWaterShadow.a == 0 &&
        gm::map_render::kOverviewCoastLine.a == 0 &&
        gm::map_render::kOverviewCoastLandHighlight.a == 0)
        return;
    auto &terrainCache = mapAtlasTerrainCache(map);
    if (!terrainCache.coastReady)
    {
        terrainCache.coast = gm::map_render::buildOverviewCoastSegments(map);
        terrainCache.coastReady = true;
    }
    const auto &segments = terrainCache.coast;
    if (segments.empty())
        return;

    const auto asImColor = [](gm::map_render::OverviewRenderColor color) {
        return IM_COL32(color.r, color.g, color.b, color.a);
    };
    const ImVec2 clipMin = drawList->GetClipRectMin();
    const ImVec2 clipMax = drawList->GetClipRectMax();
    const auto drawPass = [&](float offsetTowardLandCells,
                              float strokeCells,
                              float minimumWidth,
                              float maximumWidth,
                              gm::map_render::OverviewRenderColor color) {
        if (color.a == 0)
            return;
        const float offset = offsetTowardLandCells * cellSize;
        const float width = std::clamp(strokeCells * cellSize,
                                       minimumWidth, maximumWidth);
        for (const gm::map_render::OverviewCoastSegment &segment : segments)
        {
            const ImVec2 start(
                origin.x + (segment.startX + segment.landNormalX * offsetTowardLandCells) * cellSize,
                origin.y + (segment.startY + segment.landNormalY * offsetTowardLandCells) * cellSize);
            const ImVec2 end(
                origin.x + (segment.endX + segment.landNormalX * offsetTowardLandCells) * cellSize,
                origin.y + (segment.endY + segment.landNormalY * offsetTowardLandCells) * cellSize);
            const float margin = width + std::abs(offset) + 2.0f;
            if (std::max(start.x, end.x) + margin < clipMin.x ||
                std::min(start.x, end.x) - margin > clipMax.x ||
                std::max(start.y, end.y) + margin < clipMin.y ||
                std::min(start.y, end.y) - margin > clipMax.y)
                continue;
            drawList->AddLine(start, end, asImColor(color), width);
        }
    };

    // One broad, low-opacity land-side lift clarifies the coast without turning
    // it into a dark double UI wire. The shared policy keeps obsolete shadow
    // passes disabled, so editor and export retain exactly one visible edge.
    drawPass(-gm::map_render::kOverviewCoastWaterOffsetCells,
             gm::map_render::kOverviewCoastWaterStrokeCells,
             1.2f, 4.0f, gm::map_render::kOverviewCoastWaterShadow);
    drawPass(0.0f, gm::map_render::kOverviewCoastLineStrokeCells,
             1.0f, 2.0f, gm::map_render::kOverviewCoastLine);
    drawPass(gm::map_render::kOverviewCoastLandOffsetCells,
             gm::map_render::kOverviewCoastLandStrokeCells,
             1.0f, 2.0f, gm::map_render::kOverviewCoastLandHighlight);
}

inline const char *mapCurveProfileSemanticRole(const MapGeneratedFeature &feature)
{
    if (feature.kind == MapGeneratedFeatureKind::River)
    {
        if (feature.materialId.find("canal") != std::string::npos)
            return "canal";
        if (feature.materialId.find("creek") != std::string::npos || feature.width < 0.72f)
            return "creek";
        return feature.width >= 1.35f ? "river_deep" : "river";
    }
    if (feature.kind == MapGeneratedFeatureKind::Trail)
        return "path";
    if (feature.materialId.find("stone") != std::string::npos)
        return "road_stone";
    if (feature.materialId.find("gravel") != std::string::npos)
        return "road_gravel";
    return "road_dirt";
}

inline const char *mapBridgeStampSemanticRole(const MapGeneratedFeature &feature)
{
    if (feature.materialId.find("ford") != std::string::npos)
        return "ford";
    if (feature.materialId.find("pier") != std::string::npos)
        return "pier";
    if (feature.materialId.find("stone") != std::string::npos)
        return feature.materialId.find("dressed") != std::string::npos
                   ? "bridge_stone_dressed"
                   : "bridge_stone_rough";
    return feature.materialId.find("heavy") != std::string::npos || feature.width >= 1.35f
               ? "bridge_wood_heavy"
               : "bridge_wood_light";
}

inline void drawMapCurveProfileStamps(ImDrawList *drawList,
                                      const std::vector<ImVec2> &points,
                                      ImVec2 origin,
                                      float cellSize,
                                      float featureWidth,
                                      const MapModel &map,
                                      const MapGeneratedFeature &feature)
{
    if (!drawList || points.size() < 2 || cellSize <= 0.0f ||
        map.generation.recipe.styleVersion < 3 ||
        !mapStyleProductionSnapshotAvailable(map.generation.recipe.styleSnapshotId))
        return;

    // Current curve images are oval accents rather than connectable ribbons.
    // On World they produced broken grey ghosts beside the authoritative road
    // and river bands; local maps had the same doubling problem. Region is the
    // only scale at which the sparse painterly accent remains appropriate.
    if (!gm::map_render::overviewUsesCurveProfileAccents(map))
        return;

    float totalLength = 0.0f;
    for (std::size_t index = 1; index < points.size(); ++index)
    {
        const float dx = points[index].x - points[index - 1].x;
        const float dy = points[index].y - points[index - 1].y;
        totalLength += std::sqrt(dx * dx + dy * dy);
    }
    if (totalLength < cellSize * 0.25f)
        return;

    // Overview maps receive a few long painterly accents; tactical maps use
    // shorter, closer stamps. The gap-free vector stroke remains visible
    // beneath them, so profile art never has to be repeated once per cell.
    const MapRenderScaleProfile &profile = mapRenderScaleProfile(map.atlasLevel);
    const float spacing = cellSize * std::clamp(profile.materialCellsPerRepeat * 1.15f, 6.0f, 12.0f);
    const float nominalStampLength = cellSize * std::clamp((spacing / cellSize) * 0.42f, 2.5f, 4.5f);
    const float stampLength = std::min(nominalStampLength, std::max(cellSize, totalLength + cellSize * 0.35f));
    const bool river = feature.kind == MapGeneratedFeatureKind::River;
    const float stampWidth = std::max(cellSize * (river ? 1.18f : 1.02f), featureWidth * (river ? 2.15f : 2.05f));
    const ImU32 tint = river ? IM_COL32(255, 255, 255, 48) : IM_COL32(255, 255, 255, 38);
    const char *semanticRole = mapCurveProfileSemanticRole(feature);

    const ImVec2 clipMin = drawList->GetClipRectMin();
    const ImVec2 clipMax = drawList->GetClipRectMax();
    const float cullMargin = std::max(stampLength, stampWidth);
    float nextStampDistance = totalLength < spacing ? totalLength * 0.5f : spacing * 0.55f;
    float traversed = 0.0f;
    int stampIndex = 0;
    for (std::size_t index = 1; index < points.size() && nextStampDistance <= totalLength + 0.01f; ++index)
    {
        const ImVec2 &a = points[index - 1];
        const ImVec2 &b = points[index];
        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        const float segmentLength = std::sqrt(dx * dx + dy * dy);
        if (segmentLength <= 0.001f)
            continue;
        const float segmentEnd = traversed + segmentLength;
        while (nextStampDistance <= segmentEnd + 0.001f)
        {
            const float amount = std::clamp((nextStampDistance - traversed) / segmentLength, 0.0f, 1.0f);
            const ImVec2 center(a.x + dx * amount, a.y + dy * amount);
            if (center.x + cullMargin >= clipMin.x && center.x - cullMargin <= clipMax.x &&
                center.y + cullMargin >= clipMin.y && center.y - cullMargin <= clipMax.y)
            {
                const int mapX = static_cast<int>(std::floor((center.x - origin.x) / cellSize));
                const int mapY = static_cast<int>(std::floor((center.y - origin.y) / cellSize));
                drawProductionAssetRoleRotated(
                    drawList, center, stampLength, stampWidth, std::atan2(dy, dx), map,
                    "curve_profile", semanticRole, mapX, mapY, 1200 + stampIndex, tint,
                    mapProductionAssetZoom(cellSize));
            }
            ++stampIndex;
            nextStampDistance += spacing;
        }
        traversed = segmentEnd;
    }
}
