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

int mapGenV2CompositionPercent(const MapModel &map)
{
    if (map.width <= 0 || map.height <= 0)
        return 0;

    bool occupied[3][3] = {};
    int signalCells = 0;
    double signalX = 0.0;
    double signalY = 0.0;
    const auto addSignal = [&](float x, float y) {
        if (x < 0.0f || y < 0.0f || x >= static_cast<float>(map.width) || y >= static_cast<float>(map.height))
            return;
        const int sectorX = std::clamp(static_cast<int>(x * 3.0f / static_cast<float>(map.width)), 0, 2);
        const int sectorY = std::clamp(static_cast<int>(y * 3.0f / static_cast<float>(map.height)), 0, 2);
        occupied[sectorY][sectorX] = true;
        ++signalCells;
        signalX += x;
        signalY += y;
    };

    // Cell semantics remain useful for legacy and raster-only maps.  Continuous
    // maps additionally contribute their feature graph below, which avoids
    // rating a compact city or a coast with deliberate negative space as
    // "empty" merely because it does not fill all nine absolute sectors.
    int occupiedSectors = 0;
    for (int sectorY = 0; sectorY < 3; ++sectorY)
    {
        for (int sectorX = 0; sectorX < 3; ++sectorX)
        {
            const int minX = sectorX * map.width / 3;
            const int maxX = (sectorX + 1) * map.width / 3;
            const int minY = sectorY * map.height / 3;
            const int maxY = (sectorY + 1) * map.height / 3;
            int interest = 0;
            double sectorSignalX = 0.0;
            double sectorSignalY = 0.0;
            for (int y = minY; y < maxY; ++y)
            {
                for (int x = minX; x < maxX; ++x)
                {
                    const MapCellModel *cell = mapCellAt(map, x, y);
                    if (cell && (cell->kind == MapTileKind::Settlement || cell->kind == MapTileKind::Landmark ||
                                 cell->kind == MapTileKind::Mountain || cell->wayKind != MapWayKind::None))
                    {
                        ++interest;
                        sectorSignalX += static_cast<double>(x) + 0.5;
                        sectorSignalY += static_cast<double>(y) + 0.5;
                    }
                }
            }
            if (interest >= std::max(1, (maxX - minX) * (maxY - minY) / 35))
            {
                occupied[sectorY][sectorX] = true;
                signalCells += interest;
                signalX += sectorSignalX;
                signalY += sectorSignalY;
            }
        }
    }

    // Graph nodes are the visible compositional anchors at overview scales.
    // Sample all control points: a long road or river spanning a quiet sector
    // is meaningful even where its one-cell raster projection is sparse.
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if (feature.kind != MapGeneratedFeatureKind::Settlement && feature.kind != MapGeneratedFeatureKind::Landmark &&
            feature.kind != MapGeneratedFeatureKind::MountainRange && feature.kind != MapGeneratedFeatureKind::River &&
            feature.kind != MapGeneratedFeatureKind::Road && feature.kind != MapGeneratedFeatureKind::Trail &&
            feature.kind != MapGeneratedFeatureKind::District && feature.kind != MapGeneratedFeatureKind::TerrainRegion)
            continue;
        if (feature.kind == MapGeneratedFeatureKind::TerrainRegion &&
            feature.materialId.rfind("fantasy_neutral.territory.", 0) == 0)
            continue;
        for (const MapFeaturePoint &point : feature.points)
            addSignal(point.x, point.y);
    }

    for (int sectorY = 0; sectorY < 3; ++sectorY)
        for (int sectorX = 0; sectorX < 3; ++sectorX)
            occupiedSectors += occupied[sectorY][sectorX] ? 1 : 0;

    // Three deliberately placed sectors already provide a readable
    // foreground/midground/background relationship. Four through eight give
    // broad coverage while preserving useful negative space; filling every
    // sector remains valid but is not automatically superior.
    static constexpr int coverageScore[10] = {0, 42, 66, 84, 94, 100, 100, 100, 98, 94};
    int score = coverageScore[std::clamp(occupiedSectors, 0, 9)];
    if (signalCells > 0)
    {
        const double centerX = signalX / static_cast<double>(signalCells) / static_cast<double>(map.width);
        const double centerY = signalY / static_cast<double>(signalCells) / static_cast<double>(map.height);
        const double edgeDistance = std::min({centerX, 1.0 - centerX, centerY, 1.0 - centerY});
        if (edgeDistance < 0.12)
            score -= static_cast<int>(std::round((0.12 - edgeDistance) * 125.0));
    }
    if (map.generation.recipe.generatorVersion >= 5 && map.atlasLevel == MapLevel::World)
    {
        score = std::min(score, mapGenV5WorldTerritoryGeographyPercent(map));
        score = std::min(score, mapGenV5WorldSettlementSpreadPercent(map));
        score = std::min(score, mapGenV5WorldMountainCompositionPercent(map));
        score = std::min(score, mapGenV5WorldCoastCompositionPercent(map));
        score = std::min(score, mapGenV5WorldBiomeCompositionPercent(map));
    }
    return std::clamp(score, 0, 100);
}

