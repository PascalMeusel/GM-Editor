#pragma once

#include "Views/Editor/Widgets/Map/MapVisualSymbols.hpp"

inline bool mapFeaturePolygonContainsPoint(const MapGeneratedFeature &feature, float x, float y)
{
    if (!feature.closed || feature.points.size() < 3)
        return false;
    bool inside = false;
    for (std::size_t current = 0, previous = feature.points.size() - 1;
         current < feature.points.size(); previous = current++)
    {
        const MapFeaturePoint &a = feature.points[current];
        const MapFeaturePoint &b = feature.points[previous];
        const bool crosses = (a.y > y) != (b.y > y);
        if (!crosses)
            continue;
        const float intersectionX = (b.x - a.x) * (y - a.y) /
                                        ((b.y - a.y) + (b.y == a.y ? 0.00001f : 0.0f)) +
                                    a.x;
        if (x < intersectionX)
            inside = !inside;
    }
    return inside;
}

inline bool mapWorldPointIsLand(const MapModel &map, float x, float y)
{
    const int cellX = static_cast<int>(std::floor(x));
    const int cellY = static_cast<int>(std::floor(y));
    return isMapCellInside(map, cellX, cellY) &&
           gm::map_render::overviewGroundKindIsLand(
               gm::map_render::overviewGroundKindForRender(map, cellX, cellY));
}

inline MapFeaturePoint mapTerritoryLandLabelPoint(const MapModel &map, const MapGeneratedFeature &feature)
{
    MapFeaturePoint anchor{};
    if (gm::map_render::overviewTerritoryLabelPoint(map, feature, anchor))
        return anchor;
    return feature.points.empty() ? MapFeaturePoint{} : feature.points.front();
}

inline void drawMapLandMaskedTerritoryBoundary(ImDrawList *drawList,
                                                ImVec2 origin,
                                                float cellSize,
                                                const MapModel &map,
                                                const MapGeneratedFeature &feature,
                                                ImU32 color,
                                                float thickness)
{
    if (!drawList || !feature.closed || feature.points.size() < 3)
        return;
    std::vector<ImVec2> currentRun;
    float currentRunLengthCells = 0.0f;
    const auto flushRun = [&] {
        // Short coastal remnants and polygonisation splinters do not convey a
        // political division. Keep only coherent internal-border runs.
        if (currentRun.size() >= 2 && currentRunLengthCells >= 1.35f)
            drawList->AddPolyline(currentRun.data(), static_cast<int>(currentRun.size()),
                                  color, ImDrawFlags_None, thickness);
        currentRun.clear();
        currentRunLengthCells = 0.0f;
    };
    for (std::size_t index = 0; index < feature.points.size(); ++index)
    {
        const MapFeaturePoint &start = feature.points[index];
        const MapFeaturePoint &end = feature.points[(index + 1) % feature.points.size()];
        const float dx = end.x - start.x;
        const float dy = end.y - start.y;
        const float length = std::sqrt(dx * dx + dy * dy);
        const int pieces = std::max(1, static_cast<int>(std::ceil(length / 0.35f)));
        const float normalX = length > 0.001f ? -dy / length * 0.42f : 0.0f;
        const float normalY = length > 0.001f ? dx / length * 0.42f : 0.0f;
        for (int piece = 0; piece < pieces; ++piece)
        {
            const float first = static_cast<float>(piece) / static_cast<float>(pieces);
            const float second = static_cast<float>(piece + 1) / static_cast<float>(pieces);
            const float middle = (first + second) * 0.5f;
            const float middleX = start.x + dx * middle;
            const float middleY = start.y + dy * middle;
            // A political border exists between two land domains. Requiring
            // land on both sides prevents territory polygons from repainting
            // the continental coast as a second, coloured coastline.
            if (!mapWorldPointIsLand(map, middleX + normalX, middleY + normalY) ||
                !mapWorldPointIsLand(map, middleX - normalX, middleY - normalY))
            {
                flushRun();
                continue;
            }
            const ImVec2 screenStart(origin.x + (start.x + dx * first) * cellSize,
                                     origin.y + (start.y + dy * first) * cellSize);
            const ImVec2 screenEnd(origin.x + (start.x + dx * second) * cellSize,
                                   origin.y + (start.y + dy * second) * cellSize);
            if (currentRun.empty())
                currentRun.push_back(screenStart);
            else
            {
                const ImVec2 &last = currentRun.back();
                if (std::abs(last.x - screenStart.x) > 0.75f ||
                    std::abs(last.y - screenStart.y) > 0.75f)
                {
                    flushRun();
                    currentRun.push_back(screenStart);
                }
            }
            currentRun.push_back(screenEnd);
            currentRunLengthCells += std::sqrt(
                (screenEnd.x - screenStart.x) * (screenEnd.x - screenStart.x) +
                (screenEnd.y - screenStart.y) * (screenEnd.y - screenStart.y)) /
                std::max(0.001f, cellSize);
        }
    }
    flushRun();
}

inline void drawMapSharedPoliticalBorders(ImDrawList *drawList,
                                          ImVec2 origin,
                                          float cellSize,
                                          const MapModel &map)
{
    if (!drawList || map.atlasLevel != MapLevel::World)
        return;
    const std::vector<gm::map_render::OverviewPoliticalBorderRun> runs =
        gm::map_render::buildOverviewPoliticalBorderRuns(map);
    const auto toImColor = [](gm::map_render::OverviewRenderColor color) {
        return IM_COL32(color.r, color.g, color.b, color.a);
    };
    const ImU32 haloColor = toImColor(gm::map_render::kOverviewBorderHalo);
    const ImU32 inkColor = toImColor(gm::map_render::kOverviewBorderInk);
    const float haloThickness = std::max(1.8f, cellSize * 0.12f);
    const float inkThickness = std::max(1.0f, cellSize * 0.045f);
    for (const gm::map_render::OverviewPoliticalBorderRun &run : runs)
    {
        const std::vector<gm::map_render::OverviewPoliticalStrokeSegment> dashes =
            gm::map_render::buildOverviewPoliticalDashes(run);
        for (const gm::map_render::OverviewPoliticalStrokeSegment &dash : dashes)
        {
            const ImVec2 start(origin.x + dash.start.x * cellSize,
                               origin.y + dash.start.y * cellSize);
            const ImVec2 end(origin.x + dash.end.x * cellSize,
                             origin.y + dash.end.y * cellSize);
            if ((haloColor >> IM_COL32_A_SHIFT) != 0u)
                drawList->AddLine(start, end, haloColor, haloThickness);
            drawList->AddLine(start, end, inkColor, inkThickness);
        }
    }
}
