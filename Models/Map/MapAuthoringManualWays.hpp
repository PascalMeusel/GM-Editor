#pragma once

#include "Models/Map/MapAuthoringRaster.hpp"

struct MapManualWaySegment
{
    int startX = 0;
    int startY = 0;
    int endX = 0;
    int endY = 0;
    MapWayKind wayKind = MapWayKind::None;
};

inline bool operator==(const MapManualWaySegment &left, const MapManualWaySegment &right)
{
    return left.startX == right.startX && left.startY == right.startY &&
           left.endX == right.endX && left.endY == right.endY &&
           left.wayKind == right.wayKind;
}

inline bool mapManualWayFeatureMatchesSegment(const MapGeneratedFeature &feature,
                                              const MapManualWaySegment &segment)
{
    if (feature.points.size() != 2 || mapGeneratedFeatureWayKind(feature) != segment.wayKind)
        return false;
    const auto matches = [](const MapFeaturePoint &point, int x, int y) {
        return std::abs(point.x - (static_cast<float>(x) + 0.5f)) <= 0.001f &&
               std::abs(point.y - (static_cast<float>(y) + 0.5f)) <= 0.001f;
    };
    return (matches(feature.points[0], segment.startX, segment.startY) &&
            matches(feature.points[1], segment.endX, segment.endY)) ||
           (matches(feature.points[1], segment.startX, segment.startY) &&
            matches(feature.points[0], segment.endX, segment.endY));
}

