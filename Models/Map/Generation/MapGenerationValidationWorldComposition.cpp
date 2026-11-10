#include "Models/Map/MapGeneration.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <queue>
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

int mapGenV5WorldMountainCompositionPercent(const MapModel &map)
{
    if (map.generation.recipe.generatorVersion < 5 || map.atlasLevel != MapLevel::World)
        return 100;
    const bool islands =
        map.generation.recipe.generatorVersion >= 11 && map.generation.recipe.landform == MapLandform::Islands;
    int land = 0;
    int mountains = 0;
    int rangeCount = 0;
    float longestRange = 0.0f;
    for (const MapCellModel &cell : map.cells)
    {
        if (cell.kind == MapTileKind::Water)
            continue;
        ++land;
        mountains += cell.kind == MapTileKind::Mountain ? 1 : 0;
    }
    std::vector<int> componentSizes;
    for (const std::vector<MapCellCoordinate> &component :
         mapGenV5WorldTerrainComponents(map, MapTileKind::Mountain, true))
        componentSizes.push_back(static_cast<int>(component.size()));
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if (feature.kind != MapGeneratedFeatureKind::MountainRange || feature.points.size() < 2)
            continue;
        ++rangeCount;
        float length = 0.0f;
        for (std::size_t index = 1; index < feature.points.size(); ++index)
        {
            const float deltaX = feature.points[index].x - feature.points[index - 1].x;
            const float deltaY = feature.points[index].y - feature.points[index - 1].y;
            length += std::sqrt(deltaX * deltaX + deltaY * deltaY);
        }
        longestRange = std::max(longestRange, length);
    }
    if (land == 0)
        return 0;
    const int mountainPercent = mountains * 100 / land;
    const bool mountainLandscape = map.generation.recipe.landscape == MapLandscapeProfile::Highlands ||
                                   map.generation.recipe.landscape == MapLandscapeProfile::MountainPass;
    if (map.generation.recipe.generatorVersion >= 11 && map.generation.recipe.mountainDensity == 0)
        return mountains == 0 ? 100 : 40;
    if (mountainLandscape && mountainPercent < 1)
        return 60;
    if (mountainPercent > 20)
        return std::clamp(100 - (mountainPercent - 20) * 5, 0, 100);
    if (mountains > 0 && (rangeCount == 0 || longestRange < static_cast<float>(std::min(map.width, map.height)) *
                                                                (islands ? 0.075f : 0.18f)))
        return 60;
    if (mountains > 0)
    {
        std::sort(componentSizes.begin(), componentSizes.end(), std::greater<int>());
        const int minimumComponent = std::max(3, std::min(map.width, map.height) / 8);
        int tinyCells = 0;
        int significantComponents = 0;
        for (const int size : componentSizes)
        {
            if (size < minimumComponent)
                tinyCells += size;
            else
                ++significantComponents;
        }
        const int coherentCells =
            componentSizes.empty()
                ? 0
                : componentSizes.front() + (mountainLandscape && componentSizes.size() > 1 ? componentSizes[1] : 0);
        if (tinyCells * 100 > mountains * 10 || coherentCells * 100 < mountains * (islands ? 30 : 75) ||
            significantComponents > (islands             ? 5
                                     : mountainLandscape ? 2
                                                         : 1))
            return 55;
    }
    // More than two independent ranges at this zoom normally means the
    // raster ridge fragmented into repeated short stamps. Prefer one legible
    // continental chain (two only for mountain-heavy compositions).
    if (rangeCount > (islands ? 5 : 2))
        return std::clamp(100 - (rangeCount - 2) * 30, 0, 100);
    return 100;
}

int mapGenV5WorldLargestLandMotifPercent(const MapModel &map)
{
    if (map.width <= 0 || map.height <= 0)
        return 0;
    const int largestLandComponent = mapGenLargestComponentSize(
        map,
        [](const MapCellModel &cell) {
            return !(cell.kind == MapTileKind::Water && cell.wayKind != MapWayKind::River &&
                     cell.wayKind != MapWayKind::Canal);
        },
        false);
    return largestLandComponent * 100 / std::max(1, map.width * map.height);
}

