#pragma once

#include "Models/Map/MapOverviewCoasts.hpp"

namespace gm::map_render
{
inline std::vector<OverviewMountainStamp> buildOverviewMountainStamps(
    const MapModel &map)
{
    std::vector<OverviewMountainStamp> result;
    if ((map.atlasLevel != MapLevel::World && map.atlasLevel != MapLevel::Region) ||
        map.width <= 0 || map.height <= 0)
        return result;

    // V5 World maps expose an explicit range spine. Prefer it over a PCA fit
    // of raster cells: the spline preserves the generator's intended bends
    // and lets adjacent paintings form one readable chain. Older maps and
    // Region maps continue through the raster-derived compatibility path.
    if (map.atlasLevel == MapLevel::World)
    {
        const auto ground = buildOverviewGroundProjection(map);
        const auto fitsLand = [&](const OverviewMountainStamp &stamp) {
            // The painting's measured source axis varies by asset. Its
            // bounding circle contains every rotated variant, so preserving
            // this land clearance keeps transparent-footprint tips out of sea.
            const float length = stamp.lengthCells * kOverviewMountainLengthScale;
            const float width = stamp.widthCells * kOverviewV33MountainWidthScale;
            const float radius = std::sqrt(length * length + width * width) * 0.5f + 0.55f;
            const int minX = static_cast<int>(std::floor(stamp.centerX - radius));
            const int maxX = static_cast<int>(std::ceil(stamp.centerX + radius));
            const int minY = static_cast<int>(std::floor(stamp.centerY - radius));
            const int maxY = static_cast<int>(std::ceil(stamp.centerY + radius));
            for (int y = minY; y <= maxY; ++y)
                for (int x = minX; x <= maxX; ++x)
                {
                    const float dx = x + 0.5f - stamp.centerX;
                    const float dy = y + 0.5f - stamp.centerY;
                    if (dx * dx + dy * dy > radius * radius) continue;
                    if (isMapCellInside(map, x, y) &&
                        !overviewGroundKindIsLand(overviewGroundKindFromProjection(map, ground, x, y)))
                        return false;
                }
            return true;
        };
        int rangeIndex = 0;
        for (const MapGeneratedFeature &feature : map.generation.graph.features)
        {
            if (feature.kind != MapGeneratedFeatureKind::MountainRange ||
                feature.points.size() < 2)
                continue;
            float totalLength = 0.0f;
            for (std::size_t index = 1; index < feature.points.size(); ++index)
            {
                const float deltaX = feature.points[index].x - feature.points[index - 1].x;
                const float deltaY = feature.points[index].y - feature.points[index - 1].y;
                totalLength += std::sqrt(deltaX * deltaX + deltaY * deltaY);
            }
            if (totalLength <= 0.25f)
                continue;

            const auto pointAtDistance = [&](float requestedDistance) {
                const float distance = std::clamp(requestedDistance, 0.0f, totalLength);
                float traversed = 0.0f;
                for (std::size_t index = 1; index < feature.points.size(); ++index)
                {
                    const MapFeaturePoint &start = feature.points[index - 1];
                    const MapFeaturePoint &end = feature.points[index];
                    const float deltaX = end.x - start.x;
                    const float deltaY = end.y - start.y;
                    const float segmentLength = std::sqrt(deltaX * deltaX + deltaY * deltaY);
                    if (segmentLength <= 0.0001f)
                        continue;
                    if (traversed + segmentLength >= distance)
                    {
                        const float amount = std::clamp(
                            (distance - traversed) / segmentLength, 0.0f, 1.0f);
                        return MapFeaturePoint{start.x + deltaX * amount,
                                               start.y + deltaY * amount};
                    }
                    traversed += segmentLength;
                }
                return feature.points.back();
            };

            const int sectionCount = std::max(1, static_cast<int>(std::ceil(totalLength / 10.0f)));
            const float sectionLength = totalLength / static_cast<float>(sectionCount);
            for (int sectionIndex = 0; sectionIndex < sectionCount; ++sectionIndex)
            {
                const float startDistance = sectionLength * sectionIndex;
                const float endDistance = sectionIndex + 1 == sectionCount
                                              ? totalLength
                                              : sectionLength * (sectionIndex + 1);
                const float middleDistance = (startDistance + endDistance) * 0.5f;
                const float tangentProbe = std::min(0.85f, sectionLength * 0.28f);
                const MapFeaturePoint before = pointAtDistance(middleDistance - tangentProbe);
                const MapFeaturePoint after = pointAtDistance(middleDistance + tangentProbe);
                const MapFeaturePoint center = pointAtDistance(middleDistance);
                OverviewMountainStamp stamp;
                stamp.centerX = center.x;
                stamp.centerY = center.y;
                stamp.angleRadians = std::atan2(after.y - before.y, after.x - before.x);
                // The source painting carries transparent ends. Deliberate
                // overlap makes a two-part very long range read as one chain,
                // not as several pasted fragments.
                stamp.lengthCells = std::clamp((sectionLength + 3.5f) / kOverviewMountainLengthScale,
                                               6.0f, 17.0f);
                stamp.lengthCells = std::min(stamp.lengthCells,
                    std::max(1.0f, std::min(middleDistance, totalLength - middleDistance) *
                                       2.0f / kOverviewMountainLengthScale));
                stamp.widthCells = std::clamp(
                    std::max(feature.width * 0.92f + 0.75f,
                             stamp.lengthCells / 2.45f),
                    2.8f, 6.8f);
                stamp.anchorX = std::clamp(
                    static_cast<int>(std::floor(center.x)), 0, map.width - 1);
                stamp.anchorY = std::clamp(
                    static_cast<int>(std::floor(center.y)), 0, map.height - 1);
                stamp.componentCellCount = static_cast<int>(std::lround(totalLength));
                stamp.sectionIndex = rangeIndex * 32 + sectionIndex;
                for (int attempt = 0; attempt < 10 && !fitsLand(stamp); ++attempt)
                {
                    stamp.lengthCells *= 0.87f;
                    stamp.widthCells *= 0.87f;
                }
                // Keep the range on its semantic spine. Independently moving
                // whole paintings away from a river split a continuous range
                // into unrelated fragments. Water, routes and labels render
                // above the relief and retain their own clearance policy.
                if (fitsLand(stamp)) result.push_back(stamp);
            }
            ++rangeIndex;
        }
        if (!result.empty())
            return result;
    }

    const std::size_t width = static_cast<std::size_t>(map.width);
    const std::size_t height = static_cast<std::size_t>(map.height);
    if (height > (std::numeric_limits<std::size_t>::max)() / width)
        return result;
    std::vector<std::uint8_t> visited(width * height, 0u);
    constexpr int neighbors[][2] = {
        {-1, -1}, {0, -1}, {1, -1}, {-1, 0},
        {1, 0}, {-1, 1}, {0, 1}, {1, 1}};

    const float maximumSectionSpan = map.atlasLevel == MapLevel::World ? 14.0f : 11.0f;
    const int maximumSections = map.atlasLevel == MapLevel::World ? 2 : 4;

    for (int startY = 0; startY < map.height; ++startY)
    {
        for (int startX = 0; startX < map.width; ++startX)
        {
            const std::size_t startIndex = static_cast<std::size_t>(startY) * width +
                                           static_cast<std::size_t>(startX);
            if (visited[startIndex] || !detail::isMountainCell(map, startX, startY))
                continue;

            std::queue<std::pair<int, int>> pending;
            std::vector<detail::MountainCell> component;
            pending.emplace(startX, startY);
            visited[startIndex] = 1u;
            while (!pending.empty())
            {
                const auto [x, y] = pending.front();
                pending.pop();
                component.push_back({x, y, 0.0f});
                for (const auto &offset : neighbors)
                {
                    const int neighborX = x + offset[0];
                    const int neighborY = y + offset[1];
                    if (!detail::isMountainCell(map, neighborX, neighborY))
                        continue;
                    const std::size_t neighborIndex = static_cast<std::size_t>(neighborY) * width +
                                                      static_cast<std::size_t>(neighborX);
                    if (visited[neighborIndex])
                        continue;
                    visited[neighborIndex] = 1u;
                    pending.emplace(neighborX, neighborY);
                }
            }

            // One isolated semantic mountain cell remains visible through the
            // warm relief biome, but it must not inflate into a continent-scale
            // photographic stamp.
            if (component.size() < 2u)
                continue;

            float centroidX = 0.0f;
            float centroidY = 0.0f;
            for (const detail::MountainCell &cell : component)
            {
                centroidX += static_cast<float>(cell.x) + 0.5f;
                centroidY += static_cast<float>(cell.y) + 0.5f;
            }
            centroidX /= static_cast<float>(component.size());
            centroidY /= static_cast<float>(component.size());

            float xx = 0.0f;
            float yy = 0.0f;
            float xy = 0.0f;
            for (const detail::MountainCell &cell : component)
            {
                const float dx = static_cast<float>(cell.x) + 0.5f - centroidX;
                const float dy = static_cast<float>(cell.y) + 0.5f - centroidY;
                xx += dx * dx;
                yy += dy * dy;
                xy += dx * dy;
            }
            const float angle = 0.5f * std::atan2(2.0f * xy, xx - yy);
            const float axisX = std::cos(angle);
            const float axisY = std::sin(angle);
            const float normalX = -axisY;
            const float normalY = axisX;
            float minimumAlong = (std::numeric_limits<float>::max)();
            float maximumAlong = (std::numeric_limits<float>::lowest)();
            for (detail::MountainCell &cell : component)
            {
                const float dx = static_cast<float>(cell.x) + 0.5f - centroidX;
                const float dy = static_cast<float>(cell.y) + 0.5f - centroidY;
                cell.along = dx * axisX + dy * axisY;
                minimumAlong = std::min(minimumAlong, cell.along);
                maximumAlong = std::max(maximumAlong, cell.along);
            }

            const float componentSpan = maximumAlong - minimumAlong + 1.0f;
            const int sectionCount = std::clamp(
                static_cast<int>(std::ceil(componentSpan / maximumSectionSpan)),
                1, maximumSections);
            const float sectionSpan = std::max(1.0f, componentSpan / static_cast<float>(sectionCount));

            for (int sectionIndex = 0; sectionIndex < sectionCount; ++sectionIndex)
            {
                const float sectionMinimum = minimumAlong + sectionIndex * sectionSpan;
                const float sectionMaximum = sectionIndex + 1 == sectionCount
                                                 ? maximumAlong + 0.001f
                                                 : sectionMinimum + sectionSpan;
                float sectionCenterX = 0.0f;
                float sectionCenterY = 0.0f;
                float minimumSectionAlong = (std::numeric_limits<float>::max)();
                float maximumSectionAlong = (std::numeric_limits<float>::lowest)();
                float minimumAcross = (std::numeric_limits<float>::max)();
                float maximumAcross = (std::numeric_limits<float>::lowest)();
                int cellsInSection = 0;
                for (const detail::MountainCell &cell : component)
                {
                    if (cell.along < sectionMinimum || cell.along >= sectionMaximum)
                        continue;
                    const float cellX = static_cast<float>(cell.x) + 0.5f;
                    const float cellY = static_cast<float>(cell.y) + 0.5f;
                    const float dx = cellX - centroidX;
                    const float dy = cellY - centroidY;
                    const float across = dx * normalX + dy * normalY;
                    sectionCenterX += cellX;
                    sectionCenterY += cellY;
                    minimumSectionAlong = std::min(minimumSectionAlong, cell.along);
                    maximumSectionAlong = std::max(maximumSectionAlong, cell.along);
                    minimumAcross = std::min(minimumAcross, across);
                    maximumAcross = std::max(maximumAcross, across);
                    ++cellsInSection;
                }
                if (cellsInSection == 0)
                    continue;

                sectionCenterX /= static_cast<float>(cellsInSection);
                sectionCenterY /= static_cast<float>(cellsInSection);
                const float occupiedLength = maximumSectionAlong - minimumSectionAlong + 1.0f;
                const float occupiedWidth = maximumAcross - minimumAcross + 1.0f;
                OverviewMountainStamp stamp;
                stamp.centerX = sectionCenterX;
                stamp.centerY = sectionCenterY;
                stamp.angleRadians = angle;
                stamp.lengthCells = std::clamp(occupiedLength + 2.25f, 5.5f, 15.5f);
                // Preserve the broad 3:2 macro-painting silhouette without
                // stretching a narrow range into an opaque rectangular slab.
                stamp.widthCells = std::clamp(
                    std::max(occupiedWidth + 1.5f, stamp.lengthCells / 2.15f),
                    3.2f, 8.0f);
                stamp.anchorX = std::clamp(static_cast<int>(std::floor(sectionCenterX)), 0, map.width - 1);
                stamp.anchorY = std::clamp(static_cast<int>(std::floor(sectionCenterY)), 0, map.height - 1);
                stamp.componentCellCount = static_cast<int>(component.size());
                stamp.sectionIndex = sectionIndex;
                result.push_back(stamp);
            }
        }
    }
    return result;
}
} // namespace gm::map_render
