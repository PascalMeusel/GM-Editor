#pragma once

#include "Models/Map/MapEditSelection.hpp"

inline std::vector<std::vector<MapFeaturePoint>> mapGenerationClipPolyline(
    const std::vector<MapFeaturePoint> &points,
    const MapGenerationSelectionBounds &bounds,
    bool keepInside)
{
    std::vector<std::vector<MapFeaturePoint>> pieces;
    std::vector<MapFeaturePoint> current;
    auto flush = [&]() {
        if (current.size() >= 2)
            pieces.push_back(current);
        current.clear();
    };
    if (points.size() < 2)
        return pieces;

    for (std::size_t index = 1; index < points.size(); ++index)
    {
        const MapFeaturePoint &start = points[index - 1];
        const MapFeaturePoint &end = points[index];
        MapFeaturePoint insideStart;
        MapFeaturePoint insideEnd;
        const bool intersects = mapGenerationClipSegmentToBounds(start, end, bounds, insideStart, insideEnd);
        const bool startInside = mapGenerationPointInsideBounds(start, bounds);
        const bool endInside = mapGenerationPointInsideBounds(end, bounds);

        if (keepInside)
        {
            if (!intersects)
            {
                flush();
                continue;
            }
            mapGenerationAppendUniquePoint(current, insideStart);
            mapGenerationAppendUniquePoint(current, insideEnd);
            if (!endInside)
                flush();
            continue;
        }

        if (!intersects)
        {
            mapGenerationAppendUniquePoint(current, start);
            mapGenerationAppendUniquePoint(current, end);
            continue;
        }
        if (!startInside)
        {
            mapGenerationAppendUniquePoint(current, start);
            mapGenerationAppendUniquePoint(current, insideStart);
            flush();
        }
        else
        {
            flush();
        }
        if (!endInside)
        {
            mapGenerationAppendUniquePoint(current, insideEnd);
            mapGenerationAppendUniquePoint(current, end);
        }
    }
    flush();
    return pieces;
}

inline bool mapGenerationSelectionTouchesFeature(const MapGenerationSelection &selection,
                                                 const MapModel &map,
                                                 const MapGeneratedFeature &feature)
{
    if (!mapGenerationSelectionMatchesKind(selection, feature.kind))
        return false;
    if (!selection.useArea)
        return true;
    if (std::any_of(feature.points.begin(), feature.points.end(), [&](const MapFeaturePoint &point) {
        return mapGenerationSelectionContains(selection, map, point);
    }))
        return true;
    if (feature.points.size() < 2)
        return false;
    const MapGenerationSelectionBounds bounds = mapGenerationSelectionBounds(selection, map);
    for (std::size_t index = 1; index < feature.points.size(); ++index)
    {
        MapFeaturePoint clippedStart;
        MapFeaturePoint clippedEnd;
        if (mapGenerationClipSegmentToBounds(feature.points[index - 1], feature.points[index], bounds,
                                             clippedStart, clippedEnd))
            return true;
    }
    if (feature.closed)
    {
        MapFeaturePoint clippedStart;
        MapFeaturePoint clippedEnd;
        if (mapGenerationClipSegmentToBounds(feature.points.back(), feature.points.front(), bounds,
                                             clippedStart, clippedEnd))
            return true;
        const MapFeaturePoint corners[] = {
            {bounds.left, bounds.top}, {bounds.right, bounds.top},
            {bounds.right, bounds.bottom}, {bounds.left, bounds.bottom}};
        for (const MapFeaturePoint &corner : corners)
            if (mapGenerationPointInsideClosedFeature(corner, feature))
                return true;
    }
    return false;
}

inline bool mapGenerationClosedFeatureCrossesSelection(const MapGenerationSelection &selection,
                                                       const MapModel &map,
                                                       const MapGeneratedFeature &feature)
{
    if (!selection.useArea || !feature.closed || feature.points.size() < 3 ||
        !mapGenerationSelectionMatchesKind(selection, feature.kind) ||
        !mapGenerationSelectionTouchesFeature(selection, map, feature))
        return false;
    return !std::all_of(feature.points.begin(), feature.points.end(),
                        [&](const MapFeaturePoint &point) {
                            return mapGenerationSelectionContains(selection, map, point);
                        });
}

inline std::string mapGenerationDeterministicInsertedFeatureId(
    const std::vector<MapGeneratedFeature> &features,
    const MapGeneratedFeature &candidate,
    std::uint64_t targetRevision,
    std::size_t pieceIndex)
{
    const std::string base = candidate.id.empty() ? "regenerated_feature" : candidate.id;
    const auto available = [&](const std::string &id) {
        return std::none_of(features.begin(), features.end(), [&](const MapGeneratedFeature &feature) {
            return feature.id == id;
        });
    };
    if (pieceIndex == 0 && available(base))
        return base;

    const std::string deterministicBase = base + "__regenerated_" +
                                          std::to_string(targetRevision) + "_" +
                                          std::to_string(pieceIndex + 1);
    if (available(deterministicBase))
        return deterministicBase;
    for (std::size_t collision = 2;; ++collision)
    {
        const std::string id = deterministicBase + "_" + std::to_string(collision);
        if (available(id))
            return id;
    }
}
