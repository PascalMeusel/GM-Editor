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

void MapGenerationOverworldBuilder::establishLakes()
{
    // Establish inland basins before tracing rivers so the drainage search can
    // terminate at a real lake instead of drawing through a lake painted later.
    if (!(options.generatorVersion >= 11 && options.landform == MapLandform::InlandLake &&
          (options.scale == MapLevel::World || options.scale == MapLevel::Region)) &&
        (options.landscape == MapLandscapeProfile::LakeCountry || options.waterLevel >= 72))
    {
        const int lakeCount = options.landscape == MapLandscapeProfile::LakeCountry ? 2 : 1;
        for (int lakeIndex = 0; lakeIndex < lakeCount; ++lakeIndex)
        {
            int radius = std::max(2, std::min(map.width, map.height) / (lakeIndex == 0 ? 7 : 9));
            int centerX = mapGenRandomBetween(rng, radius + 2, std::max(radius + 2, map.width - radius - 3));
            int centerY = mapGenRandomBetween(rng, radius + 2, std::max(radius + 2, map.height - radius - 3));
            if (options.generatorVersion >= 8)
            {
                // Find a basin wholly inland, including a dry buffer. A lake
                // painted over existing ocean used to disappear from the map.
                std::vector<int> dryDistance(map.cells.size(), 10000);
                std::queue<MapCellCoordinate> frontier;
                for (int y = 0; y < map.height; ++y)
                    for (int x = 0; x < map.width; ++x)
                        if (x == 0 || y == 0 || x == map.width - 1 || y == map.height - 1 ||
                            mapCellAt(map, x, y)->kind == MapTileKind::Water)
                        {
                            dryDistance[mapCellIndex(map, x, y)] = 0;
                            frontier.push({x, y});
                        }
                while (!frontier.empty())
                {
                    const auto p = frontier.front();
                    frontier.pop();
                    for (int dy = -1; dy <= 1; ++dy)
                        for (int dx = -1; dx <= 1; ++dx)
                        {
                            if (!isMapCellInside(map, p.x + dx, p.y + dy))
                                continue;
                            const int i = mapCellIndex(map, p.x + dx, p.y + dy),
                                      d = dryDistance[mapCellIndex(map, p.x, p.y)] + 1;
                            if (dryDistance[i] > d)
                            {
                                dryDistance[i] = d;
                                frontier.push({p.x + dx, p.y + dy});
                            }
                        }
                }
                bool found = false;
                for (; radius >= 2 && !found; --radius)
                {
                    float best = std::numeric_limits<float>::max();
                    int chosenX = -1, chosenY = -1;
                    for (int y = 2; y < map.height - 2; ++y)
                        for (int x = 2; x < map.width - 2; ++x)
                        {
                            const auto i = mapCellIndex(map, x, y);
                            if (dryDistance[i] < radius + 2)
                                continue;
                            const float score =
                                elevation[i] + (mapCellAt(map, x, y)->kind == MapTileKind::Mountain ? .4f : 0.f) +
                                std::hypot(static_cast<float>(x - centerX), static_cast<float>(y - centerY)) * .003f +
                                mapGenV2Hash01(options.seed + lakeIndex * 731u, x, y) * .07f;
                            if (score < best)
                            {
                                best = score;
                                chosenX = x;
                                chosenY = y;
                            }
                        }
                    if (chosenX >= 0)
                    {
                        centerX = chosenX;
                        centerY = chosenY;
                        found = true;
                        ++radius;
                    }
                }
                if (!found)
                    continue;
            }
            const auto lakeRadius = [&](float angle) {
                if (options.generatorVersion >= 10 && options.scale == MapLevel::World)
                    return radius * (.75f + .15f * std::sin(angle * 2.f + (options.seed % 31) * .13f) +
                                     .09f * std::sin(angle * 3.f + lakeIndex) + .035f * std::cos(angle * 5.f));
                return radius * (.83f + .10f * std::sin(angle * 3.f + (options.seed % 31) * .13f) +
                                 .06f * std::cos(angle * 5.f + lakeIndex));
            };
            if (options.generatorVersion >= 8)
            {
                for (int y = centerY - radius; y <= centerY + radius; ++y)
                    for (int x = centerX - radius; x <= centerX + radius; ++x)
                    {
                        const float dx = static_cast<float>(x - centerX), dy = (y - centerY) / .78f;
                        if (std::hypot(dx, dy) <= lakeRadius(std::atan2(dy, dx)))
                            mapGenSetCell(map, x, y, MapTileKind::Water);
                    }
            }
            else
                mapGenPaintDisc(map, centerX, centerY, radius, MapTileKind::Water, rng, 8);
            MapGeneratedFeature lake;
            lake.id = mapGenV2FeatureId(options.seed, "lake", lakeIndex);
            lake.kind = MapGeneratedFeatureKind::TerrainRegion;
            lake.name = "Lake " + std::to_string(lakeIndex + 1);
            lake.materialId = "fantasy_neutral.water.lake";
            lake.closed = true;
            lake.width = static_cast<float>(radius);
            const int outlinePoints = options.generatorVersion >= 8 ? 64 : 16;
            for (int point = 0; point < outlinePoints; ++point)
            {
                constexpr float pi = 3.14159265358979323846f;
                const float angle = static_cast<float>(point) / outlinePoints * pi * 2.0f;
                const float wobble =
                    0.84f +
                    mapGenV2Hash01(options.seed + static_cast<std::uint32_t>(lakeIndex * 37), point, lakeIndex) * 0.24f;
                if (options.generatorVersion >= 8)
                    lake.points.push_back({centerX + .5f + std::cos(angle) * lakeRadius(angle),
                                           centerY + .5f + std::sin(angle) * lakeRadius(angle) * .78f});
                else
                    lake.points.push_back({centerX + 0.5f + std::cos(angle) * radius * wobble,
                                           centerY + 0.5f + std::sin(angle) * radius * wobble});
            }
            map.generation.graph.features.push_back(std::move(lake));
        }
    }
}

