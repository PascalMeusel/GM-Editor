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

bool mapGenCanHostTacticalFeature(const MapCellModel &cell, MapScope scope)
{
    if (cell.wayKind != MapWayKind::None || cell.movementCost >= 99 || cell.kind == MapTileKind::Water ||
        cell.kind == MapTileKind::Lava || cell.kind == MapTileKind::Chasm)
        return false;
    if (cell.kind == MapTileKind::Settlement || cell.kind == MapTileKind::Building ||
        cell.kind == MapTileKind::Bridge || cell.kind == MapTileKind::Stairs || cell.kind == MapTileKind::Door)
        return false;
    if (scope == MapScope::Dungeon)
        return cell.kind == MapTileKind::Room || cell.kind == MapTileKind::Cave || cell.kind == MapTileKind::Stone;
    return cell.kind == MapTileKind::Empty || cell.kind == MapTileKind::Dirt || cell.kind == MapTileKind::Forest ||
           cell.kind == MapTileKind::Hill || cell.kind == MapTileKind::Stone || cell.kind == MapTileKind::Desert ||
           cell.kind == MapTileKind::Snow || cell.kind == MapTileKind::Swamp;
}

MapTileKind mapGenRandomTacticalFeature(MapScope scope, std::mt19937 &rng)
{
    if (scope == MapScope::Dungeon)
    {
        const int roll = mapGenRandomBetween(rng, 1, 100);
        if (roll <= 45)
            return MapTileKind::Cover;
        if (roll <= 72)
            return MapTileKind::Rubble;
        if (roll <= 90)
            return MapTileKind::Hazard;
        return MapTileKind::Pit;
    }

    const int roll = mapGenRandomBetween(rng, 1, 100);
    if (roll <= 44)
        return MapTileKind::Cover;
    if (roll <= 68)
        return MapTileKind::Rubble;
    if (roll <= 86)
        return MapTileKind::Hazard;
    return MapTileKind::Landmark;
}

void mapGenBuildTacticalFeatures(MapModel &map, const MapGenerationOptions &options, std::mt19937 &rng)
{
    const int density = mapGenClampPercent(options.tacticalDensity);
    if (density == 0)
        return;

    const int area = std::max(1, map.width * map.height);
    const int featureCount = std::max(1, area * density / (options.scope == MapScope::World ? 4200 : 1900));
    for (int i = 0; i < featureCount; ++i)
    {
        for (int attempt = 0; attempt < 60; ++attempt)
        {
            const int x = mapGenRandomBetween(rng, 1, map.width - 2);
            const int y = mapGenRandomBetween(rng, 1, map.height - 2);
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell || !mapGenCanHostTacticalFeature(*cell, options.scope))
                continue;
            if (options.scope == MapScope::Dungeon && ((options.generatorVersion >= 10 && !options.caveNetwork) ||
                                                       (options.generatorVersion >= 11 && options.caveNetwork)))
            {
                // Props have a visual footprint wider than one cell. Keep a
                // full floor margin so they neither erase boundary wall ink
                // nor protrude into the dungeon void or conceal a doorway.
                bool interior = true;
                for (int dy = -1; dy <= 1 && interior; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        const auto *neighbor = mapCellAt(map, x + dx, y + dy);
                        if (!neighbor ||
                            neighbor->kind != (options.caveNetwork ? MapTileKind::Cave : MapTileKind::Room))
                        {
                            interior = false;
                            break;
                        }
                    }
                if (!interior)
                    continue;
            }
            const MapTileKind feature = mapGenRandomTacticalFeature(options.scope, rng);
            if (feature == MapTileKind::Landmark && mapGenNearKind(map, x, y, MapTileKind::Water, 1))
                continue;
            mapGenSetCell(map, x, y, feature);
            break;
        }
    }
}

std::string mapGenInteriorFeatureId(std::uint32_t seed, const char *family, std::size_t index)
{
    return std::string("mapgen_interior_") + family + "_" + std::to_string(seed) + "_" + std::to_string(index);
}

void mapGenAddInteriorGraphFeature(MapModel &map, std::uint32_t seed, const char *family, std::size_t index,
                                   const char *name, const char *materialId, float width, bool closed,
                                   std::vector<MapFeaturePoint> points)
{
    if (points.empty())
        return;
    MapGeneratedFeature feature;
    feature.id = mapGenInteriorFeatureId(seed, family, index);
    feature.kind = MapGeneratedFeatureKind::TerrainRegion;
    feature.name = name;
    feature.materialId = materialId;
    feature.width = width;
    feature.closed = closed;
    feature.points = std::move(points);
    map.generation.graph.features.push_back(std::move(feature));
}
