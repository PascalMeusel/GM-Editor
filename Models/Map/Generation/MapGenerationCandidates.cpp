#include "Models/Map/MapGeneration.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
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

MapGenerationResult mapGenGenerateCandidate(const MapGenerationOptions &options)
{
    const int totalSteps = options.scope == MapScope::Dungeon ? 6 : (options.generatorVersion >= 2 ? 10 : 12);
    mapGenReportProgress(options, 1, totalSteps, "Preparing map grid");

    MapGenerationResult result;
    result.map.scope = options.scope;
    result.map.atlasLevel = options.scale;
    result.map.creationIntent = options.creationIntent;
    result.map.sizePreset = options.sizePreset;
    result.map.mapKind =
        options.scope == MapScope::Dungeon
            ? MapKind::Dungeon
            : (options.purpose == MapGenerationPurpose::SettlementEnvirons ? MapKind::Settlement : MapKind::Geographic);
    result.map.gridType = GridType::Square;
    result.map.width = std::clamp(options.width, 4, 120);
    result.map.height = std::clamp(options.height, 4, 120);
    result.map.cellSize = options.scope == MapScope::World ? 5.0f : 1.5f;
    result.map.name = "Generated Map";
    result.map.description = "Editable semantic map generated from sliders and a stable seed.";
    resizeMapCells(result.map);
    ensureDefaultMapLayer(result.map);

    std::mt19937 rng(options.seed);
    if (options.scope == MapScope::Dungeon && options.caveNetwork)
    {
        mapGenReportProgress(options, 2, totalSteps, "Building cave network");
        mapGenBuildCaveNetwork(result, options, rng, totalSteps);
    }
    else if (options.scope == MapScope::Dungeon)
    {
        mapGenReportProgress(options, 2, totalSteps, "Building dungeon rooms");
        mapGenClear(result.map, MapTileKind::Wall);
        struct InteriorRoom
        {
            int x = 0;
            int y = 0;
            int width = 1;
            int height = 1;
            int cornerCut = 0;

            bool contains(int px, int py) const
            {
                if (px < x || py < y || px >= x + width || py >= y + height)
                    return false;
                const bool edgeX = px < x + cornerCut || px >= x + width - cornerCut;
                const bool edgeY = py < y + cornerCut || py >= y + height - cornerCut;
                return !(edgeX && edgeY);
            }

            MapCellCoordinate center() const
            {
                return {x + width / 2, y + height / 2};
            }
        };
        const int area = std::max(1, result.map.width * result.map.height);
        const bool composedDungeon = options.generatorVersion >= 10;
        if (composedDungeon)
            result.map.metadata.customProperties["dungeon.composition_revision"] = "3";
        const int requestedRoomCount =
            composedDungeon ? std::clamp(3 + area / 170, 3, 24) : std::clamp(1 + area / 190, 3, 8);
        std::vector<InteriorRoom> rooms;
        rooms.reserve(static_cast<std::size_t>(requestedRoomCount));
        const auto overlaps = [&](const InteriorRoom &candidate, int padding) {
            return std::any_of(rooms.begin(), rooms.end(), [&](const InteriorRoom &room) {
                return candidate.x - padding < room.x + room.width &&
                       candidate.x + candidate.width + padding > room.x &&
                       candidate.y - padding < room.y + room.height &&
                       candidate.y + candidate.height + padding > room.y;
            });
        };
        for (int roomIndex = 0; roomIndex < requestedRoomCount; ++roomIndex)
        {
            bool placed = false;
            for (int attempt = 0; attempt < 180 && !placed; ++attempt)
            {
                const int minimumWidth = std::min(4, std::max(2, result.map.width - 2));
                const int maximumWidth = std::max(
                    minimumWidth, std::min(composedDungeon ? 13 : 9, std::max(minimumWidth, result.map.width / 4)));
                const int minimumHeight = std::min(3, std::max(2, result.map.height - 2));
                const int maximumHeight = std::max(
                    minimumHeight, std::min(composedDungeon ? 11 : 7, std::max(minimumHeight, result.map.height / 4)));
                InteriorRoom candidate;
                candidate.width = mapGenRandomBetween(rng, minimumWidth, maximumWidth);
                candidate.height = mapGenRandomBetween(rng, minimumHeight, maximumHeight);
                candidate.x = mapGenRandomBetween(rng, 1, std::max(1, result.map.width - candidate.width - 1));
                candidate.y = mapGenRandomBetween(rng, 1, std::max(1, result.map.height - candidate.height - 1));
                if (options.generatorVersion >= 11 && roomIndex == 0)
                {
                    // One generous principal chamber creates a spatial anchor.
                    // Jitter its position so the surrounding branches do not
                    // collapse into the same symmetric hub for every seed.
                    candidate.width = maximumWidth;
                    candidate.height = maximumHeight;
                    candidate.x = std::clamp(result.map.width / 2 - candidate.width / 2 +
                                                 mapGenRandomBetween(rng, -std::max(1, result.map.width / 10),
                                                                     std::max(1, result.map.width / 10)),
                                             1, std::max(1, result.map.width - candidate.width - 1));
                    candidate.y = std::clamp(result.map.height / 2 - candidate.height / 2 +
                                                 mapGenRandomBetween(rng, -std::max(1, result.map.height / 10),
                                                                     std::max(1, result.map.height / 10)),
                                             1, std::max(1, result.map.height - candidate.height - 1));
                }
                // Prefer a full wall cell between rooms.  The final third of
                // the fixed attempt budget permits touching walls so small
                // valid presets still complete without timing dependence.
                const int padding = attempt < 120 ? 2 : (attempt < 160 ? 1 : 0);
                if (overlaps(candidate, padding))
                    continue;
                if (composedDungeon && candidate.width >= 5 && candidate.height >= 5)
                {
                    const int form = mapGenRandomBetween(rng, 0, 4);
                    candidate.cornerCut = form == 1   ? 1
                                          : form == 2 ? std::min(candidate.width, candidate.height) / 3
                                                      : 0;
                }
                rooms.push_back(candidate);
                placed = true;
            }
        }
        // Extremely small legacy dimensions cannot physically fit three
        // separated rooms.  Preserve their data compatibility with compact,
        // deterministic room records; ordinary presets never use this path.
        for (int fallback = static_cast<int>(rooms.size()); fallback < 3; ++fallback)
        {
            const int width = std::max(1, std::min(4, result.map.width - 2));
            const int height = std::max(1, std::min(3, result.map.height - 2));
            const int x = std::clamp(1 + fallback * std::max(1, (result.map.width - 2) / 3), 0,
                                     std::max(0, result.map.width - width));
            const int y = std::clamp(1 + (fallback & 1) * std::max(1, result.map.height - height - 2), 0,
                                     std::max(0, result.map.height - height));
            rooms.push_back({x, y, width, height});
        }

        std::vector<MapCellCoordinate> roomCenters;
        roomCenters.reserve(rooms.size());
        for (std::size_t roomIndex = 0; roomIndex < rooms.size(); ++roomIndex)
        {
            const InteriorRoom &room = rooms[roomIndex];
            for (int y = room.y; y < room.y + room.height; ++y)
                for (int x = room.x; x < room.x + room.width; ++x)
                    if (room.contains(x, y))
                        mapGenSetCell(result.map, x, y, MapTileKind::Room);
            roomCenters.push_back(room.center());
            const int x1 = std::min(result.map.width, room.x + room.width);
            const int y1 = std::min(result.map.height, room.y + room.height);
            std::vector<MapFeaturePoint> outline;
            const float x = static_cast<float>(room.x), y = static_cast<float>(room.y);
            const float right = static_cast<float>(x1), bottom = static_cast<float>(y1);
            const float cut = static_cast<float>(room.cornerCut);
            if (room.cornerCut > 0)
                outline = {{x + cut, y},          {right - cut, y},      {right - cut, y + cut},
                           {right, y + cut},      {right, bottom - cut}, {right - cut, bottom - cut},
                           {right - cut, bottom}, {x + cut, bottom},     {x + cut, bottom - cut},
                           {x, bottom - cut},     {x, y + cut},          {x + cut, y + cut}};
            else
                outline = {{x, y}, {right, y}, {right, bottom}, {x, bottom}};
            mapGenAddInteriorGraphFeature(
                result.map, options.seed, "dungeon_room", roomIndex, "Room", "fantasy_neutral.dungeon.room",
                static_cast<float>(std::max(room.width, room.height)), true, std::move(outline));
        }

        std::vector<std::pair<std::size_t, std::size_t>> corridorEdges;
        if (!rooms.empty())
        {
            std::vector<unsigned char> connected(rooms.size(), 0);
            connected[0] = 1;
            while (corridorEdges.size() + 1 < rooms.size())
            {
                int bestDistance = std::numeric_limits<int>::max();
                std::pair<std::size_t, std::size_t> best{0, 0};
                for (std::size_t from = 0; from < rooms.size(); ++from)
                {
                    if (!connected[from])
                        continue;
                    for (std::size_t to = 0; to < rooms.size(); ++to)
                    {
                        if (connected[to])
                            continue;
                        const int dx = roomCenters[from].x - roomCenters[to].x;
                        const int dy = roomCenters[from].y - roomCenters[to].y;
                        const int distance = dx * dx + dy * dy;
                        if (distance < bestDistance || (distance == bestDistance && std::pair{from, to} < best))
                        {
                            bestDistance = distance;
                            best = {from, to};
                        }
                    }
                }
                connected[best.second] = 1;
                corridorEdges.push_back(best);
            }
        }
        // Short optional loops permit alternate approaches. Keep the historical
        // N-1 corridor graph unchanged for old recipes.
        if (composedDungeon && options.routeDensity > 0 && rooms.size() > 3)
        {
            const int desiredLoops = std::clamp(static_cast<int>(rooms.size()) * options.routeDensity / 140, 1, 5);
            for (int loop = 0; loop < desiredLoops; ++loop)
            {
                int bestDistance = std::numeric_limits<int>::max();
                std::pair<std::size_t, std::size_t> best{rooms.size(), rooms.size()};
                for (std::size_t a = 0; a < rooms.size(); ++a)
                    for (std::size_t b = a + 1; b < rooms.size(); ++b)
                    {
                        if (std::any_of(corridorEdges.begin(), corridorEdges.end(), [&](const auto &edge) {
                                return (edge.first == a && edge.second == b) || (edge.first == b && edge.second == a);
                            }))
                            continue;
                        const int dx = roomCenters[a].x - roomCenters[b].x, dy = roomCenters[a].y - roomCenters[b].y;
                        const int distance = dx * dx + dy * dy;
                        if (distance < bestDistance)
                        {
                            bestDistance = distance;
                            best = {a, b};
                        }
                    }
                if (best.first == rooms.size())
                    break;
                corridorEdges.push_back(best);
            }
        }
        const auto roomBoundaryToward = [](const InteriorRoom &room, const MapCellCoordinate &target) {
            const MapCellCoordinate center = room.center();
            const int dx = target.x - center.x;
            const int dy = target.y - center.y;
            if (std::abs(dx) * std::max(1, room.height) >= std::abs(dy) * std::max(1, room.width))
                return MapCellCoordinate{dx >= 0 ? room.x + room.width - 1 : room.x, center.y};
            return MapCellCoordinate{center.x, dy >= 0 ? room.y + room.height - 1 : room.y};
        };
        const auto roomExitAlongCorridor = [&](const InteriorRoom &room, MapCellCoordinate start,
                                               MapCellCoordinate elbow, MapCellCoordinate end) {
            MapCellCoordinate current = start;
            for (const MapCellCoordinate target : {elbow, end})
                while (current.x != target.x || current.y != target.y)
                {
                    const MapCellCoordinate previous = current;
                    if (current.x != target.x)
                        current.x += current.x < target.x ? 1 : -1;
                    else
                        current.y += current.y < target.y ? 1 : -1;
                    if (!room.contains(current.x, current.y))
                        return options.generatorVersion >= 11 ? current : previous;
                }
            return start;
        };
        for (std::size_t corridorIndex = 0; corridorIndex < corridorEdges.size(); ++corridorIndex)
        {
            const std::size_t fromIndex = corridorEdges[corridorIndex].first;
            const std::size_t toIndex = corridorEdges[corridorIndex].second;
            const MapCellCoordinate from = roomCenters[fromIndex];
            const MapCellCoordinate to = roomCenters[toIndex];
            const bool horizontalFirst =
                ((options.seed ^ static_cast<std::uint32_t>(corridorIndex * 0x9e37u)) & 1u) != 0u;
            const MapCellCoordinate elbow =
                horizontalFirst ? MapCellCoordinate{to.x, from.y} : MapCellCoordinate{from.x, to.y};
            mapGenDrawWay(result.map, from.x, from.y, elbow.x, elbow.y, MapWayKind::DungeonCorridor, rng, 0, true,
                          MapTileKind::Room, 0, false);
            mapGenDrawWay(result.map, elbow.x, elbow.y, to.x, to.y, MapWayKind::DungeonCorridor, rng, 0, true,
                          MapTileKind::Room, 0, false);
            const MapCellCoordinate exitDoor = composedDungeon
                                                   ? roomExitAlongCorridor(rooms[fromIndex], from, elbow, to)
                                                   : roomBoundaryToward(rooms[fromIndex], to);
            const MapCellCoordinate entryDoor = composedDungeon ? roomExitAlongCorridor(rooms[toIndex], to, elbow, from)
                                                                : roomBoundaryToward(rooms[toIndex], from);
            mapGenSetCell(result.map, exitDoor.x, exitDoor.y, MapTileKind::Door);
            mapGenSetWay(result.map, exitDoor.x, exitDoor.y, MapWayKind::DungeonCorridor);
            mapGenSetCell(result.map, entryDoor.x, entryDoor.y, MapTileKind::Door);
            mapGenSetWay(result.map, entryDoor.x, entryDoor.y, MapWayKind::DungeonCorridor);
            mapGenAddInteriorGraphFeature(result.map, options.seed, "dungeon_corridor", corridorIndex,
                                          "Dungeon Corridor", "fantasy_neutral.dungeon.corridor", 1.0f, false,
                                          {{static_cast<float>(from.x) + 0.5f, static_cast<float>(from.y) + 0.5f},
                                           {static_cast<float>(elbow.x) + 0.5f, static_cast<float>(elbow.y) + 0.5f},
                                           {static_cast<float>(to.x) + 0.5f, static_cast<float>(to.y) + 0.5f}});
        }
        if (options.generatorVersion >= 11)
        {
            // A doorway belongs in the narrow threshold outside a room. Later
            // corridor junctions can open that threshold; do not leave a door
            // floating in open floor when this happens.
            const auto wall = [&](int x, int y) {
                const auto *c = mapCellAt(result.map, x, y);
                return !c || c->kind == MapTileKind::Wall || c->kind == MapTileKind::Empty;
            };
            for (int y = 0; y < result.map.height; ++y)
                for (int x = 0; x < result.map.width; ++x)
                {
                    const auto *c = mapCellAt(result.map, x, y);
                    if (!c || c->kind != MapTileKind::Door)
                        continue;
                    const bool north = wall(x, y - 1), south = wall(x, y + 1), west = wall(x - 1, y),
                               east = wall(x + 1, y);
                    if (!((north && south && !west && !east) || (west && east && !north && !south)))
                        mapGenSetCell(result.map, x, y, MapTileKind::Room);
                    else
                    {
                        // Two room thresholds can meet in a two-cell passage.
                        // Keep one gate instead of two doors with no vestibule.
                        const auto *left = mapCellAt(result.map, x - 1, y);
                        const auto *above = mapCellAt(result.map, x, y - 1);
                        if ((left && left->kind == MapTileKind::Door) || (above && above->kind == MapTileKind::Door))
                            mapGenSetCell(result.map, x, y, MapTileKind::Room);
                    }
                }
        }
        if (!roomCenters.empty())
        {
            std::pair<std::size_t, std::size_t> farthest{0, 0};
            int farthestDistance = -1;
            for (std::size_t first = 0; first < roomCenters.size(); ++first)
                for (std::size_t second = first + 1; second < roomCenters.size(); ++second)
                {
                    const int dx = roomCenters[first].x - roomCenters[second].x;
                    const int dy = roomCenters[first].y - roomCenters[second].y;
                    const int distance = dx * dx + dy * dy;
                    if (distance > farthestDistance)
                    {
                        farthestDistance = distance;
                        farthest = {first, second};
                    }
                }
            mapGenSetCell(result.map, roomCenters[farthest.first].x, roomCenters[farthest.first].y,
                          MapTileKind::Stairs);
            mapGenSetCell(result.map, roomCenters[farthest.second].x, roomCenters[farthest.second].y,
                          MapTileKind::Stairs);
        }
        mapGenReportProgress(options, 3, totalSteps, "Connecting structure entrances");
        if (!composedDungeon)
            mapGenConnectStructuresToWays(result.map, options, rng);
        mapGenReportProgress(options, 4, totalSteps, "Placing tactical features");
        mapGenBuildTacticalFeatures(result.map, options, rng);
        if (composedDungeon)
            mapComposeDungeonFurnitureV12(result.map, options.generatorVersion, options.seed);
    }
    else if (options.generatorVersion >= 3 && options.scope == MapScope::Local &&
             options.purpose == MapGenerationPurpose::SettlementEnvirons)
    {
        mapGenV3BuildSettlement(result, options, rng, totalSteps);
    }
    else if (options.generatorVersion >= 2)
    {
        mapGenV2BuildOverworld(result, options, rng, totalSteps);
    }
    else
    {
        mapGenClear(result.map, MapTileKind::Empty);
        mapGenReportProgress(options, 2, totalSteps, "Building mountains");
        mapGenBuildMountains(result.map, options, rng);
        mapGenReportProgress(options, 3, totalSteps, "Building water and river routes");
        mapGenBuildWater(result.map, options, rng);
        mapGenReportProgress(options, 4, totalSteps, "Placing forests and settlements");
        mapGenBuildForestsAndSettlements(result.map, options, rng);
        mapGenReportProgress(options, 5, totalSteps, "Drawing primary routes");
        mapGenBuildRoutes(result.map, options, rng);
        mapGenReportProgress(options, 6, totalSteps, "Connecting structure entrances");
        mapGenConnectStructuresToWays(result.map, options, rng);
        mapGenReportProgress(options, 7, totalSteps, "Extending route network");
        mapGenAddSameFamilyRoadBranches(result.map, options, rng);
        mapGenReportProgress(options, 8, totalSteps, "Placing tactical features");
        mapGenBuildTacticalFeatures(result.map, options, rng);
        mapGenReportProgress(options, 9, totalSteps, "Rechecking structure entrances");
        mapGenConnectStructuresToWays(result.map, options, rng);
        mapGenReportProgress(options, 10, totalSteps, "Normalizing generated art contracts");
        mapGenNormalizeGeneratedArtContractWays(result.map);
        mapGenClearIsolatedWays(result.map, MapWayKind::River);
    }

    mapGenEnsureGenerationMetadata(result.map, options);
    mapGenApplyRequiredFeatures(result.map, options);
    mapGenRestorePreservedFeatures(result.map, options);

    mapGenReportProgress(options, totalSteps - 1, totalSteps, "Checking style coverage");
    const std::vector<MapTileKind> palette = paletteForMapScope(options.scope);
    for (MapTileKind kind : palette)
    {
        if (!mapStyleSupportsTileKind(options.styleId, kind))
            result.warnings.push_back(std::string("Style lacks explicit coverage for ") + mapTileKindLabel(kind));
    }
    mapGenReportProgress(options, totalSteps, totalSteps, "Complete");
    return result;
}