int mapGenV4ScaleCorrectnessPercent(const MapModel &map)
{
    int incompatible = 0;
    int considered = 0;
    for (const MapCellModel &cell : map.cells)
    {
        const bool tactical = cell.kind == MapTileKind::Cover || cell.kind == MapTileKind::Hazard ||
                              cell.kind == MapTileKind::Pit || cell.kind == MapTileKind::Rubble;
        const bool individualBuilding = cell.kind == MapTileKind::Building;
        if (tactical || individualBuilding)
        {
            ++considered;
            if (map.atlasLevel == MapLevel::World || (map.atlasLevel == MapLevel::Region && tactical))
                ++incompatible;
        }
    }
    if (considered == 0)
        return 100;
    return std::clamp(100 - incompatible * 100 / considered, 0, 100);
}

int mapGenV4VisualHierarchyPercent(const MapModel &map)
{
    int majorFeatures = 0;
    int detailCells = 0;
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
        if (feature.kind == MapGeneratedFeatureKind::Settlement || feature.kind == MapGeneratedFeatureKind::Landmark ||
            feature.kind == MapGeneratedFeatureKind::River || feature.kind == MapGeneratedFeatureKind::Road ||
            feature.kind == MapGeneratedFeatureKind::District)
            ++majorFeatures;
    // Tactical composition is intentionally represented by named regions:
    // deployment zones, objectives, routes and chokepoints. Ignoring these
    // made a fully prepared encounter score 75 solely because it did not also
    // duplicate those concepts into the render feature graph.
    for (const MapRegionModel &region : map.regions)
        if (!region.name.empty() && region.width > 0 && region.height > 0)
            ++majorFeatures;
    const bool footprintBuildings =
        map.atlasLevel == MapLevel::Tactical && map.generation.recipe.generatorVersion >= 10;
    for (const MapCellModel &cell : map.cells)
        if ((!footprintBuildings && cell.kind == MapTileKind::Building) || cell.kind == MapTileKind::Cover ||
            cell.kind == MapTileKind::Rubble || cell.kind == MapTileKind::Hazard)
            ++detailCells;
    if (footprintBuildings)
    {
        // V10 renders one roof over each actual building footprint. Counting
        // each blocked cell as an independent stamp penalizes coherent urban
        // frontage and rewards invisible tiny icons over large obstacles.
        // Isolated building stamps still count separately; walkability and
        // structure access remain independent non-negotiable quality gates.
        std::vector<unsigned char> visited(map.cells.size(), 0);
        for (int y = 0; y < map.height; ++y)
            for (int x = 0; x < map.width; ++x)
            {
                const auto *cell = mapCellAt(map, x, y);
                const auto index = static_cast<std::size_t>(mapCellIndex(map, x, y));
                if (!cell || cell->kind != MapTileKind::Building || visited[index])
                    continue;
                ++detailCells;
                std::queue<MapCellCoordinate> pending;
                pending.push({x, y});
                visited[index] = 1;
                while (!pending.empty())
                {
                    const auto point = pending.front();
                    pending.pop();
                    for (auto delta : {MapCellCoordinate{1, 0}, MapCellCoordinate{-1, 0}, MapCellCoordinate{0, 1},
                                       MapCellCoordinate{0, -1}})
                    {
                        const int nx = point.x + delta.x, ny = point.y + delta.y;
                        const auto *neighbor = mapCellAt(map, nx, ny);
                        if (!neighbor || neighbor->kind != MapTileKind::Building)
                            continue;
                        const auto next = static_cast<std::size_t>(mapCellIndex(map, nx, ny));
                        if (!visited[next])
                        {
                            visited[next] = 1;
                            pending.push({nx, ny});
                        }
                    }
                }
            }
    }
    const int area = std::max(1, map.width * map.height);
    const int allowedDetail =
        map.atlasLevel == MapLevel::World ? area / 80 : (map.atlasLevel == MapLevel::Region ? area / 35 : area / 8);
    const int clutterPenalty =
        std::max(0, detailCells - allowedDetail) * 100 / std::max(1, allowedDetail + detailCells);
    const int focusPenalty = majorFeatures == 0 && map.scope != MapScope::Dungeon ? 25 : 0;
    return std::clamp(100 - clutterPenalty - focusPenalty, 0, 100);
}

