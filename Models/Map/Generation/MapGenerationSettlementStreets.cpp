#include "Models/Map/MapGeneration.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <queue>
#include <random>
#include <string>
#include <utility>
#include <vector>
#include "Models/Map/MapCaveCompositionV12.hpp"
#include "Models/Map/MapDungeonCompositionV12.hpp"
#include "Models/Map/MapTileSemantics.hpp"
#include "Models/Map/MapMountainPassRouting.hpp"
#include "Models/Map/MapRoadClearance.hpp"
#include "Models/Map/MapRegionComposition.hpp"
#include "Models/Map/MapWorldLandform.hpp"
#include "Models/Map/Generation/MapGenerationSettlementBuilder.hpp"

void MapGenerationSettlementBuilder::addLineFeature(const std::string &id, const std::string &name,
                                                    MapGeneratedFeatureKind kind, const std::string &material,
                                                    float width, std::vector<MapFeaturePoint> points, bool closed)
{
    MapGeneratedFeature feature;
    feature.id = mapGenV2FeatureId(options.seed, id.c_str(), static_cast<int>(map.generation.graph.features.size()));
    feature.kind = kind;
    feature.name = name;
    feature.materialId = material;
    feature.width = width;
    feature.closed = closed;
    feature.points = std::move(points);
    map.generation.graph.features.push_back(std::move(feature));
}

void MapGenerationSettlementBuilder::addStreet(const std::string &name, MapWayKind way,
                                               std::vector<MapFeaturePoint> points, bool closed)
{
    if (points.size() < 2)
        return;
    for (std::size_t index = 1; index < points.size(); ++index)
        mapGenDrawWay(map, static_cast<int>(points[index - 1].x), static_cast<int>(points[index - 1].y),
                      static_cast<int>(points[index].x), static_cast<int>(points[index].y), way, rng, 0, false);
    if (closed && (points.front().x != points.back().x || points.front().y != points.back().y))
    {
        mapGenDrawWay(map, static_cast<int>(points.back().x), static_cast<int>(points.back().y),
                      static_cast<int>(points.front().x), static_cast<int>(points.front().y), way, rng, 0, false);
        points.push_back(points.front());
    }
    addLineFeature("street", name, MapGeneratedFeatureKind::Road,
                   way == MapWayKind::StoneRoad ? "fantasy_neutral.road.stone" : "fantasy_neutral.road.dirt",
                   way == MapWayKind::StoneRoad ? 1.08f : 0.72f, std::move(points), closed);
}

