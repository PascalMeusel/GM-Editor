#pragma once

#include "Models/Map/MapAuthoring.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

struct MapGenerationSelection
{
    bool useArea = false;
    MapNormalizedRect normalizedArea;
    std::vector<MapGeneratedFeatureKind> featureKinds;
    // Generated features and their semantic cell projection are one change.
    // These switches let a caller update only the selected family while all
    // authored/session cell state (fog, movement, LOS and lighting) survives.
    bool replaceTerrainProjection = false;
    bool replaceWayProjection = false;
    bool replacePlaceProjection = false;
};

// A partial generation patch is allowed to replace only the generated content
// described by MapGenerationSelection. Everything listed here is a positive,
// inspectable preservation promise rather than an implicit side effect of
// starting from a copy of the old map.
enum class MapGenerationPreservedContent
{
    MapIdentity,
    MapPresentation,
    MapClassificationAndGrid,
    PlayerRelease,
    AtlasLineage,
    ImportedReference,
    GenerationRecipe,
    Layers,
    Tokens,
    Regions,
    Annotations,
    Routes,
    Fog,
    TacticalCellState
};

struct MapGenerationPreservationSet
{
    // Whole-map collections and contracts that must remain unchanged. The
    // vectors below additionally make every preserved identity/cell explicit
    // for diff UIs, tests and transactional application.
    std::vector<MapGenerationPreservedContent> completeContents;
    std::vector<std::size_t> byteIdenticalCellIndices;
    std::vector<std::size_t> authoredCellStateIndices;
    std::vector<std::string> generatedFeatureIds;
    std::vector<EntityId> layerIds;
    std::vector<EntityId> tokenIds;
    std::vector<EntityId> regionIds;
    std::vector<EntityId> annotationIds;
    std::vector<EntityId> routeIds;
};

struct MapGenerationPatch
{
    bool valid = false;
    std::string errorCode;
    EntityId targetMapId;
    std::uint64_t sourceRevision = 0;
    MapGenerationSelection impact;
    MapGenerationPreservationSet preservation;
    bool preservationVerified = false;
    MapModel before;
    MapModel after;
    std::vector<std::string> removedFeatureIds;
    std::vector<std::string> addedFeatureIds;
    std::vector<std::string> preservedLockedFeatureIds;
};

// Compatibility spelling for the existing controller while MapGenerationPatch
// remains the single implementation and public transaction type.
using MapGenerationPatchPreview = MapGenerationPatch;

inline bool mapGenerationSelectionContains(const MapGenerationSelection &selection,
                                           const MapModel &map,
                                           const MapFeaturePoint &point)
{
    if (!selection.useArea)
        return true;
    const float left = selection.normalizedArea.x * static_cast<float>(map.width);
    const float top = selection.normalizedArea.y * static_cast<float>(map.height);
    const float right = (selection.normalizedArea.x + selection.normalizedArea.width) * static_cast<float>(map.width);
    const float bottom = (selection.normalizedArea.y + selection.normalizedArea.height) * static_cast<float>(map.height);
    return point.x >= left && point.x <= right && point.y >= top && point.y <= bottom;
}

inline bool mapGenerationSelectionMatchesKind(const MapGenerationSelection &selection, MapGeneratedFeatureKind kind)
{
    return selection.featureKinds.empty() ||
           std::find(selection.featureKinds.begin(), selection.featureKinds.end(), kind) != selection.featureKinds.end();
}

inline bool mapGenerationFeatureIsLinear(MapGeneratedFeatureKind kind)
{
    return kind == MapGeneratedFeatureKind::River || kind == MapGeneratedFeatureKind::Road ||
           kind == MapGeneratedFeatureKind::Trail || kind == MapGeneratedFeatureKind::Coastline;
}

struct MapGenerationSelectionBounds
{
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};

inline MapGenerationSelectionBounds mapGenerationSelectionBounds(const MapGenerationSelection &selection,
                                                                 const MapModel &map)
{
    return {
        selection.normalizedArea.x * static_cast<float>(map.width),
        selection.normalizedArea.y * static_cast<float>(map.height),
        (selection.normalizedArea.x + selection.normalizedArea.width) * static_cast<float>(map.width),
        (selection.normalizedArea.y + selection.normalizedArea.height) * static_cast<float>(map.height)};
}

inline bool mapGenerationPointInsideBounds(const MapFeaturePoint &point,
                                           const MapGenerationSelectionBounds &bounds)
{
    return point.x >= bounds.left && point.x <= bounds.right &&
           point.y >= bounds.top && point.y <= bounds.bottom;
}

inline bool mapGenerationPointInsideClosedFeature(const MapFeaturePoint &point,
                                                  const MapGeneratedFeature &feature)
{
    if (!feature.closed || feature.points.size() < 3)
        return false;
    bool inside = false;
    for (std::size_t current = 0, previous = feature.points.size() - 1;
         current < feature.points.size(); previous = current++)
    {
        const MapFeaturePoint &a = feature.points[current];
        const MapFeaturePoint &b = feature.points[previous];
        const float dy = b.y - a.y;
        const bool crosses = ((a.y > point.y) != (b.y > point.y)) &&
                             (point.x < (b.x - a.x) * (point.y - a.y) /
                                                (std::fabs(dy) <= std::numeric_limits<float>::epsilon()
                                                     ? 0.000001f
                                                     : dy) +
                                            a.x);
        if (crosses)
            inside = !inside;
    }
    return inside;
}

// Liang-Barsky clipping keeps the boundary point deterministic and avoids
// rasterizing a curve merely to discover whether it crosses the selected area.
inline bool mapGenerationClipSegmentToBounds(const MapFeaturePoint &start,
                                             const MapFeaturePoint &end,
                                             const MapGenerationSelectionBounds &bounds,
                                             MapFeaturePoint &clippedStart,
                                             MapFeaturePoint &clippedEnd)
{
    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    float minimum = 0.0f;
    float maximum = 1.0f;
    const auto clip = [&](float denominator, float numerator) {
        if (std::fabs(denominator) <= std::numeric_limits<float>::epsilon())
            return numerator >= 0.0f;
        const float ratio = numerator / denominator;
        if (denominator < 0.0f)
        {
            if (ratio > maximum) return false;
            minimum = std::max(minimum, ratio);
        }
        else
        {
            if (ratio < minimum) return false;
            maximum = std::min(maximum, ratio);
        }
        return true;
    };
    if (!clip(-dx, start.x - bounds.left) || !clip(dx, bounds.right - start.x) ||
        !clip(-dy, start.y - bounds.top) || !clip(dy, bounds.bottom - start.y) ||
        minimum > maximum)
        return false;
    clippedStart = {start.x + minimum * dx, start.y + minimum * dy};
    clippedEnd = {start.x + maximum * dx, start.y + maximum * dy};
    return true;
}

inline bool mapGenerationPointsEqual(const MapFeaturePoint &left, const MapFeaturePoint &right)
{
    constexpr float epsilon = 0.0001f;
    return std::fabs(left.x - right.x) <= epsilon && std::fabs(left.y - right.y) <= epsilon;
}

inline void mapGenerationAppendUniquePoint(std::vector<MapFeaturePoint> &points, const MapFeaturePoint &point)
{
    if (points.empty() || !mapGenerationPointsEqual(points.back(), point))
        points.push_back(point);
}
