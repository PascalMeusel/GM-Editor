#include "Models/Map/TacticalMapGenerator.hpp"
#include "Models/Map/MapTileSemantics.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "Models/Map/TacticalMapGeneratorInternal.hpp"

namespace TacticalMapGeneratorInternal
{
const MapRegionModel *findRegion(const MapModel &map, const std::string &name)
{
    const auto found = std::find_if(map.regions.begin(), map.regions.end(), [&](const MapRegionModel &region) { return region.name == name; });
    return found == map.regions.end() ? nullptr : &*found;
}

bool regionReachable(const MapModel &map, const MapRegionModel &from, const MapRegionModel &to)
{
    return mapGenCoordinatesConnected(map,
                                      {from.x + from.width / 2, from.y + from.height / 2},
                                      {to.x + to.width / 2, to.y + to.height / 2}, false);
}

bool hasFootprint(const MapModel &map, int size)
{
    size = std::clamp(size, 1, 4);
    for (int y = 0; y + size <= map.height; ++y)
        for (int x = 0; x + size <= map.width; ++x)
        {
            bool clear = true;
            for (int oy = 0; oy < size && clear; ++oy)
                for (int ox = 0; ox < size && clear; ++ox)
                    clear = cellCanSpawn(mapCellAt(map, x + ox, y + oy));
            if (clear)
                return true;
        }
    return false;
}

bool hasRouteAcrossBand(const MapModel &map, int routeY, bool upperBand)
{
    const Point start{2, std::clamp(routeY, 0, map.height - 1)};
    const Point goal{map.width - 3, std::clamp(routeY, 0, map.height - 1)};
    auto insideBand = [&](int y) {
        return upperBand ? y <= map.height / 2 + 1 : y >= map.height / 2 - 1;
    };
    if (!insideBand(start.y) || !cellCanSpawn(mapCellAt(map, start.x, start.y)) ||
        !cellCanSpawn(mapCellAt(map, goal.x, goal.y)))
        return false;
    std::vector<unsigned char> visited(static_cast<std::size_t>(map.width * map.height), 0);
    std::queue<Point> pending;
    pending.push(start);
    visited[static_cast<std::size_t>(mapCellIndex(map, start.x, start.y))] = 1;
    constexpr int offsets[][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    while (!pending.empty())
    {
        const Point current = pending.front();
        pending.pop();
        if (current.x == goal.x && current.y == goal.y)
            return true;
        for (const auto &offset : offsets)
        {
            const int x = current.x + offset[0];
            const int y = current.y + offset[1];
            if (!isMapCellInside(map, x, y) || !insideBand(y) || !cellCanSpawn(mapCellAt(map, x, y)))
                continue;
            const std::size_t index = static_cast<std::size_t>(mapCellIndex(map, x, y));
            if (visited[index])
                continue;
            visited[index] = 1;
            pending.push({x, y});
        }
    }
    return false;
}

std::vector<std::string> validate(const MapModel &map, const TacticalMapRecipe &recipe)
{
    std::vector<std::string> issues;
    const MapRegionModel *party = findRegion(map, "Party Start");
    const MapRegionModel *opposition = findRegion(map, "Opposition Start");
    if (!party || !opposition)
        issues.push_back("Two reachable starting zones are required.");

    int coverLeft = 0;
    int coverRight = 0;
    int traversable = 0;
    int hazards = 0;
    for (int y = 0; y < map.height; ++y)
        for (int x = 0; x < map.width; ++x)
            if (const MapCellModel *cell = mapCellAt(map, x, y))
            {
                if (cellCanSpawn(cell)) ++traversable;
                if (cell->kind == MapTileKind::Cover || cell->kind == MapTileKind::Rubble || cell->blocksLineOfSight)
                    (x < map.width / 2 ? coverLeft : coverRight)++;
                if (cell->kind == MapTileKind::Hazard || cell->kind == MapTileKind::Pit || cell->kind == MapTileKind::Chasm)
                    ++hazards;
            }
    if (isSubterranean(recipe.mapTemplate))
    {
        // Rock and wall are intentional negative space on cave and dungeon
        // canvases. Requiring half of the complete bounding rectangle to be
        // floor rewards implausibly hollow rectangles and penalizes readable
        // rooms and tunnels. Require enough connected playable floor for two
        // deployments, objectives and manoeuvring; the route/access checks
        // below still reject narrow or disconnected layouts.
        const int minimumPlayableFloor = std::max(32, map.width * map.height / 6);
        if (traversable < minimumPlayableFloor)
            issues.push_back("The carved cave or dungeon floor is too small for the encounter.");
    }
    else if (traversable < map.width * map.height / 2)
        issues.push_back("Less than half the map is safely traversable.");
    if (coverLeft < 2 || coverRight < 2)
        issues.push_back("Both sides need usable cover or line-of-sight breaks.");

    int objectives = 0;
    int routes = 0;
    int upperRouteY = map.height;
    int lowerRouteY = -1;
    bool objectiveUnreachable = false;
    for (const MapRegionModel &region : map.regions)
    {
        if (region.name.rfind("Objective ", 0) == 0)
        {
            ++objectives;
            if (party && opposition && (!regionReachable(map, *party, region) || !regionReachable(map, *opposition, region)))
                objectiveUnreachable = true;
        }
        if (region.name.rfind("Tactical Route ", 0) == 0)
        {
            ++routes;
            const int centerY = region.y + region.height / 2;
            upperRouteY = std::min(upperRouteY, centerY);
            lowerRouteY = std::max(lowerRouteY, centerY);
        }
    }
    if (objectives < std::clamp(recipe.objectiveCount, 1, 3))
        issues.push_back("The requested visible encounter objectives are missing.");
    else if (objectiveUnreachable)
        issues.push_back("An objective is not reachable from both starting zones.");
    if (routes < 2)
        issues.push_back("At least two meaningful routes between the teams are required.");
    else if (!hasRouteAcrossBand(map, upperRouteY, true) || !hasRouteAcrossBand(map, lowerRouteY, false))
        issues.push_back("Both the upper and lower tactical routes must remain usable.");
    if (hazards == 0 && !findRegion(map, "Chokepoint"))
        issues.push_back("The encounter needs a hazard or a deliberate chokepoint.");
    if (party && opposition && !regionReachable(map, *party, *opposition))
        issues.push_back("The starting zones do not have a usable route between them.");
    if (!hasFootprint(map, recipe.minimumCreatureFootprint))
        issues.push_back("No area supports the requested creature footprint.");
    return issues;
}
}