void MapGenerationSettlementBuilder::planStreetsAndCrossings()
{
    mapGenReportProgress(options, 3, totalSteps, "Planning main streets");
    westGateX = std::max(1, centerX - radiusX);
    eastGateX = std::min(map.width - 2, centerX + radiusX);
    northGateY = std::max(1, centerY - radiusY);
    southGateY = std::min(map.height - 2, centerY + radiusY);
    const MapFeaturePoint west{static_cast<float>(westGateX) + 0.5f, static_cast<float>(centerY) + 0.5f};
    const MapFeaturePoint east{static_cast<float>(eastGateX) + 0.5f, static_cast<float>(centerY) + 0.5f};
    const MapFeaturePoint north{static_cast<float>(centerX) + 0.5f, static_cast<float>(northGateY) + 0.5f};
    const MapFeaturePoint south{static_cast<float>(centerX) + 0.5f, static_cast<float>(southGateY) + 0.5f};
    const MapFeaturePoint center{static_cast<float>(centerX) + 0.5f, static_cast<float>(centerY) + 0.5f};

    std::vector<MapFeaturePoint> ring;
    constexpr int ringSteps = 20;
    const float ringScale = settlementArchetype == 4 ? 0.82f : 0.66f;
    const float ringPhase = mapGenV2Hash01(options.seed + 4973u, 3, 9) * 6.28318530718f;
    for (int step = 0; step < ringSteps; ++step)
    {
        const float angle = static_cast<float>(step) / static_cast<float>(ringSteps) * 6.28318530718f;
        float wobble = 1.0f;
        if (settlementArchetype == 0)
            wobble = 0.82f + mapGenV2Hash01(options.seed + 5003u, step, 7) * 0.28f;
        else if (settlementArchetype == 1 || settlementArchetype == 4)
        {
            // Radial towns and walls still need a legible loop, but a perfect
            // mathematical ellipse reads like a diagram rather than a place.
            // Low-frequency seeded deformation keeps gates predictable while
            // giving each plan a recognisable, hand-laid silhouette.
            wobble = 0.97f + std::sin(angle * 3.0f + ringPhase) * 0.055f +
                     std::sin(angle * 5.0f - ringPhase * 0.63f) * 0.028f;
        }
        ring.push_back({centerX + std::cos(angle) * radiusX * ringScale * wobble + 0.5f,
                        centerY + std::sin(angle) * radiusY * ringScale * wobble + 0.5f});
    }

    if (settlementArchetype == 0) // organic: bent spine and irregular branches
    {
        addStreet("Old Road", MapWayKind::StoneRoad,
                  {west,
                   {center.x - radiusX * 0.18f, center.y + radiusY * 0.12f},
                   center,
                   {center.x + radiusX * 0.22f, center.y - radiusY * 0.15f},
                   east});
        addStreet("Temple Lane", MapWayKind::DirtRoad,
                  {north, {center.x - radiusX * 0.12f, center.y - radiusY * 0.16f}, center});
        addStreet("Mill Lane", MapWayKind::DirtRoad,
                  {center, {center.x + radiusX * 0.18f, center.y + radiusY * 0.20f}, south});
    }
    else if (settlementArchetype == 1) // radial
    {
        const float bendX = (mapGenV2Hash01(options.seed + 5171u, 1, 2) - 0.5f) * radiusX * 0.16f;
        const float bendY = (mapGenV2Hash01(options.seed + 5179u, 2, 1) - 0.5f) * radiusY * 0.18f;
        addStreet("West Gate Road", MapWayKind::StoneRoad,
                  {west, {center.x - radiusX * 0.34f, center.y + bendY}, center});
        addStreet("East Gate Road", MapWayKind::StoneRoad,
                  {center, {center.x + radiusX * 0.33f, center.y - bendY * 0.62f}, east});
        addStreet("North Gate Road", MapWayKind::DirtRoad,
                  {north, {center.x + bendX, center.y - radiusY * 0.34f}, center});
        addStreet("South Gate Road", MapWayKind::DirtRoad,
                  {center, {center.x - bendX * 0.65f, center.y + radiusY * 0.35f}, south});
        addStreet("Inner Ring", MapWayKind::DirtRoad, ring, true);
    }
    else if (settlementArchetype == 2) // river city
    {
        const int riverX = centerX + std::max(2, radiusX / 5);
        std::vector<MapCellCoordinate> cityRiver;
        // Regional water cannot begin and end at the town boundary.  Carry the
        // channel through both map edges so the preview has a readable outlet
        // and the hydrology gate evaluates the same geometry users see.
        const float riverPhase = mapGenV2Hash01(options.seed + 5281u, 3, 5) * 6.28318530718f;
        for (int y = 0; y < map.height; ++y)
        {
            const float meander =
                std::sin(y * 0.18f + riverPhase) * 1.45f + std::sin(y * 0.065f - riverPhase * 0.72f) * 0.85f;
            const int x = std::clamp(riverX + static_cast<int>(std::round(meander)), 1, map.width - 2);
            mapGenSetRiverCell(map, x, y);
            cityRiver.push_back({x, y});
        }
        mapGenV2AddPathFeature(map, MapGeneratedFeatureKind::River, mapGenV2FeatureId(options.seed, "city_river", 0),
                               "City River", "fantasy_neutral.water.river", 1.15f, cityRiver);
        addStreet("River Road", MapWayKind::StoneRoad,
                  {{static_cast<float>(riverX - 3), north.y}, {static_cast<float>(riverX - 3), south.y}});
        addStreet("Bridge Street", MapWayKind::StoneRoad, {west, center, east});
        addStreet("South Bridge Road", MapWayKind::DirtRoad,
                  {{west.x, center.y + radiusY * 0.42f}, {east.x, center.y + radiusY * 0.42f}});
    }
    else if (settlementArchetype == 3) // coastal
    {
        const int coastX = centerX + radiusX / 2;
        for (int y = 0; y < map.height; ++y)
            for (int x = coastX; x < map.width; ++x)
                mapGenSetCell(map, x, y, MapTileKind::Water);
        addStreet("Harbor Road", MapWayKind::StoneRoad, {west, {static_cast<float>(coastX - 2), center.y}});
        addStreet("Quayside", MapWayKind::StoneRoad,
                  {{static_cast<float>(coastX - 2), north.y}, {static_cast<float>(coastX - 2), south.y}});
        addStreet("Market Lane", MapWayKind::DirtRoad, {north, center, south});
    }
    else if (settlementArchetype == 4) // fortified
    {
        addStreet("Wall Road", MapWayKind::StoneRoad, ring, true);
        const float marketBend = (mapGenV2Hash01(options.seed + 5333u, 4, 1) - 0.5f) * radiusY * 0.18f;
        const float gateBend = (mapGenV2Hash01(options.seed + 5347u, 1, 4) - 0.5f) * radiusX * 0.16f;
        addStreet("Market Road", MapWayKind::StoneRoad,
                  {west,
                   {center.x - radiusX * 0.28f, center.y + marketBend},
                   center,
                   {center.x + radiusX * 0.31f, center.y - marketBend * 0.55f},
                   east});
        addStreet("Gate Road", MapWayKind::DirtRoad,
                  {north,
                   {center.x + gateBend, center.y - radiusY * 0.30f},
                   center,
                   {center.x - gateBend * 0.60f, center.y + radiusY * 0.32f},
                   south});
    }
    else // planned grid
    {
        for (int lane = -1; lane <= 1; ++lane)
        {
            const float y = center.y + lane * radiusY * 0.34f;
            addStreet("East-West Street " + std::to_string(lane + 2),
                      lane == 0 ? MapWayKind::StoneRoad : MapWayKind::DirtRoad, {{west.x, y}, {east.x, y}});
            const float x = center.x + lane * radiusX * 0.34f;
            addStreet("North-South Street " + std::to_string(lane + 2),
                      lane == 0 ? MapWayKind::StoneRoad : MapWayKind::DirtRoad, {{x, north.y}, {x, south.y}});
        }
    }

    struct SettlementApproach
    {
        const char *name;
        MapFeaturePoint edge;
        MapFeaturePoint gate;
    };
    const std::array<SettlementApproach, 4> approaches = {
        {{"West Gate Road", {0.5f, west.y}, west},
         {"East Gate Road", {static_cast<float>(map.width) - 0.5f, east.y}, east},
         {"North Gate Road", {north.x, 0.5f}, north},
         {"South Gate Road", {south.x, static_cast<float>(map.height) - 0.5f}, south}}};
    int approachesAdded = 0;
    for (int approachIndex :
         (settlementArchetype == 3 ? std::array<int, 4>{0, 2, 3, 1} : std::array<int, 4>{0, 1, 2, 3}))
    {
        if (approachesAdded >= map.generation.recipe.edgeConnectionCount ||
            (settlementArchetype == 3 && approachIndex == 1))
            continue;
        const SettlementApproach &approach = approaches[static_cast<std::size_t>(approachIndex)];
        MapFeaturePoint midpoint{approach.edge.x + (approach.gate.x - approach.edge.x) * 0.56f,
                                 approach.edge.y + (approach.gate.y - approach.edge.y) * 0.56f};
        const float drift = (mapGenV2Hash01(options.seed + 5413u, approachIndex, 6) - 0.5f) * 2.8f;
        if (approachIndex < 2)
            midpoint.y += drift;
        else
            midpoint.x += drift;
        addStreet(approach.name, MapWayKind::DirtRoad, {approach.edge, midpoint, approach.gate});
        ++approachesAdded;
    }

    // Publish the city-river crossings as real graph features.  The semantic
    // raster already contained Bridge cells, but without graph geometry the
    // shared renderer could only show an unexplained road over blue water.
    int cityBridgeIndex = 0;
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const MapCellModel *bridgeCell = mapCellAt(map, x, y);
            if (!bridgeCell || bridgeCell->kind != MapTileKind::Bridge || !mapGenWayKindIsRoadLike(bridgeCell->wayKind))
                continue;
            const bool groupedWithEarlierCell =
                (mapCellAt(map, x - 1, y) && mapCellAt(map, x - 1, y)->kind == MapTileKind::Bridge) ||
                (mapCellAt(map, x, y - 1) && mapCellAt(map, x, y - 1)->kind == MapTileKind::Bridge);
            if (groupedWithEarlierCell)
                continue;
            float roadX = 1.0f;
            float roadY = 0.0f;
            const float centreX = static_cast<float>(x) + 0.5f;
            const float centreY = static_cast<float>(y) + 0.5f;
            if (!mapGenV2NearestFeatureTangent(map.generation.graph, MapGeneratedFeatureKind::Road, centreX, centreY,
                                               2.5f, roadX, roadY))
                mapGenV2NearestFeatureTangent(map.generation.graph, MapGeneratedFeatureKind::Trail, centreX, centreY,
                                              2.5f, roadX, roadY);
            MapGeneratedFeature bridge;
            bridge.id = mapGenV2FeatureId(options.seed, "city_bridge", cityBridgeIndex++);
            bridge.kind = MapGeneratedFeatureKind::Bridge;
            bridge.name = "River crossing";
            bridge.materialId = bridgeCell->wayKind == MapWayKind::StoneRoad ? "fantasy_neutral.bridge.stone"
                                                                             : "fantasy_neutral.bridge.wood";
            bridge.width = 1.0f;
            bridge.points = {{centreX - roadX * 0.72f, centreY - roadY * 0.72f},
                             {centreX + roadX * 0.72f, centreY + roadY * 0.72f}};
            map.generation.graph.features.push_back(std::move(bridge));
        }
    }
}

