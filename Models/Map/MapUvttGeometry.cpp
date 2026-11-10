#include "Models/Map/MapUvttExport.hpp"
#include <map>
#include <set>

namespace
{
const MapCellModel *cellAt(const MapModel &map, int x, int y)
{
    if (x < 0 || y < 0 || x >= map.width || y >= map.height) return nullptr;
    const auto index = static_cast<std::size_t>(y) * map.width + x;
    return index < map.cells.size() ? &map.cells[index] : nullptr;
}
bool solid(const MapCellModel *cell)
{
    return cell && cell->kind != MapTileKind::Door &&
           (cell->kind == MapTileKind::Wall || cell->blocksLineOfSight);
}
bool verticalDoor(const MapModel &map, int x, int y, const MapCellModel &cell)
{
    if (map.scope != MapScope::Dungeon || map.creationIntent != MapCreationIntent::Dungeon ||
        map.generation.recipe.generatorVersion < 10)
        return normalizeMapRotation(cell.rotationQuarterTurns) % 2 != 0;
    // Same corridor/wall orientation rule as buildDungeonArchitectureInk.
    const auto wall = [&](int dx, int dy) {
        const auto *neighbor = cellAt(map, x + dx, y + dy);
        return !neighbor || neighbor->kind == MapTileKind::Wall || neighbor->kind == MapTileKind::Empty;
    };
    const auto route = [&](int dx, int dy) {
        const auto *neighbor = cellAt(map, x + dx, y + dy);
        return neighbor && neighbor->wayKind == MapWayKind::DungeonCorridor;
    };
    const int horizontal = route(-1, 0) + route(1, 0), vertical = route(0, -1) + route(0, 1);
    return horizontal == vertical ? wall(0, -1) + wall(0, 1) >= wall(-1, 0) + wall(1, 0) : horizontal > vertical;
}
}

MapUvttGeometry mapUvttGeometry(const MapModel &map)
{
    MapUvttGeometry result;
    if (map.width < 1 || map.height < 1 || map.width > 256 || map.height > 256) return result;
    // Set membership de-duplicates symmetric cell-edge masks. Merge adjacent
    // collinear unit edges to keep importer wall work proportional to structure.
    std::map<int, std::set<int>> horizontal, vertical;
    for (int y = 0; y < map.height; ++y)
        for (int x = 0; x < map.width; ++x)
        {
            const auto *cell = cellAt(map, x, y);
            if (!cell) continue;
            if (cell->movementOneWayBlockedMask || cell->lineOfSightOneWayBlockedMask)
                ++result.omittedDirectionalCells;
            if (cell->movementBlockedMask & ~cell->lineOfSightBlockedMask & 15)
                ++result.omittedMovementOnlyCells;
            if (cell->kind == MapTileKind::Door)
            {
                result.doors.push_back(verticalDoor(map, x, y, *cell)
                    ? std::array<int, 4>{2*x+1, 2*y, 2*x+1, 2*y+2}
                    : std::array<int, 4>{2*x, 2*y+1, 2*x+2, 2*y+1});
                continue;
            }
            for (int direction = 0; direction < 4; ++direction)
            {
                const auto *neighbor = cellAt(map, x + mapCardinalEdgeDeltaX(direction),
                                              y + mapCardinalEdgeDeltaY(direction));
                if (solid(cell) && solid(neighbor)) continue;
                if (!solid(cell) && !(cell->lineOfSightBlockedMask & mapCardinalEdgeMask(direction))) continue;
                if (direction == 0) horizontal[y].insert(x);
                else if (direction == 2) horizontal[y+1].insert(x);
                else if (direction == 1) vertical[x+1].insert(y);
                else vertical[x].insert(y);
            }
        }
    const auto collect = [&](const auto &lines, bool horizontalLine) {
        for (const auto &[fixed, starts] : lines)
        {
            auto next = starts.begin();
            while (next != starts.end())
            {
                const int first = *next;
                int last = first + 1;
                ++next;
                while (next != starts.end() && *next == last) { ++last; ++next; }
                result.walls.push_back(horizontalLine
                    ? std::array<int, 4>{2*first, 2*fixed, 2*last, 2*fixed}
                    : std::array<int, 4>{2*fixed, 2*first, 2*fixed, 2*last});
            }
        }
    };
    collect(horizontal, true);
    collect(vertical, false);
    return result;
}