const char *mapGenV4TerrainAssetSemantic(MapTileKind kind)
{
    switch (kind)
    {
    case MapTileKind::Water:
        return "water_shallow";
    case MapTileKind::Dirt:
        return "earth";
    case MapTileKind::Stone:
    case MapTileKind::Room:
        return "dressed_stone_floor";
    case MapTileKind::Cave:
        return "cave_floor";
    case MapTileKind::Forest:
        return "forest_floor";
    case MapTileKind::Swamp:
        return "mud";
    case MapTileKind::Road:
        return "gravel";
    default:
        return "grass_temperate";
    }
}

int mapGenV4AssetCoveragePercent(const MapModel &map)
{
    if (!mapStyleProductionSnapshotAvailable(map.generation.recipe.styleSnapshotId))
        return 0;
    struct Requirement
    {
        std::string role;
        std::string semantic;
        int x = 0;
        int y = 0;
    };
    std::vector<Requirement> requirements;
    auto require = [&](const std::string &role, const std::string &semantic, int x, int y) {
        const auto found = std::find_if(requirements.begin(), requirements.end(), [&](const Requirement &item) {
            return item.role == role && item.semantic == semantic;
        });
        if (found == requirements.end())
            requirements.push_back({role, semantic, x, y});
    };
    std::vector<MapTileKind> terrainKinds;
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell)
                continue;
            const bool terrain = cell->kind != MapTileKind::Settlement && cell->kind != MapTileKind::Building &&
                                 cell->kind != MapTileKind::Landmark && cell->kind != MapTileKind::Bridge &&
                                 cell->kind != MapTileKind::Cover && cell->kind != MapTileKind::Rubble &&
                                 cell->kind != MapTileKind::Hazard && cell->kind != MapTileKind::Wall &&
                                 cell->kind != MapTileKind::Door && cell->kind != MapTileKind::Stairs;
            if (terrain)
            {
                // The accepted forest_floor atlas is a close-up regional fern
                // texture. At World scale forests are semantic biome tinting
                // over the neutral macro land material, never that tactical
                // ground asset (and never per-cell bushes or trees).
                const char *terrainSemantic = map.atlasLevel == MapLevel::World && cell->kind == MapTileKind::Forest
                                                  ? "grass_temperate"
                                                  : mapGenV4TerrainAssetSemantic(cell->kind);
                require("terrain_material", terrainSemantic, x, y);
                if (std::find(terrainKinds.begin(), terrainKinds.end(), cell->kind) == terrainKinds.end())
                    terrainKinds.push_back(cell->kind);
            }
            if (cell->kind == MapTileKind::Forest && map.atlasLevel != MapLevel::World)
                require("vegetation_stamp", "forest_edge", x, y);
            if (cell->kind == MapTileKind::Mountain)
                require("landscape_stamp", map.atlasLevel == MapLevel::World ? "connected_ridge" : "peak", x, y);
            if (cell->kind == MapTileKind::Settlement)
            {
                const bool overview = map.atlasLevel == MapLevel::World || map.atlasLevel == MapLevel::Region;
                std::string overviewSemantic = "town";
                if (overview)
                {
                    for (const MapGeneratedFeature &feature : map.generation.graph.features)
                    {
                        if (feature.kind != MapGeneratedFeatureKind::Settlement || feature.points.empty() ||
                            static_cast<int>(std::floor(feature.points.front().x)) != x ||
                            static_cast<int>(std::floor(feature.points.front().y)) != y)
                            continue;
                        if (feature.id == "brief_capital" || feature.materialId.find("capital") != std::string::npos)
                            overviewSemantic = "capital";
                        else if (feature.id.rfind("brief_city_", 0) == 0 || feature.locked)
                            overviewSemantic = "city";
                        break;
                    }
                }
                require(overview ? "map_symbol" : "settlement_stamp",
                        overview ? overviewSemantic : mapSettlementVisualSemanticRole(map, cell->kind), x, y);
            }
            if (cell->kind == MapTileKind::Building &&
                (map.atlasLevel == MapLevel::Site || map.atlasLevel == MapLevel::Tactical))
                require("settlement_stamp", mapSettlementVisualSemanticRole(map, cell->kind), x, y);
            if (cell->kind == MapTileKind::Landmark && map.atlasLevel != MapLevel::World)
                require("map_symbol", "discovery", x, y);
            if (map.atlasLevel == MapLevel::Tactical &&
                (cell->kind == MapTileKind::Cover || cell->kind == MapTileKind::Rubble ||
                 cell->kind == MapTileKind::Hazard))
                require("tactical_prop",
                        cell->kind == MapTileKind::Rubble
                            ? "rubble"
                            : (cell->kind == MapTileKind::Hazard ? "barricade" : "well_log_cover"),
                        x, y);
            if (map.scope == MapScope::Dungeon &&
                (cell->kind == MapTileKind::Door || cell->kind == MapTileKind::Stairs))
                require("dungeon_stamp", "dungeon_architecture", x, y);
        }
    }
    if (terrainKinds.size() > 1 && map.scope != MapScope::Dungeon)
        require("blend_mask_or_decal", "organic_blend_mask", map.width / 2, map.height / 2);
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if (feature.points.empty())
            continue;
        const int x = static_cast<int>(std::floor(feature.points.front().x));
        const int y = static_cast<int>(std::floor(feature.points.front().y));
        // World routes are deliberately rendered as continuous vector bands.
        // Their graph remains authoritative, but the renderer does not stamp
        // close-range curve-profile sprites at this scale. Region and closer
        // levels still consume the corresponding production assets.
        if (map.atlasLevel != MapLevel::World && feature.kind == MapGeneratedFeatureKind::River)
            require("curve_profile", feature.width >= 1.35f ? "river_deep" : "river", x, y);
        else if (map.atlasLevel != MapLevel::World && feature.kind == MapGeneratedFeatureKind::Trail)
            require("curve_profile", "path", x, y);
        else if (map.atlasLevel != MapLevel::World && feature.kind == MapGeneratedFeatureKind::Road)
            require("curve_profile", feature.materialId.find("stone") != std::string::npos ? "road_stone" : "road_dirt",
                    x, y);
        else if (feature.kind == MapGeneratedFeatureKind::Bridge)
            require("bridge_stamp",
                    feature.materialId.find("stone") != std::string::npos ? "bridge_stone_rough" : "bridge_wood_light",
                    x, y);
    }
    if (requirements.empty())
        return 100;
    int resolved = 0;
    for (std::size_t index = 0; index < requirements.size(); ++index)
    {
        const Requirement &requirement = requirements[index];
        if (resolveMapStyleProductionAssetForSnapshot(map.generation.recipe.styleSnapshotId, map.atlasLevel,
                                                      requirement.role, requirement.semantic, requirement.x,
                                                      requirement.y, static_cast<int>(index)))
            ++resolved;
    }
    return resolved * 100 / static_cast<int>(requirements.size());
}

