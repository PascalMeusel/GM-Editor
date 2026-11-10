#include "Models/Map/MapGeneration.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
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
#include "Models/Map/Generation/MapGenerationOverworldBuilder.hpp"

void MapGenerationOverworldBuilder::finalizeMap()
{
    mapGenReportProgress(options, 8, totalSteps, "Adding tactical detail");
    if (options.scale == MapLevel::Site || options.scale == MapLevel::Tactical)
        mapGenBuildTacticalFeatures(map, options, rng);
    // A requested edge connection can legitimately be unavailable on an
    // island-style world map.  Publish the session route only between nodes
    // that the road planner actually connected; otherwise the quality report
    // would describe an intentionally ocean-bound map edge as a broken road.
    std::vector<MapCellCoordinate> connectedRouteNodes;
    connectedRouteNodes.reserve(networkNodes.size());
    for (std::size_t nodeIndex = 0; nodeIndex < networkNodes.size(); ++nodeIndex)
        if (nodeIndex < connected.size() && connected[nodeIndex])
            connectedRouteNodes.push_back(networkNodes[nodeIndex]);
    if (connectedRouteNodes.size() >= 2)
    {
        MapRouteModel route;
        route.name = "Generated Route";
        route.fromMapId = map.metadata.id;
        route.startX = connectedRouteNodes.front().x;
        route.startY = connectedRouteNodes.front().y;
        route.endX = connectedRouteNodes.back().x;
        route.endY = connectedRouteNodes.back().y;
        route.distanceMiles = static_cast<float>(std::max(1, map.width / 2));
        route.terrain = mapWayKindLabel(roadKind);
        route.notes = "Primary route generated from the V2 feature graph.";
        map.routes.push_back(std::move(route));
    }
    map.description = "Editable V2 map with deterministic terrain, hydrology, settlements, and cost-aware routes.";
}

