#include "Models/Map/MapAtlasService.hpp"
#include "Models/Map/MapAuthoring.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

#include "Models/Map/MapAtlasServiceInternal.hpp"

namespace MapAtlasServiceInternal
{
void extractRegionsAndRoutes(MapAtlasExtractionResult &result,
                             const MapModel &parent,
                             const MapAtlasPlacement &placement,
                             int childWidth,
                             int childHeight)
{
    for (const MapRegionModel &source : parent.regions)
    {
        if (source.metadata.id.empty())
        {
            result.errors.push_back("A parent region has no stable ID.");
            continue;
        }
        const float sourceLeft = static_cast<float>(source.x) / static_cast<float>(parent.width);
        const float sourceTop = static_cast<float>(source.y) / static_cast<float>(parent.height);
        const float sourceRight = static_cast<float>(source.x + source.width) / static_cast<float>(parent.width);
        const float sourceBottom = static_cast<float>(source.y + source.height) / static_cast<float>(parent.height);
        const float left = std::max(sourceLeft, placement.parentRect.x);
        const float top = std::max(sourceTop, placement.parentRect.y);
        const float right = std::min(sourceRight, placement.parentRect.x + placement.parentRect.width);
        const float bottom = std::min(sourceBottom, placement.parentRect.y + placement.parentRect.height);
        if (right <= left || bottom <= top)
            continue;

        MapRegionModel region = source;
        const float normalizedLeft = (left - placement.parentRect.x) / placement.parentRect.width;
        const float normalizedTop = (top - placement.parentRect.y) / placement.parentRect.height;
        const float normalizedRight = (right - placement.parentRect.x) / placement.parentRect.width;
        const float normalizedBottom = (bottom - placement.parentRect.y) / placement.parentRect.height;
        region.x = std::clamp(static_cast<int>(std::floor(normalizedLeft * childWidth)), 0, childWidth - 1);
        region.y = std::clamp(static_cast<int>(std::floor(normalizedTop * childHeight)), 0, childHeight - 1);
        const int regionRight = std::clamp(static_cast<int>(std::ceil(normalizedRight * childWidth)),
                                           region.x + 1, childWidth);
        const int regionBottom = std::clamp(static_cast<int>(std::ceil(normalizedBottom * childHeight)),
                                            region.y + 1, childHeight);
        region.width = regionRight - region.x;
        region.height = regionBottom - region.y;
        const EntityId sourceId = source.metadata.id;
        region.metadata.id = sourceId + "@atlas_region";
        markAtlasDerived(region.metadata, parent, sourceId, "region");
        result.regions.push_back(std::move(region));
    }

    for (const MapRouteModel &source : parent.routes)
    {
        if (source.metadata.id.empty())
        {
            result.errors.push_back("A parent route has no stable ID.");
            continue;
        }
        const MapAtlasPoint start{
            static_cast<float>(source.startX) / static_cast<float>(parent.width),
            static_cast<float>(source.startY) / static_cast<float>(parent.height)};
        const MapAtlasPoint end{
            static_cast<float>(source.endX) / static_cast<float>(parent.width),
            static_cast<float>(source.endY) / static_cast<float>(parent.height)};
        MapAtlasPoint clippedStart;
        MapAtlasPoint clippedEnd;
        if (!clipSegmentToRect(start, end, placement.parentRect, clippedStart, clippedEnd))
            continue;
        const std::optional<MapAtlasPoint> childStart =
            MapAtlasService::parentToChildNormalized(placement, clippedStart);
        const std::optional<MapAtlasPoint> childEnd =
            MapAtlasService::parentToChildNormalized(placement, clippedEnd);
        if (!childStart || !childEnd)
            continue;

        MapRouteModel route = source;
        route.startX = std::clamp(static_cast<int>(std::lround(childStart->x * childWidth)), 0,
                                  childWidth - 1);
        route.startY = std::clamp(static_cast<int>(std::lround(childStart->y * childHeight)), 0,
                                  childHeight - 1);
        route.endX = std::clamp(static_cast<int>(std::lround(childEnd->x * childWidth)), 0,
                                childWidth - 1);
        route.endY = std::clamp(static_cast<int>(std::lround(childEnd->y * childHeight)), 0,
                                childHeight - 1);
        const float originalLength = std::hypot(end.x - start.x, end.y - start.y);
        const float clippedLength = std::hypot(clippedEnd.x - clippedStart.x,
                                               clippedEnd.y - clippedStart.y);
        if (originalLength > atlasEpsilon)
            route.distanceMiles *= clippedLength / originalLength;
        const EntityId sourceId = source.metadata.id;
        route.metadata.id = sourceId + "@atlas_route";
        markAtlasDerived(route.metadata, parent, sourceId, "route");
        result.routes.push_back(std::move(route));
    }
}

void addBoundaryConstraint(
    MapAtlasExtractionResult &result,
    const MapModel &parent,
    const MapGeneratedFeature &sourceFeature,
    const MapAtlasPlacement &placement,
    MapAtlasPoint parentPoint,
    int childWidth,
    int childHeight)
{
    const std::optional<MapAtlasBoundaryEdge> edge = boundaryEdge(placement.parentRect, parentPoint);
    if (!edge)
        return;
    const std::optional<MapAtlasPoint> childPoint = MapAtlasService::parentToChildNormalized(placement, parentPoint);
    if (!childPoint)
        return;

    MapAtlasBoundaryConstraint constraint;
    constraint.featureKind = sourceFeature.kind;
    constraint.edge = *edge;
    constraint.childPoint = normalizedToFeaturePoint(*childPoint, childWidth, childHeight);
    // This namespace marks a topological hand-off, not a point feature request.
    // Generator and viewer code can therefore keep it out of the legacy
    // landmark-placement path while still retaining it for atlas diagnostics.
    constraint.id = "atlas-edge::" + sourceFeature.id + "@" + edgeSuffix(*edge) + "_" +
                    std::to_string(static_cast<long long>(std::llround(constraint.childPoint.x * 1000.0f))) + "_" +
                    std::to_string(static_cast<long long>(std::llround(constraint.childPoint.y * 1000.0f)));
    constraint.lineage = {parent.metadata.id, sourceFeature.id, parent.revision};
    const auto duplicate = std::find_if(result.boundaryConstraints.begin(), result.boundaryConstraints.end(), [&](const MapAtlasBoundaryConstraint &candidate) {
        return candidate.id == constraint.id && samePoint(candidate.childPoint, constraint.childPoint);
    });
    if (duplicate == result.boundaryConstraints.end())
    {
        MapGenerationConstraint generationConstraint;
        generationConstraint.id = constraint.id;
        generationConstraint.label = sourceFeature.name.empty() ? constraint.id : sourceFeature.name + " boundary";
        generationConstraint.featureKind = sourceFeature.kind;
        generationConstraint.x = constraint.childPoint.x;
        generationConstraint.y = constraint.childPoint.y;
        generationConstraint.required = true;
        generationConstraint.locked = true;
        result.generationConstraints.push_back(std::move(generationConstraint));
        result.boundaryConstraints.push_back(std::move(constraint));
    }
}
}
