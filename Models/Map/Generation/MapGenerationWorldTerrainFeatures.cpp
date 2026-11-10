#include "Models/Map/MapGeneration.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
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


float mapGenV5WorldMountainRidgeInfluence(std::uint32_t seed, float normalizedX, float normalizedY, int ridgeIndex,
                                          int mountainDensity)
{
    // Overview mountains are continental structures, not every cell above a
    // global noise threshold. A narrow, gently curved ridge produces a
    // readable range with foothills on either side and prevents one noisy
    // quadrant from turning into a solid wall of repeated mountain stamps.
    constexpr float pi = 3.14159265358979323846f;
    const std::uint32_t ridgeSeed = seed + 4013u + static_cast<std::uint32_t>(ridgeIndex * 977u);
    const float angle = (mapGenV2Hash01(ridgeSeed, 3, 7) - 0.5f) * pi * 0.84f + (ridgeIndex == 0 ? 0.0f : pi * 0.47f);
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    const float along = normalizedX * cosine + normalizedY * sine;
    const float across = -normalizedX * sine + normalizedY * cosine;
    const float offsetDirection = mapGenV2Hash01(ridgeSeed + 13u, 11, 5) < 0.5f ? -1.0f : 1.0f;
    const float offset = offsetDirection * (0.20f + mapGenV2Hash01(ridgeSeed + 17u, 5, 11) * 0.24f);
    const float phase = mapGenV2Hash01(ridgeSeed + 29u, 2, 19) * pi * 2.0f;
    const float bend =
        std::sin(along * pi * 1.35f + phase) * (0.055f + mapGenV2Hash01(ridgeSeed + 41u, 17, 3) * 0.055f);
    const float halfLength = 0.50f + mapGenV2Hash01(ridgeSeed + 53u, 23, 2) * 0.28f;
    const float endFade = std::clamp((halfLength - std::abs(along)) / 0.16f, 0.0f, 1.0f);
    // At 72x48 the former visible core was narrower than one vertical cell and
    // sampled as a row of unrelated tiny stamps. Keep the mathematical ridge
    // wide enough for an unbroken 8-neighbour raster spine.
    const float halfWidth = 0.078f + static_cast<float>(std::clamp(mountainDensity, 0, 100)) * 0.00032f;
    const float distance = std::abs(across - offset - bend);
    return std::clamp((1.0f - distance / std::max(0.025f, halfWidth)) * endFade, 0.0f, 1.0f);
}

float mapGenV5WorldForestClusterInfluence(std::uint32_t seed, float normalizedX, float normalizedY, int clusterIndex)
{
    // World forests are geographic masses rather than per-cell noise.  A
    // rotated, gently curved belt plus a secondary lobe creates a readable
    // macro-biome with an irregular edge and deliberate open plains.
    constexpr float pi = 3.14159265358979323846f;
    const std::uint32_t clusterSeed = seed + 4513u + static_cast<std::uint32_t>(clusterIndex * 811u);
    const float centerX = (mapGenV2Hash01(clusterSeed, 5, 17) - 0.5f) * 1.18f;
    const float centerY = (mapGenV2Hash01(clusterSeed + 7u, 19, 3) - 0.5f) * 1.04f;
    const float radiusX = 0.30f + mapGenV2Hash01(clusterSeed + 13u, 2, 23) * 0.18f;
    const float radiusY = 0.17f + mapGenV2Hash01(clusterSeed + 29u, 29, 7) * 0.12f;
    const float angle = (mapGenV2Hash01(clusterSeed + 37u, 11, 31) - 0.5f) * pi * 0.88f;
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    const float localX = (normalizedX - centerX) * cosine + (normalizedY - centerY) * sine;
    const float localY = -(normalizedX - centerX) * sine + (normalizedY - centerY) * cosine;
    const float phase = mapGenV2Hash01(clusterSeed + 43u, 17, 5) * pi * 2.0f;
    const float curvedY = localY - std::sin(localX / std::max(0.08f, radiusX) * pi + phase) * radiusY * 0.24f;
    const float belt =
        1.0f - std::sqrt((localX / radiusX) * (localX / radiusX) + (curvedY / radiusY) * (curvedY / radiusY));
    const float lobeX = localX - radiusX * 0.42f;
    const float lobeY = localY + radiusY * 0.48f;
    const float lobe = 1.0f - std::sqrt((lobeX / (radiusX * 0.58f)) * (lobeX / (radiusX * 0.58f)) +
                                        (lobeY / (radiusY * 0.82f)) * (lobeY / (radiusY * 0.82f)));
    return std::clamp(std::max(belt, lobe * 0.90f), 0.0f, 1.0f);
}

bool mapGenV5WorldTerrainOverlayGapCell(const MapModel &map, int x, int y, bool includeMountainPasses)
{
    const MapCellModel *cell = mapCellAt(map, x, y);
    if (!cell)
        return false;
    if (includeMountainPasses && cell->kind == MapTileKind::Stone && mapGenWayKindIsRoadLike(cell->wayKind))
        return true;

    // A river or its bridge is an overlay through an otherwise continuous
    // overview biome/range. Treat only cells close to an actual editable
    // river as a narrow connector; arbitrary ocean and lake cells must still
    // split separate landforms.
    if (cell->kind != MapTileKind::Water && cell->kind != MapTileKind::Bridge && !mapWayKindIsWaterLike(cell->wayKind))
        return false;
    float tangentX = 0.0f;
    float tangentY = 0.0f;
    return mapGenV2NearestFeatureTangent(map.generation.graph, MapGeneratedFeatureKind::River,
                                         static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f, 1.65f, tangentX,
                                         tangentY);
}