int mapGenV5WorldCoastCompositionPercent(const MapModel &map)
{
    if (map.generation.recipe.generatorVersion < 5 || map.atlasLevel != MapLevel::World || map.width <= 0 ||
        map.height <= 0)
        return 100;

    const auto isLand = [&](int x, int y) {
        const MapCellModel *cell = mapCellAt(map, x, y);
        // A projected river is Water in the raster but still belongs to the
        // continental land component. Lakes and ocean remain true water.
        return cell && !(cell->kind == MapTileKind::Water && cell->wayKind != MapWayKind::River &&
                         cell->wayKind != MapWayKind::Canal);
    };
    struct LandComponent
    {
        int cells = 0;
        int perimeter = 0;
        int minX = 0;
        int maxX = 0;
        int minY = 0;
        int maxY = 0;
    };
    std::vector<LandComponent> components;
    std::vector<unsigned char> visited(map.cells.size(), 0);
    constexpr MapCellCoordinate cardinal[] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
    int totalLand = 0;
    for (int y = 0; y < map.height; ++y)
        for (int x = 0; x < map.width; ++x)
        {
            const int startIndex = mapCellIndex(map, x, y);
            if (startIndex < 0 || !isLand(x, y) || visited[static_cast<std::size_t>(startIndex)])
                continue;
            LandComponent component;
            component.minX = component.maxX = x;
            component.minY = component.maxY = y;
            std::queue<MapCellCoordinate> pending;
            pending.push({x, y});
            visited[static_cast<std::size_t>(startIndex)] = 1;
            while (!pending.empty())
            {
                const MapCellCoordinate current = pending.front();
                pending.pop();
                ++component.cells;
                component.minX = std::min(component.minX, current.x);
                component.maxX = std::max(component.maxX, current.x);
                component.minY = std::min(component.minY, current.y);
                component.maxY = std::max(component.maxY, current.y);
                for (const MapCellCoordinate direction : cardinal)
                {
                    const int nextX = current.x + direction.x;
                    const int nextY = current.y + direction.y;
                    if (!isLand(nextX, nextY))
                    {
                        ++component.perimeter;
                        continue;
                    }
                    const int nextIndex = mapCellIndex(map, nextX, nextY);
                    if (nextIndex < 0 || visited[static_cast<std::size_t>(nextIndex)])
                        continue;
                    visited[static_cast<std::size_t>(nextIndex)] = 1;
                    pending.push({nextX, nextY});
                }
            }
            totalLand += component.cells;
            components.push_back(component);
        }
    if (components.empty())
        return 0;
    std::stable_sort(components.begin(), components.end(),
                     [](const LandComponent &left, const LandComponent &right) { return left.cells > right.cells; });
    const int mapArea = std::max(1, map.width * map.height);
    const int landPercent = totalLand * 100 / mapArea;
    if ((map.generation.recipe.generatorVersion < 11 || map.generation.recipe.landform == MapLandform::Automatic) &&
        (landPercent < 24 || landPercent > 82))
        return 55;

    // V11 promises geography explicitly. An inland world is a cropped piece
    // of a continent, so demanding an ocean perimeter would reject valid maps.
    if (map.generation.recipe.generatorVersion >= 11 && map.generation.recipe.landform != MapLandform::Automatic)
    {
        const auto landform = map.generation.recipe.landform;
        int dryBoundary = 0, boundary = 0;
        for (int y = 0; y < map.height; ++y)
            for (int x = 0; x < map.width; ++x)
                if (x == 0 || y == 0 || x == map.width - 1 || y == map.height - 1)
                {
                    ++boundary;
                    bool riverMouth = false;
                    if (!isLand(x, y))
                        for (int dy = -1; dy <= 1; ++dy)
                            for (int dx = -1; dx <= 1; ++dx)
                            {
                                const auto *nearby = mapCellAt(map, x + dx, y + dy);
                                riverMouth = riverMouth || (nearby && (nearby->wayKind == MapWayKind::River ||
                                                                       nearby->wayKind == MapWayKind::Canal));
                            }
                    dryBoundary += isLand(x, y) || riverMouth ? 1 : 0;
                }
        if (landform == MapLandform::Continental)
            return dryBoundary == boundary && landPercent >= 85 ? 100 : 40;
        if (landform == MapLandform::InlandLake)
            return dryBoundary == boundary && landPercent >= 55 && landPercent <= 94 &&
                           components.front().cells * 100 >= totalLand * 95
                       ? 100
                       : 40;
        if (landform == MapLandform::SingleIsland)
            return dryBoundary <= boundary / 12 && landPercent >= 15 && landPercent <= 60 &&
                           components.front().cells * 100 >= totalLand * 95
                       ? 100
                       : 40;
        if (landform == MapLandform::Coast || landform == MapLandform::Peninsula)
            return dryBoundary >= boundary / 6 && dryBoundary <= boundary * 3 / 4 && landPercent >= 18 &&
                           landPercent <= 75 && components.front().cells * 100 >= totalLand * 95
                       ? 100
                       : 40;
        int substantial = 0;
        for (const auto &component : components)
            substantial += component.cells >= mapArea / 30 ? 1 : 0;
        return dryBoundary <= boundary / 12 && substantial >= 3 && substantial <= 6 && landPercent >= 16 &&
                       landPercent <= 55
                   ? 100
                   : 40;
    }
    const LandComponent &mainland = components.front();
    const bool sharedRiverWorld = mapGenV4RequestsSharedGreatRiver(map);
    const int mainlandMapPercent = mainland.cells * 100 / mapArea;
    if (mainlandMapPercent < (sharedRiverWorld ? 22 : 12))
        return 52;
    if (sharedRiverWorld && mainland.cells * 100 < totalLand * 80)
        return 55;
    if (!sharedRiverWorld && mainland.cells * 100 < totalLand * 34)
        return 60;

    const int significantThreshold = std::max(3, mapArea / 900);
    int significantComponents = 0;
    int offshoreLand = 0;
    for (std::size_t index = 0; index < components.size(); ++index)
    {
        if (components[index].cells >= significantThreshold)
            ++significantComponents;
        if (index > 0)
            offshoreLand += components[index].cells;
    }
    if (significantComponents > 6 || (sharedRiverWorld && offshoreLand * 100 > totalLand * 18))
        return 60;

    const int boundsWidth = mainland.maxX - mainland.minX + 1;
    const int boundsHeight = mainland.maxY - mainland.minY + 1;
    const int boundsArea = std::max(1, boundsWidth * boundsHeight);
    const float boundsFill = static_cast<float>(mainland.cells) / static_cast<float>(boundsArea);
    const float rectangularPerimeter = std::max(1.0f, 2.0f * static_cast<float>(boundsWidth + boundsHeight));
    const float coastArticulation = static_cast<float>(mainland.perimeter) / rectangularPerimeter;
    // A rasterised ellipse fills roughly pi/4 of its bounds and has about the
    // same cardinal perimeter as that bounds rectangle. Requiring either a
    // meaningful indentation or a lower fill rejects the broad oval worlds
    // while accepting lobed coasts, bays and peninsulas.
    if (boundsFill > 0.745f && coastArticulation < 1.105f)
        return 62;
    if (boundsFill > 0.86f || coastArticulation > 1.90f)
        return 58;
    return 100;
}