inline void rebuildContinuousMapWayFeaturesFromCells(MapModel &map)
{
    if (!map.generation.continuousRendering)
        return;

    auto featureKind = [](MapWayKind wayKind) {
        if (wayKind == MapWayKind::River || wayKind == MapWayKind::Canal)
            return MapGeneratedFeatureKind::River;
        if (wayKind == MapWayKind::Trail)
            return MapGeneratedFeatureKind::Trail;
        return MapGeneratedFeatureKind::Road;
    };
    auto material = [](MapWayKind wayKind) {
        if (wayKind == MapWayKind::River || wayKind == MapWayKind::Canal)
            return std::string("fantasy_neutral.water.river");
        if (wayKind == MapWayKind::StoneRoad)
            return std::string("fantasy_neutral.road.stone");
        if (wayKind == MapWayKind::Trail)
            return std::string("fantasy_neutral.road.trail");
        return std::string("fantasy_neutral.road.dirt");
    };

    struct OwnedWayCell
    {
        MapCellCoordinate coordinate;
        MapWayKind wayKind = MapWayKind::None;
    };
    std::vector<OwnedWayCell> graphOwnedCells;
    std::vector<MapCellCoordinate> graphOwnedBridgeCells;
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if (!mapGeneratedFeatureIsWayOrBridge(feature) ||
            (mapGeneratedFeatureIsGridDerivedManualWay(feature) &&
             !mapGeneratedFeatureIsLocked(map, feature)))
            continue;
        const std::vector<MapCellCoordinate> footprint =
            mapGeneratedFeatureRasterCells(map, feature);
        if (feature.kind == MapGeneratedFeatureKind::Bridge)
        {
            graphOwnedBridgeCells.insert(graphOwnedBridgeCells.end(), footprint.begin(), footprint.end());
            continue;
        }
        const MapWayKind wayKind = mapGeneratedFeatureWayKind(feature);
        for (const MapCellCoordinate &coordinate : footprint)
            graphOwnedCells.push_back({coordinate, wayKind});
    }
    const auto graphOwnsWayCell = [&](int x, int y, MapWayKind wayKind) {
        return std::any_of(graphOwnedCells.begin(), graphOwnedCells.end(),
                           [&](const OwnedWayCell &owned) {
                               return owned.wayKind == wayKind && owned.coordinate.x == x &&
                                      owned.coordinate.y == y;
                           });
    };
    const auto graphOwnsBridgeCell = [&](int x, int y) {
        return std::any_of(graphOwnedBridgeCells.begin(), graphOwnedBridgeCells.end(),
                           [&](const MapCellCoordinate &owned) {
                               return owned.x == x && owned.y == y;
                           });
    };

    const MapCellCoordinate forwardNeighbors[] = {{1, 0}, {0, 1}, {1, 1}, {-1, 1}};
    std::vector<MapManualWaySegment> desiredSegments;
    std::vector<MapCellCoordinate> desiredBridges;
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell || cell->wayKind == MapWayKind::None)
                continue;
            for (const MapCellCoordinate &offset : forwardNeighbors)
            {
                const MapCellModel *neighbor = mapCellAt(map, x + offset.x, y + offset.y);
                if (!neighbor || neighbor->wayKind != cell->wayKind)
                    continue;
                if (graphOwnsWayCell(x, y, cell->wayKind) &&
                    graphOwnsWayCell(x + offset.x, y + offset.y, cell->wayKind))
                    continue;
                desiredSegments.push_back(
                    {x, y, x + offset.x, y + offset.y, cell->wayKind});
            }
            if (cell->kind == MapTileKind::Bridge &&
                (cell->wayKind == MapWayKind::Trail ||
                 cell->wayKind == MapWayKind::DirtRoad ||
                 cell->wayKind == MapWayKind::StoneRoad) &&
                !graphOwnsBridgeCell(x, y))
                desiredBridges.push_back({x, y});
        }
    }

    std::vector<bool> segmentUsed(desiredSegments.size(), false);
    std::vector<bool> bridgeUsed(desiredBridges.size(), false);
    std::vector<MapGeneratedFeature> reconciled;
    reconciled.reserve(map.generation.graph.features.size() + desiredSegments.size() +
                       desiredBridges.size());
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if (!mapGeneratedFeatureIsGridDerivedManualWay(feature) ||
            mapGeneratedFeatureIsLocked(map, feature))
        {
            reconciled.push_back(feature);
            continue;
        }

        bool retained = false;
        if (feature.kind == MapGeneratedFeatureKind::Bridge)
        {
            const std::vector<MapCellCoordinate> footprint =
                mapGeneratedFeatureRasterCells(map, feature);
            for (std::size_t index = 0; index < desiredBridges.size() && !retained; ++index)
            {
                if (!bridgeUsed[index] &&
                    std::find(footprint.begin(), footprint.end(), desiredBridges[index]) !=
                        footprint.end())
                {
                    bridgeUsed[index] = true;
                    retained = true;
                }
            }
        }
        else
        {
            for (std::size_t index = 0; index < desiredSegments.size(); ++index)
            {
                if (!segmentUsed[index] &&
                    mapManualWayFeatureMatchesSegment(feature, desiredSegments[index]))
                {
                    segmentUsed[index] = true;
                    retained = true;
                    break;
                }
            }
        }
        if (retained)
            reconciled.push_back(feature);
    }

    const auto uniqueId = [&](const std::string &base) {
        if (std::none_of(reconciled.begin(), reconciled.end(), [&](const MapGeneratedFeature &feature) {
                return feature.id == base;
            }))
            return base;
        for (int suffix = 2;; ++suffix)
        {
            const std::string candidate = base + "__" + std::to_string(suffix);
            if (std::none_of(reconciled.begin(), reconciled.end(),
                             [&](const MapGeneratedFeature &feature) {
                                 return feature.id == candidate;
                             }))
                return candidate;
        }
    };
    for (std::size_t index = 0; index < desiredSegments.size(); ++index)
    {
        if (segmentUsed[index])
            continue;
        const MapManualWaySegment &segment = desiredSegments[index];
        MapGeneratedFeature feature;
        const std::string idBase = "manual_way_" +
            std::to_string(static_cast<int>(segment.wayKind)) + "_" +
            std::to_string(segment.startX) + "_" + std::to_string(segment.startY) + "_" +
            std::to_string(segment.endX) + "_" + std::to_string(segment.endY);
        feature.id = uniqueId(idBase);
        feature.kind = featureKind(segment.wayKind);
        feature.name = "Edited way";
        feature.materialId = material(segment.wayKind);
        feature.width = segment.wayKind == MapWayKind::River ||
                                segment.wayKind == MapWayKind::Canal
                            ? 1.0f
                            : (segment.wayKind == MapWayKind::Trail ? 0.55f : 0.82f);
        feature.points = {
            {static_cast<float>(segment.startX) + 0.5f,
             static_cast<float>(segment.startY) + 0.5f},
            {static_cast<float>(segment.endX) + 0.5f,
             static_cast<float>(segment.endY) + 0.5f}};
        reconciled.push_back(std::move(feature));
    }
    for (std::size_t index = 0; index < desiredBridges.size(); ++index)
    {
        if (bridgeUsed[index])
            continue;
        const int x = desiredBridges[index].x;
        const int y = desiredBridges[index].y;
        const MapCellModel *cell = mapCellAt(map, x, y);
        if (!cell)
            continue;
        const MapCellModel *east = mapCellAt(map, x + 1, y);
        const MapCellModel *west = mapCellAt(map, x - 1, y);
        const bool horizontal = (east && east->wayKind == cell->wayKind) ||
                                (west && west->wayKind == cell->wayKind);
        MapGeneratedFeature bridge;
        bridge.id = uniqueId("manual_bridge_" + std::to_string(x) + "_" +
                             std::to_string(y));
        bridge.kind = MapGeneratedFeatureKind::Bridge;
        bridge.name = "Edited crossing";
        bridge.materialId = cell->wayKind == MapWayKind::StoneRoad
                                ? "fantasy_neutral.bridge.stone"
                                : "fantasy_neutral.bridge.wood";
        bridge.points = {
            {static_cast<float>(x) + (horizontal ? 0.05f : 0.5f),
             static_cast<float>(y) + (horizontal ? 0.5f : 0.05f)},
            {static_cast<float>(x) + (horizontal ? 0.95f : 0.5f),
             static_cast<float>(y) + (horizontal ? 0.5f : 0.95f)}};
        reconciled.push_back(std::move(bridge));
    }
    map.generation.graph.features = std::move(reconciled);
}