std::vector<std::vector<MapCellCoordinate>> mapGenV5WorldTerrainComponents(const MapModel &map, MapTileKind kind,
                                                                           bool includeMountainPasses)
{
    std::vector<std::vector<MapCellCoordinate>> components;
    std::vector<unsigned char> visited(map.cells.size(), 0);
    constexpr MapCellCoordinate neighbors[] = {{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}};
    constexpr int maximumOverlayGap = 3;
    for (int y = 0; y < map.height; ++y)
        for (int x = 0; x < map.width; ++x)
        {
            const int startIndex = mapCellIndex(map, x, y);
            const MapCellModel *start = mapCellAt(map, x, y);
            if (startIndex < 0 || !start || start->kind != kind || visited[static_cast<std::size_t>(startIndex)])
                continue;
            std::vector<MapCellCoordinate> component;
            std::queue<MapCellCoordinate> pending;
            pending.push({x, y});
            visited[static_cast<std::size_t>(startIndex)] = 1;
            while (!pending.empty())
            {
                const MapCellCoordinate current = pending.front();
                pending.pop();
                component.push_back(current);
                for (const MapCellCoordinate direction : neighbors)
                {
                    bool crossedOverlay = false;
                    for (int distance = 1; distance <= maximumOverlayGap + 1; ++distance)
                    {
                        const int nextX = current.x + direction.x * distance;
                        const int nextY = current.y + direction.y * distance;
                        const int nextIndex = mapCellIndex(map, nextX, nextY);
                        const MapCellModel *next = mapCellAt(map, nextX, nextY);
                        if (nextIndex < 0 || !next)
                            break;
                        if (next->kind == kind)
                        {
                            // Adjacent terrain always joins. Across a gap,
                            // join only when every intervening cell belonged
                            // to a narrow river/pass overlay.
                            if ((distance == 1 || crossedOverlay) && !visited[static_cast<std::size_t>(nextIndex)])
                            {
                                visited[static_cast<std::size_t>(nextIndex)] = 1;
                                pending.push({nextX, nextY});
                            }
                            break;
                        }
                        if (!mapGenV5WorldTerrainOverlayGapCell(map, nextX, nextY, includeMountainPasses))
                            break;
                        crossedOverlay = true;
                    }
                }
            }
            components.push_back(std::move(component));
        }
    return components;
}

void mapGenV5AddWorldMountainRangeFeatures(MapModel &map, const MapGenerationOptions &options)
{
    if (options.generatorVersion < 5 ||
        (map.atlasLevel != MapLevel::World && !(options.generatorVersion >= 8 && map.atlasLevel == MapLevel::Region)))
        return;
    int rangeIndex = 0;
    const int minimumCells = std::max(3, std::min(map.width, map.height) / 7);
    std::vector<std::vector<MapCellCoordinate>> components =
        mapGenV5WorldTerrainComponents(map, MapTileKind::Mountain, true);
    for (std::vector<MapCellCoordinate> &component : components)
    {
        if (static_cast<int>(component.size()) < minimumCells)
            continue;

        // Project the component onto its dominant axis and retain a small
        // number of averaged controls. The raster remains authoritative for
        // movement, while this spline gives atlas inheritance and QA an
        // explicit, scale-correct mountain range. Narrow rivers and explicit
        // road passes may cross the range without splitting it into unrelated
        // atlas features.
        int minX = map.width;
        int maxX = 0;
        int minY = map.height;
        int maxY = 0;
        for (const MapCellCoordinate cell : component)
        {
            minX = std::min(minX, cell.x);
            maxX = std::max(maxX, cell.x);
            minY = std::min(minY, cell.y);
            maxY = std::max(maxY, cell.y);
        }
        const bool horizontal = maxX - minX >= maxY - minY;
        std::stable_sort(component.begin(), component.end(),
                         [&](const MapCellCoordinate &left, const MapCellCoordinate &right) {
                             const int leftPrimary = horizontal ? left.x : left.y;
                             const int rightPrimary = horizontal ? right.x : right.y;
                             if (leftPrimary != rightPrimary)
                                 return leftPrimary < rightPrimary;
                             return (horizontal ? left.y : left.x) < (horizontal ? right.y : right.x);
                         });
        const int controlCount = std::clamp(static_cast<int>(component.size()) / 4, 2, 8);
        MapGeneratedFeature range;
        range.id = mapGenV2FeatureId(options.seed, "mountain_range", rangeIndex);
        range.kind = MapGeneratedFeatureKind::MountainRange;
        range.name = "Mountain range " + std::to_string(rangeIndex + 1);
        range.materialId = "fantasy_neutral.mountain.connected_ridge";
        range.width = std::max(1.4f, static_cast<float>(component.size()) /
                                         std::max(4.0f, static_cast<float>(controlCount) * 2.2f));
        for (int control = 0; control < controlCount; ++control)
        {
            const std::size_t begin =
                static_cast<std::size_t>(control) * component.size() / static_cast<std::size_t>(controlCount);
            const std::size_t end =
                static_cast<std::size_t>(control + 1) * component.size() / static_cast<std::size_t>(controlCount);
            float averageX = 0.0f;
            float averageY = 0.0f;
            for (std::size_t index = begin; index < end; ++index)
            {
                averageX += static_cast<float>(component[index].x) + 0.5f;
                averageY += static_cast<float>(component[index].y) + 0.5f;
            }
            const float divisor = static_cast<float>(std::max<std::size_t>(1, end - begin));
            range.points.push_back({averageX / divisor, averageY / divisor});
        }
        map.generation.graph.features.push_back(std::move(range));
        ++rangeIndex;
    }
}