int mapGenV5WorldBiomeCompositionPercent(const MapModel &map)
{
    if (map.generation.recipe.generatorVersion < 5 || map.atlasLevel != MapLevel::World)
        return 100;
    const bool islands =
        map.generation.recipe.generatorVersion >= 11 && map.generation.recipe.landform == MapLandform::Islands;
    int land = 0;
    int forest = 0;
    for (const MapCellModel &cell : map.cells)
    {
        if (cell.kind == MapTileKind::Water && cell.wayKind != MapWayKind::River && cell.wayKind != MapWayKind::Canal)
            continue;
        ++land;
        forest += cell.kind == MapTileKind::Forest ? 1 : 0;
    }
    if (land <= 0)
        return 0;
    if (map.generation.recipe.generatorVersion >= 11 && map.generation.recipe.forestDensity == 0)
        return forest == 0 ? 100 : 40;
    if (forest == 0)
        return map.generation.recipe.landscape == MapLandscapeProfile::ForestFrontier ? 0 : 82;
    const int forestPercent = forest * 100 / land;
    if (forestPercent <
        (map.generation.recipe.generatorVersion >= 11 && map.generation.recipe.forestDensity < 15 ? 0 : 3))
        return 72;
    if (forestPercent > 34)
        return 58;

    int largestComponent = 0;
    int significantComponents = 0;
    const int minimumComponent = std::max(3, std::min(map.width, map.height) / 10);
    for (const std::vector<MapCellCoordinate> &component :
         mapGenV5WorldTerrainComponents(map, MapTileKind::Forest, false))
    {
        const int componentSize = static_cast<int>(component.size());
        largestComponent = std::max(largestComponent, componentSize);
        significantComponents += componentSize >= minimumComponent ? 1 : 0;
    }
    if (largestComponent * 100 < forest * (islands ? 24 : 48) || significantComponents > (islands ? 6 : 4))
        return 62;
    return 100;
}

