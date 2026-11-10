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

void MapGenerationSettlementBuilder::buildDistrictsAndBlocks()
{
    mapGenReportProgress(options, 4, totalSteps, "Building districts and blocks");
    const MapFeaturePoint center{static_cast<float>(centerX) + 0.5f, static_cast<float>(centerY) + 0.5f};
    const char *districtNames[] = {"Market Quarter", "Old Quarter", "Temple Quarter", "Craft Quarter"};
    for (int district = 0; district < 4; ++district)
    {
        const int sx = district % 2 == 0 ? -1 : 1;
        const int sy = district < 2 ? -1 : 1;
        const float cx = centerX + sx * radiusX * 0.35f;
        const float cy = centerY + sy * radiusY * 0.35f;
        const float halfWidth = radiusX * 0.27f;
        const float halfHeight = radiusY * 0.25f;
        const float cornerX =
            std::max(0.75f, halfWidth * (0.22f + mapGenV2Hash01(options.seed + 6101u, district, 1) * 0.10f));
        const float cornerY =
            std::max(0.75f, halfHeight * (0.22f + mapGenV2Hash01(options.seed + 6109u, district, 2) * 0.10f));
        addLineFeature("district", districtNames[district], MapGeneratedFeatureKind::District,
                       "fantasy_neutral.district." + std::to_string(district), 1.0f,
                       {{cx - halfWidth + cornerX, cy - halfHeight},
                        {cx + halfWidth - cornerX, cy - halfHeight},
                        {cx + halfWidth, cy - halfHeight + cornerY},
                        {cx + halfWidth, cy + halfHeight - cornerY},
                        {cx + halfWidth - cornerX, cy + halfHeight},
                        {cx - halfWidth + cornerX, cy + halfHeight},
                        {cx - halfWidth, cy + halfHeight - cornerY},
                        {cx - halfWidth, cy - halfHeight + cornerY}},
                       true);
    }

    const int settlementArea = std::max(1, map.width * map.height);
    const int baselineBuildingTarget = settlementArea / 90;
    // City density is a user-facing promise.  Previously it was ignored and
    // every 48x34 settlement stopped at roughly eighteen buildings, even when
    // the creation flow requested a dense town.  Preserve the restrained
    // twelve-building floor for low-density briefs while allowing the existing
    // thirty-stamp visual-hierarchy ceiling to be reached deliberately.
    const int densityBuildingTarget = settlementArea * mapGenClampPercent(options.settlementDensity) / 2500;
    const int buildingTarget = std::clamp(std::max(baselineBuildingTarget, densityBuildingTarget), 12, 30);
    int buildings = 0;
    std::vector<MapCellCoordinate> buildingSites;
    buildingSites.reserve(static_cast<std::size_t>(buildingTarget));
    // Three stable neighbourhood foci create readable streetscapes and quiet
    // negative space. Buildings still have to touch a road and pass footprint
    // separation, so clusters never turn into overlapping stamp piles.
    std::array<MapFeaturePoint, 3> buildingClusters = {{{center.x - radiusX * 0.38f, center.y - radiusY * 0.25f},
                                                        {center.x + radiusX * 0.30f, center.y - radiusY * 0.24f},
                                                        {center.x + radiusX * 0.08f, center.y + radiusY * 0.37f}}};
    if (settlementArchetype == 3)
    {
        buildingClusters[0].x = center.x - radiusX * 0.42f;
        buildingClusters[1].x = center.x + radiusX * 0.12f;
        buildingClusters[2].x = center.x + radiusX * 0.18f;
    }
    // Cover enough of each district for dense briefs to reach their requested
    // building count without relaxing the non-overlap rule.  The former 0.34
    // radius admitted only about seventeen sites on the standard city preset,
    // so increasing the density slider had no observable effect.
    const float clusterRadiusX = std::max(4.0f, radiusX * 0.48f);
    const float clusterRadiusY = std::max(3.5f, radiusY * 0.48f);
    for (int attempt = 0; attempt < buildingTarget * 20 && buildings < buildingTarget; ++attempt)
    {
        const int x = mapGenRandomBetween(rng, std::max(1, centerX - radiusX + 2),
                                          std::min(map.width - 2, centerX + radiusX - 2));
        const int y = mapGenRandomBetween(rng, std::max(1, centerY - radiusY + 2),
                                          std::min(map.height - 2, centerY + radiusY - 2));
        MapCellModel *cell = mapCellAt(map, x, y);
        if (!cell || cell->wayKind != MapWayKind::None || cell->kind == MapTileKind::Building ||
            cell->kind == MapTileKind::Water || cell->kind == MapTileKind::Door || std::abs(x - centerX) <= 1 ||
            std::abs(y - centerY) <= 1)
            continue;
        bool nearStreet = false;
        for (int oy = -2; oy <= 2 && !nearStreet; ++oy)
            for (int ox = -2; ox <= 2 && !nearStreet; ++ox)
                if (const MapCellModel *neighbor = mapCellAt(map, x + ox, y + oy))
                    nearStreet =
                        neighbor->wayKind == MapWayKind::DirtRoad || neighbor->wayKind == MapWayKind::StoneRoad;
        if (!nearStreet)
            continue;
        float closestCluster = std::numeric_limits<float>::max();
        for (const MapFeaturePoint &cluster : buildingClusters)
        {
            const float clusterX = (static_cast<float>(x) + 0.5f - cluster.x) / clusterRadiusX;
            const float clusterY = (static_cast<float>(y) + 0.5f - cluster.y) / clusterRadiusY;
            closestCluster = std::min(closestCluster, clusterX * clusterX + clusterY * clusterY);
        }
        if (closestCluster > 1.0f)
            continue;
        const bool crowded =
            std::any_of(buildingSites.begin(), buildingSites.end(), [&](const MapCellCoordinate &placed) {
                return std::abs(placed.x - x) <= 1 && std::abs(placed.y - y) <= 1;
            });
        if (crowded)
            continue;
        mapGenSetCell(map, x, y, MapTileKind::Building);
        buildingSites.push_back({x, y});
        ++buildings;
    }
}
