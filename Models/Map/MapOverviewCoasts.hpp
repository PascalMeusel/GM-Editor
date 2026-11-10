#pragma once

#include "Models/Map/MapOverviewCurves.hpp"

namespace gm::map_render
{
inline float overviewLandCoverage(const MapModel &map, float mapX, float mapY)
{
    if (map.width <= 0 || map.height <= 0)
        return 0.0f;
    const std::vector<MapTileKind> groundProjection =
        buildOverviewGroundProjection(map);
    std::vector<std::uint8_t> landMask(
        static_cast<std::size_t>(map.width) * static_cast<std::size_t>(map.height), 0u);
    for (int y = 0; y < map.height; ++y)
        for (int x = 0; x < map.width; ++x)
            landMask[static_cast<std::size_t>(mapCellIndex(map, x, y))] =
                detail::isOverviewLandKind(overviewGroundKindFromProjection(
                    map, groundProjection, x, y))
                    ? 1u
                    : 0u;
    return detail::overviewLandCoverageFromMask(map, landMask, mapX, mapY);
}

// Marching-squares contour of the soft semantic land mask. This is cheap
// derived geometry (one (width+1)*(height+1) scalar field) and avoids either
// renderer tracing the hard cell boundary or guessing from painted pixels.
namespace detail
{
inline std::vector<OverviewCoastSegment> buildOverviewCoastSegmentsCached(
    const MapModel &map, const OverviewGroundSamplingCache *cache)
{
    std::vector<OverviewCoastSegment> result;
    if ((map.atlasLevel != MapLevel::World && !(map.atlasLevel == MapLevel::Region &&
            map.generation.recipe.styleId == "illustrated_atlas")) || map.width <= 0 || map.height <= 0)
        return result;

    // Half-cell contours still retained the source grid's saw-tooth rhythm at
    // overview zoom. Four samples per semantic cell are cheap for a 72x48
    // world and give both renderers a stable, substantially smoother contour.
    constexpr int contourSubdivisions = 4;
    const int fieldWidth = map.width * contourSubdivisions + 1;
    const int fieldHeight = map.height * contourSubdivisions + 1;
    const auto *providedGround = cache ? cache->projection(map) : nullptr;
    const std::vector<MapTileKind> generatedGround =
        providedGround ? std::vector<MapTileKind>{} : buildOverviewGroundProjection(map);
    const auto &groundProjection = providedGround ? *providedGround : generatedGround;
    std::vector<std::uint8_t> landMask(
        static_cast<std::size_t>(map.width) * static_cast<std::size_t>(map.height), 0u);
    for (int y = 0; y < map.height; ++y)
        for (int x = 0; x < map.width; ++x)
            landMask[static_cast<std::size_t>(mapCellIndex(map, x, y))] =
                detail::isOverviewLandKind(overviewGroundKindFromProjection(
                    map, groundProjection, x, y))
                    ? 1u
                    : 0u;
    const std::size_t landCellCount = static_cast<std::size_t>(
        std::count(landMask.begin(), landMask.end(), static_cast<std::uint8_t>(1u)));
    if (landCellCount == 0u || landCellCount == landMask.size())
        return result;
    std::vector<float> coverage(static_cast<std::size_t>(fieldWidth) *
                                static_cast<std::size_t>(fieldHeight), 0.0f);
    for (int y = 0; y < fieldHeight; ++y)
        for (int x = 0; x < fieldWidth; ++x)
            coverage[static_cast<std::size_t>(y * fieldWidth + x)] =
                detail::overviewLandCoverageFromMaskCached(
                    map, landMask,
                    static_cast<float>(x) / contourSubdivisions,
                    static_cast<float>(y) / contourSubdivisions,
                    !(map.generation.recipe.styleId == "illustrated_atlas" &&
                      ((map.generation.recipe.styleVersion == 3 && map.generation.recipe.styleSnapshotId == "illustrated_atlas_v3") ||
                       ((map.generation.recipe.styleVersion == 4 && map.generation.recipe.styleSnapshotId == "illustrated_atlas_v4") ||
          (((map.generation.recipe.styleVersion == 5 && map.generation.recipe.styleSnapshotId == "illustrated_atlas_v5") || (map.generation.recipe.styleVersion == 6 && map.generation.recipe.styleSnapshotId == "illustrated_atlas_v6")))))), cache);

    constexpr float threshold = 0.5f;
    struct Point
    {
        float x = 0.0f;
        float y = 0.0f;
    };
    const auto crossingAmount = [threshold](float first, float second) {
        const float difference = second - first;
        if (std::abs(difference) <= 0.00001f)
            return 0.5f;
        return std::clamp((threshold - first) / difference, 0.0f, 1.0f);
    };

    for (int y = 0; y + 1 < fieldHeight; ++y)
    {
        for (int x = 0; x + 1 < fieldWidth; ++x)
        {
            const auto at = [&](int fieldX, int fieldY) {
                return coverage[static_cast<std::size_t>(fieldY * fieldWidth + fieldX)];
            };
            const float topLeft = at(x, y);
            const float topRight = at(x + 1, y);
            const float bottomRight = at(x + 1, y + 1);
            const float bottomLeft = at(x, y + 1);
            const int mask = (topLeft >= threshold ? 1 : 0) |
                             (topRight >= threshold ? 2 : 0) |
                             (bottomRight >= threshold ? 4 : 0) |
                             (bottomLeft >= threshold ? 8 : 0);
            if (mask == 0 || mask == 15)
                continue;

            const Point top{(static_cast<float>(x) + crossingAmount(topLeft, topRight)) /
                                contourSubdivisions,
                            static_cast<float>(y) / contourSubdivisions};
            const Point right{static_cast<float>(x + 1) / contourSubdivisions,
                              (static_cast<float>(y) + crossingAmount(topRight, bottomRight)) /
                                  contourSubdivisions};
            const Point bottom{(static_cast<float>(x) + crossingAmount(bottomLeft, bottomRight)) /
                                   contourSubdivisions,
                               static_cast<float>(y + 1) / contourSubdivisions};
            const Point left{static_cast<float>(x) / contourSubdivisions,
                             (static_cast<float>(y) + crossingAmount(topLeft, bottomLeft)) /
                                 contourSubdivisions};

            const auto bilinearCoverage = [&](float mapPointX, float mapPointY) {
                const float u = std::clamp(mapPointX * contourSubdivisions -
                                               static_cast<float>(x),
                                           0.0f, 1.0f);
                const float v = std::clamp(mapPointY * contourSubdivisions -
                                               static_cast<float>(y),
                                           0.0f, 1.0f);
                const float upper = topLeft + (topRight - topLeft) * u;
                const float lower = bottomLeft + (bottomRight - bottomLeft) * u;
                return upper + (lower - upper) * v;
            };
            const auto appendSegment = [&](Point start, Point end) {
                const float deltaX = end.x - start.x;
                const float deltaY = end.y - start.y;
                const float length = std::sqrt(deltaX * deltaX + deltaY * deltaY);
                if (length <= 0.0001f)
                    return;
                float normalX = -deltaY / length;
                float normalY = deltaX / length;
                const float middleX = (start.x + end.x) * 0.5f;
                const float middleY = (start.y + end.y) * 0.5f;
                constexpr float probeDistance = 0.08f;
                const float positiveCoverage = bilinearCoverage(
                    middleX + normalX * probeDistance,
                    middleY + normalY * probeDistance);
                const float negativeCoverage = bilinearCoverage(
                    middleX - normalX * probeDistance,
                    middleY - normalY * probeDistance);
                if (positiveCoverage < negativeCoverage)
                {
                    normalX = -normalX;
                    normalY = -normalY;
                }
                result.push_back({start.x, start.y, end.x, end.y, normalX, normalY});
            };

            const float centerCoverage =
                (topLeft + topRight + bottomRight + bottomLeft) * 0.25f;
            switch (mask)
            {
            case 1: appendSegment(left, top); break;
            case 2: appendSegment(top, right); break;
            case 3: appendSegment(left, right); break;
            case 4: appendSegment(right, bottom); break;
            case 5:
                if (centerCoverage >= threshold)
                {
                    appendSegment(top, right);
                    appendSegment(bottom, left);
                }
                else
                {
                    appendSegment(left, top);
                    appendSegment(right, bottom);
                }
                break;
            case 6: appendSegment(top, bottom); break;
            case 7: appendSegment(left, bottom); break;
            case 8: appendSegment(bottom, left); break;
            case 9: appendSegment(top, bottom); break;
            case 10:
                if (centerCoverage >= threshold)
                {
                    appendSegment(left, top);
                    appendSegment(right, bottom);
                }
                else
                {
                    appendSegment(top, right);
                    appendSegment(bottom, left);
                }
                break;
            case 11: appendSegment(right, bottom); break;
            case 12: appendSegment(left, right); break;
            case 13: appendSegment(top, right); break;
            case 14: appendSegment(left, top); break;
            default: break;
            }
        }
    }
    return result;
}
} // namespace detail

inline std::vector<OverviewCoastSegment> buildOverviewCoastSegments(const MapModel &map)
{
    return detail::buildOverviewCoastSegmentsCached(map, nullptr);
}

} // namespace gm::map_render
