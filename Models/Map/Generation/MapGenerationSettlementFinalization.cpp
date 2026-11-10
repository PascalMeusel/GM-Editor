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

void MapGenerationSettlementBuilder::finalizeMap()
{
    MapGeneratedFeature settlement;
    settlement.id = mapGenV2FeatureId(options.seed, "settlement", 0);
    settlement.kind = MapGeneratedFeatureKind::Settlement;
    settlement.name = "Settlement Center";
    for (const MapGenerationConstraint &constraint : options.constraints)
        if (constraint.featureKind == MapGeneratedFeatureKind::Settlement && !constraint.label.empty())
            settlement.name = constraint.label;
    settlement.materialId = "fantasy_neutral.settlement.city";
    settlement.locked = true;
    settlement.points.push_back({static_cast<float>(centerX) + 0.5f, static_cast<float>(centerY) + 0.5f});
    map.generation.graph.features.push_back(settlement);
    mapGenSetCell(map, centerX, centerY, MapTileKind::Settlement);

    mapGenReportProgress(options, 5, totalSteps, "Adding gates and landmarks");
    std::vector<MapCellCoordinate> gates = {
        {westGateX, centerY}, {eastGateX, centerY}, {centerX, northGateY}, {centerX, southGateY}};
    if (settlementArchetype == 3)
        gates.erase(gates.begin() + 1);
    for (const MapCellCoordinate gate : gates)
    {
        mapGenSetCell(map, gate.x, gate.y, MapTileKind::Door);
        MapFeatureModel feature;
        feature.kind = MapFeatureKind::Door;
        feature.name = "City Gate";
        feature.x = gate.x;
        feature.y = gate.y;
        map.features.push_back(std::move(feature));
    }
    for (int landmark = 0; landmark < std::min(4, map.generation.recipe.landmarkCount); ++landmark)
    {
        const int sx = landmark % 2 == 0 ? -1 : 1;
        const int sy = landmark < 2 ? -1 : 1;
        const int x = std::clamp(centerX + sx * std::max(3, radiusX / 3), 1, map.width - 2);
        const int y = std::clamp(centerY + sy * std::max(3, radiusY / 3), 1, map.height - 2);
        mapGenSetCell(map, x, y, MapTileKind::Landmark);
        addLineFeature("landmark", "District Landmark " + std::to_string(landmark + 1),
                       MapGeneratedFeatureKind::Landmark, "fantasy_neutral.landmark", 1.0f, {{x + 0.5f, y + 0.5f}});
    }

    MapRouteModel route;
    route.name = "Main Gate Road";
    route.fromMapId = map.metadata.id;
    route.startX = westGateX;
    route.startY = centerY;
    route.endX = settlementArchetype == 3 ? centerX + radiusX / 2 - 2 : eastGateX;
    route.endY = centerY;
    route.distanceMiles = std::max(1.0f, static_cast<float>(eastGateX - westGateX) * 0.05f);
    route.terrain = "Stone road";
    map.routes.push_back(std::move(route));
    // Remove one-cell ground flecks left between roads and building footprints.
    // They carry no semantic value and are especially visible with painterly
    // material blending.
    std::vector<MapCellCoordinate> isolatedGround;
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell || (cell->kind != MapTileKind::Dirt && cell->kind != MapTileKind::Forest))
                continue;
            bool neighbor = false;
            for (int oy = -1; oy <= 1 && !neighbor; ++oy)
                for (int ox = -1; ox <= 1; ++ox)
                    if ((ox != 0 || oy != 0) && mapCellAt(map, x + ox, y + oy) &&
                        mapCellAt(map, x + ox, y + oy)->kind == cell->kind)
                    {
                        neighbor = true;
                        break;
                    }
            if (!neighbor)
                isolatedGround.push_back({x, y});
        }
    }
    for (const MapCellCoordinate &coordinate : isolatedGround)
        mapGenSetCell(map, coordinate.x, coordinate.y, MapTileKind::Empty);
    map.description =
        "Editable settlement site with districts, gates, connected streets, landmarks, and accessible building blocks.";
    if (options.generatorVersion >= 10)
        mapGenV10ComposeSettlement(map, options);
    mapGenReportProgress(options, totalSteps - 1, totalSteps, "Checking settlement access");
}

