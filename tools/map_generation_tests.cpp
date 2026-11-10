#include "Models/Map/MapGeneration.hpp"
#include "Models/Map/MapMountainPassRouting.hpp"
#include "Models/Map/MapRoadClearance.hpp"
#include "Models/Map/MapOverviewRenderPolicy.hpp"
#include "Models/Map/MapRuntimeVisualAssets.hpp"
#include "Models/Map/MapTileSemantics.hpp"
#include "Models/Map/MapWorldBrief.hpp"
#include "tools/HeadlessDebugDiagnostics.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace
{
bool require(bool condition, const std::string &message)
{
    if (condition)
        return true;

    std::cerr << message << std::endl;
    return false;
}

int countTerrain(const MapModel &map, MapTileKind kind)
{
    int count = 0;
    for (const MapCellModel &cell : map.cells)
    {
        if (cell.kind == kind)
            ++count;
    }
    return count;
}

int countWays(const MapModel &map, MapWayKind kind)
{
    int count = 0;
    for (const MapCellModel &cell : map.cells)
    {
        if (cell.wayKind == kind)
            ++count;
    }
    return count;
}

int countGeneratedFeaturesWithMaterial(const MapModel &map, const std::string &materialId)
{
    return static_cast<int>(std::count_if(map.generation.graph.features.begin(), map.generation.graph.features.end(),
                                          [&](const MapGeneratedFeature &feature) { return feature.materialId == materialId; }));
}

int countOverlappingInteriorRegions(const MapModel &map, const std::string &materialId)
{
    struct Bounds
    {
        float left = 0.0f;
        float top = 0.0f;
        float right = 0.0f;
        float bottom = 0.0f;
    };
    std::vector<Bounds> bounds;
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if (feature.materialId != materialId || feature.points.empty())
            continue;
        Bounds value{feature.points.front().x, feature.points.front().y,
                     feature.points.front().x, feature.points.front().y};
        for (const MapFeaturePoint &point : feature.points)
        {
            value.left = std::min(value.left, point.x);
            value.top = std::min(value.top, point.y);
            value.right = std::max(value.right, point.x);
            value.bottom = std::max(value.bottom, point.y);
        }
        bounds.push_back(value);
    }
    int overlaps = 0;
    for (std::size_t first = 0; first < bounds.size(); ++first)
        for (std::size_t second = first + 1; second < bounds.size(); ++second)
            if (std::min(bounds[first].right, bounds[second].right) >
                    std::max(bounds[first].left, bounds[second].left) &&
                std::min(bounds[first].bottom, bounds[second].bottom) >
                    std::max(bounds[first].top, bounds[second].top))
                ++overlaps;
    return overlaps;
}

bool generatedFeatureGraphsEqual(const MapFeatureGraph &left, const MapFeatureGraph &right)
{
    if (left.schemaVersion != right.schemaVersion || left.features.size() != right.features.size())
        return false;
    for (std::size_t index = 0; index < left.features.size(); ++index)
    {
        const MapGeneratedFeature &a = left.features[index];
        const MapGeneratedFeature &b = right.features[index];
        if (a.id != b.id || a.kind != b.kind || a.name != b.name || a.materialId != b.materialId ||
            a.width != b.width || a.closed != b.closed || a.locked != b.locked ||
            a.discovered != b.discovered || a.gmOnly != b.gmOnly || a.points.size() != b.points.size())
        {
            std::cerr << "Feature graph mismatch at " << index << ": " << a.id << " / " << b.id
                      << " name=" << a.name << " / " << b.name << " material=" << a.materialId << " / " << b.materialId
                      << " points=" << a.points.size() << " / " << b.points.size() << '\n';
            return false;
        }
        for (std::size_t pointIndex = 0; pointIndex < a.points.size(); ++pointIndex)
        {
            if (a.points[pointIndex].x != b.points[pointIndex].x || a.points[pointIndex].y != b.points[pointIndex].y)
            {
                std::cerr << "Feature graph coordinate mismatch at " << a.id << '[' << pointIndex << "]: "
                          << a.points[pointIndex].x << ',' << a.points[pointIndex].y << " / "
                          << b.points[pointIndex].x << ',' << b.points[pointIndex].y << '\n';
                return false;
            }
        }
    }
    return true;
}

bool isGeneratedRoadWay(MapWayKind kind)
{
    return kind == MapWayKind::Trail || kind == MapWayKind::DirtRoad || kind == MapWayKind::StoneRoad;
}

int countRoadWays(const MapModel &map)
{
    int count = 0;
    for (const MapCellModel &cell : map.cells)
    {
        if (isGeneratedRoadWay(cell.wayKind))
            ++count;
    }
    return count;
}

int countRoadWaysOnTerrain(const MapModel &map, MapTileKind kind)
{
    int count = 0;
    for (const MapCellModel &cell : map.cells)
    {
        if (isGeneratedRoadWay(cell.wayKind) && cell.kind == kind)
            ++count;
    }
    return count;
}

int countDistinctGeneratedRoadKinds(const MapModel &map)
{
    bool hasTrail = false;
    bool hasDirt = false;
    bool hasStone = false;
    for (const MapCellModel &cell : map.cells)
    {
        hasTrail = hasTrail || cell.wayKind == MapWayKind::Trail;
        hasDirt = hasDirt || cell.wayKind == MapWayKind::DirtRoad;
        hasStone = hasStone || cell.wayKind == MapWayKind::StoneRoad;
    }
    return (hasTrail ? 1 : 0) + (hasDirt ? 1 : 0) + (hasStone ? 1 : 0);
}

int countTacticalFeatures(const MapModel &map)
{
    int count = 0;
    for (const MapCellModel &cell : map.cells)
    {
        if (cell.kind == MapTileKind::Cover || cell.kind == MapTileKind::Rubble || cell.kind == MapTileKind::Hazard || cell.kind == MapTileKind::Pit || cell.kind == MapTileKind::Landmark)
            ++count;
    }
    return count;
}

bool isStructureLike(MapTileKind kind)
{
    return kind == MapTileKind::Settlement || kind == MapTileKind::Building || kind == MapTileKind::Landmark;
}

bool hasGeneratedStructureApproach(const MapModel &map, int x, int y)
{
    const MapCellCoordinate neighbors[] = {
        {x, y - 1},
        {x + 1, y},
        {x, y + 1},
        {x - 1, y}};
    for (const MapCellCoordinate &neighbor : neighbors)
    {
        const MapCellModel *cell = mapCellAt(map, neighbor.x, neighbor.y);
        if (cell && (isGeneratedRoadWay(cell->wayKind) || mapWayKindIsInteriorLike(cell->wayKind) || cell->kind == MapTileKind::Road || cell->kind == MapTileKind::Bridge))
            return true;
    }
    return false;
}

int countStructuresWithoutApproach(const MapModel &map)
{
    int count = 0;
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (cell && isStructureLike(cell->kind) && !hasGeneratedStructureApproach(map, x, y))
                ++count;
        }
    }
    return count;
}

bool hasWaterNeighbor(const MapModel &map, int x, int y)
{
    for (int dy = -1; dy <= 1; ++dy)
    {
        for (int dx = -1; dx <= 1; ++dx)
        {
            if (dx == 0 && dy == 0)
                continue;
            const MapCellModel *neighbor = mapCellAt(map, x + dx, y + dy);
            if (neighbor && neighbor->kind == MapTileKind::Water)
                return true;
        }
    }
    return false;
}

int countWaterAdjacentStructures(const MapModel &map)
{
    int count = 0;
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (cell && isStructureLike(cell->kind) && hasWaterNeighbor(map, x, y))
                ++count;
        }
    }
    return count;
}

bool hasCardinalTrailNeighbor(const MapModel &map, int x, int y)
{
    const MapCellCoordinate neighbors[] = {
        {x, y - 1},
        {x + 1, y},
        {x, y + 1},
        {x - 1, y}};
    for (const MapCellCoordinate &neighbor : neighbors)
    {
        const MapCellModel *cell = mapCellAt(map, neighbor.x, neighbor.y);
        if (cell && cell->wayKind == MapWayKind::Trail)
            return true;
    }
    return false;
}

int countTrailAdjacentStructures(const MapModel &map)
{
    int count = 0;
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (cell && isStructureLike(cell->kind) && hasCardinalTrailNeighbor(map, x, y))
                ++count;
        }
    }
    return count;
}

int countTrailBridges(const MapModel &map)
{
    int count = 0;
    for (const MapCellModel &cell : map.cells)
    {
        if (cell.kind == MapTileKind::Bridge && cell.wayKind == MapWayKind::Trail)
            ++count;
    }
    return count;
}

bool isBridgeApproachContractCell(const MapCellModel &cell)
{
    return isGeneratedRoadWay(cell.wayKind) || cell.kind == MapTileKind::Road || cell.kind == MapTileKind::Dirt || cell.kind == MapTileKind::Stone || cell.kind == MapTileKind::Settlement;
}

bool isBridgeCrossingContractCell(const MapCellModel &cell)
{
    return cell.kind == MapTileKind::Water || cell.kind == MapTileKind::Swamp || mapWayKindIsWaterLike(cell.wayKind);
}

struct BridgeContractGapCounts
{
    int missingApproachComponents = 0;
    int missingCrossingComponents = 0;
};

void addUniqueTestCoordinate(std::vector<MapCellCoordinate> &coordinates, MapCellCoordinate coordinate)
{
    const auto found = std::find_if(coordinates.begin(), coordinates.end(), [&](const MapCellCoordinate &existing) {
        return existing.x == coordinate.x && existing.y == coordinate.y;
    });
    if (found == coordinates.end())
        coordinates.push_back(coordinate);
}

BridgeContractGapCounts countBridgeContractGaps(const MapModel &map)
{
    BridgeContractGapCounts gaps;
    std::vector<bool> visited(map.cells.size(), false);
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const int startIndex = y * map.width + x;
            const MapCellModel *start = mapCellAt(map, x, y);
            if (!start || start->kind != MapTileKind::Bridge || visited[static_cast<size_t>(startIndex)])
                continue;

            bool hasCrossingContext = false;
            std::vector<MapCellCoordinate> approachEndpoints;
            std::queue<MapCellCoordinate> pending;
            pending.push({x, y});
            visited[static_cast<size_t>(startIndex)] = true;

            while (!pending.empty())
            {
                const MapCellCoordinate current = pending.front();
                pending.pop();
                const MapCellCoordinate neighbors[] = {
                    {current.x, current.y - 1},
                    {current.x + 1, current.y},
                    {current.x, current.y + 1},
                    {current.x - 1, current.y}};
                for (const MapCellCoordinate &neighbor : neighbors)
                {
                    const MapCellModel *neighborCell = mapCellAt(map, neighbor.x, neighbor.y);
                    if (!neighborCell)
                        continue;

                    if (neighborCell->kind == MapTileKind::Bridge)
                    {
                        const int neighborIndex = neighbor.y * map.width + neighbor.x;
                        if (!visited[static_cast<size_t>(neighborIndex)])
                        {
                            visited[static_cast<size_t>(neighborIndex)] = true;
                            pending.push(neighbor);
                        }
                        continue;
                    }

                    if (isBridgeApproachContractCell(*neighborCell))
                        addUniqueTestCoordinate(approachEndpoints, neighbor);
                    if (isBridgeCrossingContractCell(*neighborCell))
                        hasCrossingContext = true;
                }
            }

            if (approachEndpoints.size() < 2)
                ++gaps.missingApproachComponents;
            if (!hasCrossingContext)
                ++gaps.missingCrossingComponents;
        }
    }
    return gaps;
}

int countStructureLikeTiles(const MapModel &map)
{
    int count = 0;
    for (const MapCellModel &cell : map.cells)
    {
        if (isStructureLike(cell.kind))
            ++count;
    }
    return count;
}

bool hasDiagonalWayConnection(const MapModel &map, MapWayKind kind)
{
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell || cell->wayKind != kind)
                continue;

            const MapWayRenderContext context = makeMapWayRenderContext(
                cell,
                x,
                y,
                mapCellAt(map, x, y - 1),
                mapCellAt(map, x + 1, y),
                mapCellAt(map, x, y + 1),
                mapCellAt(map, x - 1, y),
                mapCellAt(map, x + 1, y - 1),
                mapCellAt(map, x + 1, y + 1),
                mapCellAt(map, x - 1, y + 1),
                mapCellAt(map, x - 1, y - 1));
            if ((mapWayEightNeighborMask(context) & (2 | 8 | 32 | 128)) != 0)
                return true;
        }
    }
    return false;
}

int countIsolatedWayCells(const MapModel &map, MapWayKind kind)
{
    int count = 0;
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell || cell->wayKind != kind)
                continue;

            bool connected = false;
            for (int dy = -1; dy <= 1 && !connected; ++dy)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    if (dx == 0 && dy == 0)
                        continue;
                    const MapCellModel *neighbor = mapCellAt(map, x + dx, y + dy);
                    if (neighbor && mapWayKindsConnect(kind, neighbor->wayKind))
                    {
                        connected = true;
                        break;
                    }
                }
            }
            if (!connected)
                ++count;
        }
    }
    return count;
}

bool hasDiagonalRoadConnection(const MapModel &map)
{
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell || !isGeneratedRoadWay(cell->wayKind))
                continue;

            const MapWayRenderContext context = makeMapWayRenderContext(
                cell,
                x,
                y,
                mapCellAt(map, x, y - 1),
                mapCellAt(map, x + 1, y),
                mapCellAt(map, x, y + 1),
                mapCellAt(map, x - 1, y),
                mapCellAt(map, x + 1, y - 1),
                mapCellAt(map, x + 1, y + 1),
                mapCellAt(map, x - 1, y + 1),
                mapCellAt(map, x - 1, y - 1));
            if ((mapWayEightNeighborMask(context) & (2 | 8 | 32 | 128)) != 0)
                return true;
        }
    }
    return false;
}

bool isRiverInfluenceCell(const MapCellModel *cell)
{
    return cell && (cell->kind == MapTileKind::Water || cell->kind == MapTileKind::Swamp || mapWayKindIsWaterLike(cell->wayKind));
}

struct RiverDiagonalCornerReport
{
    int diagonalPairs = 0;
    int diagonalOnlyPairs = 0;
};

RiverDiagonalCornerReport countRiverDiagonalCornerGaps(const MapModel &map)
{
    RiverDiagonalCornerReport report;
    const MapCellCoordinate diagonals[] = {
        {1, 1},
        {-1, 1}};

    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell || cell->wayKind != MapWayKind::River)
                continue;

            for (const MapCellCoordinate &diagonal : diagonals)
            {
                const MapCellModel *diagonalCell = mapCellAt(map, x + diagonal.x, y + diagonal.y);
                if (!diagonalCell || diagonalCell->wayKind != MapWayKind::River)
                    continue;

                ++report.diagonalPairs;
                if (!isRiverInfluenceCell(mapCellAt(map, x + diagonal.x, y)) &&
                    !isRiverInfluenceCell(mapCellAt(map, x, y + diagonal.y)))
                    ++report.diagonalOnlyPairs;
            }
        }
    }
    return report;
}

bool mountainComponentsHaveSingleVariantMode(const MapModel &map)
{
    std::vector<bool> visited(map.cells.size(), false);
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const int startIndex = y * map.width + x;
            const MapCellModel *start = mapCellAt(map, x, y);
            if (!start || start->kind != MapTileKind::Mountain || visited[static_cast<size_t>(startIndex)])
                continue;

            const MapSymbolVariantMode expectedMode = start->symbolVariantMode;
            std::queue<MapCellCoordinate> pending;
            pending.push({x, y});
            visited[static_cast<size_t>(startIndex)] = true;
            while (!pending.empty())
            {
                const MapCellCoordinate current = pending.front();
                pending.pop();
                const MapCellModel *cell = mapCellAt(map, current.x, current.y);
                if (!cell || cell->symbolVariantMode != expectedMode)
                    return false;

                const MapCellCoordinate neighbors[] = {
                    {current.x, current.y - 1},
                    {current.x + 1, current.y},
                    {current.x, current.y + 1},
                    {current.x - 1, current.y}};
                for (const MapCellCoordinate &neighbor : neighbors)
                {
                    if (!isMapCellInside(map, neighbor.x, neighbor.y))
                        continue;
                    const int neighborIndex = neighbor.y * map.width + neighbor.x;
                    if (visited[static_cast<size_t>(neighborIndex)])
                        continue;
                    const MapCellModel *neighborCell = mapCellAt(map, neighbor.x, neighbor.y);
                    if (neighborCell && neighborCell->kind == MapTileKind::Mountain)
                    {
                        visited[static_cast<size_t>(neighborIndex)] = true;
                        pending.push(neighbor);
                    }
                }
            }
        }
    }
    return true;
}

bool mapsHaveSameCells(const MapModel &left, const MapModel &right)
{
    if (left.width != right.width || left.height != right.height || left.cells.size() != right.cells.size())
        return false;

    for (size_t index = 0; index < left.cells.size(); ++index)
    {
        const MapCellModel &a = left.cells[index];
        const MapCellModel &b = right.cells[index];
        if (a.kind != b.kind || a.wayKind != b.wayKind || a.symbolVariantMode != b.symbolVariantMode)
            return false;
    }
    return true;
}

bool testDeterministicSeed()
{
    MapGenerationOptions options;
    options.width = 32;
    options.height = 24;
    options.seed = 123456u;
    options.waterLevel = 35;
    options.mountainDensity = 34;
    options.forestDensity = 42;
    options.routeDensity = 40;
    options.mountainPattern = MapMountainPattern::Mixed;

    const MapGenerationResult first = generateSemanticMap(options);
    const MapGenerationResult second = generateSemanticMap(options);
    options.seed = 123457u;
    const MapGenerationResult third = generateSemanticMap(options);

    bool ok = true;
    ok &= require(mapsHaveSameCells(first.map, second.map), "Same seed should generate identical semantic cells");
    ok &= require(!mapsHaveSameCells(first.map, third.map), "Different seeds should alter generated semantic cells");
    return ok;
}

bool testDensityControls()
{
    MapGenerationOptions sparse;
    sparse.width = 40;
    sparse.height = 26;
    sparse.seed = 88u;
    sparse.mountainDensity = 4;
    sparse.forestDensity = 5;
    sparse.waterLevel = 0;
    sparse.routeDensity = 0;

    MapGenerationOptions dense = sparse;
    dense.mountainDensity = 70;
    dense.forestDensity = 70;
    dense.waterLevel = 50;

    const MapGenerationResult sparseResult = generateSemanticMap(sparse);
    const MapGenerationResult denseResult = generateSemanticMap(dense);

    bool ok = true;
    ok &= require(countTerrain(denseResult.map, MapTileKind::Mountain) > countTerrain(sparseResult.map, MapTileKind::Mountain), "Mountain density should increase mountain count");
    ok &= require(countTerrain(denseResult.map, MapTileKind::Forest) > countTerrain(sparseResult.map, MapTileKind::Forest), "Forest density should increase forest count");
    ok &= require(countTerrain(denseResult.map, MapTileKind::Water) > countTerrain(sparseResult.map, MapTileKind::Water), "Water level should increase water count");
    return ok;
}

bool testMountainPatterns()
{
    MapGenerationOptions ranges;
    ranges.width = 36;
    ranges.height = 24;
    ranges.seed = 440u;
    ranges.waterLevel = 0;
    ranges.forestDensity = 0;
    ranges.routeDensity = 0;
    ranges.mountainDensity = 60;
    ranges.mountainPattern = MapMountainPattern::Ranges;

    MapGenerationOptions peaks = ranges;
    peaks.mountainPattern = MapMountainPattern::IndividualPeaks;
    MapGenerationOptions mixed = ranges;
    mixed.mountainPattern = MapMountainPattern::Mixed;

    const MapGenerationResult rangeResult = generateSemanticMap(ranges);
    const MapGenerationResult peakResult = generateSemanticMap(peaks);
    const MapGenerationResult mixedResult = generateSemanticMap(mixed);

    int rangeContinuous = 0;
    int peakIndividual = 0;
    for (const MapCellModel &cell : rangeResult.map.cells)
    {
        if (cell.kind == MapTileKind::Mountain && cell.symbolVariantMode == MapSymbolVariantMode::Continuous)
            ++rangeContinuous;
    }
    for (const MapCellModel &cell : peakResult.map.cells)
    {
        if (cell.kind == MapTileKind::Mountain && cell.symbolVariantMode == MapSymbolVariantMode::Individual)
            ++peakIndividual;
    }

    bool ok = true;
    ok &= require(rangeContinuous > 0, "Range mountain pattern should mark continuous mountain cells");
    ok &= require(peakIndividual > 0, "Individual peak pattern should mark individual mountain cells");
    ok &= require(mountainComponentsHaveSingleVariantMode(mixedResult.map), "Mixed mountain generation should not mix range and individual modes inside one connected mountain component");
    return ok;
}

bool testWayGeneration()
{
    MapGenerationOptions options;
    options.width = 30;
    options.height = 20;
    options.seed = 912u;
    options.routeDensity = 70;
    options.roadNetwork = MapRoadNetworkMode::Connected;
    options.waterLevel = 35;

    const MapGenerationResult result = generateSemanticMap(options);

    bool ok = true;
    ok &= require(countRoadWays(result.map) > 0, "Connected road generation should create road-like way cells");
    ok &= require(countDistinctGeneratedRoadKinds(result.map) == 1, "Connected road generation should keep one road family per generated network");
    ok &= require(countWays(result.map, MapWayKind::River) > 0, "Water generation should create river way cells");
    ok &= require(countIsolatedWayCells(result.map, MapWayKind::River) == 0, "Generated river way metadata should not leave isolated cells");
    ok &= require(!result.map.routes.empty(), "Generated way network should include representative travel route metadata");
    ok &= require(hasDiagonalRoadConnection(result.map), "Connected road generation should include diagonal-capable way segments");

    MapGenerationOptions grid = options;
    grid.waterLevel = 0;
    grid.mountainDensity = 0;
    grid.forestDensity = 0;
    grid.settlementDensity = 70;
    grid.roadNetwork = MapRoadNetworkMode::Grid;
    const MapGenerationResult gridResult = generateSemanticMap(grid);
    ok &= require(!gridResult.map.routes.empty(), "Grid road generation should still include representative route metadata");
    ok &= require(countDistinctGeneratedRoadKinds(gridResult.map) == 1, "Grid road generation should not mix stone and dirt road families");
    ok &= require(countTerrain(gridResult.map, MapTileKind::Settlement) > 0, "Grid settlement stress should create structures to connect");
    ok &= require(countStructuresWithoutApproach(gridResult.map) == 0, "Generated settlement structures should receive adjacent route approach semantics");
    return ok;
}

bool testAppDefaultCityGenerationUsesStoneRoadCandidateGround()
{
    MapGenerationOptions options;
    options.scope = MapScope::World;
    options.width = 24;
    options.height = 18;
    options.seed = 240529u;
    options.waterLevel = 20;
    options.mountainDensity = 0;
    options.forestDensity = 10;
    options.settlementDensity = 34;
    options.routeDensity = 28;
    options.tacticalDensity = 18;
    options.mountainPattern = MapMountainPattern::Mixed;
    options.roadNetwork = MapRoadNetworkMode::Grid;

    const MapGenerationResult result = generateSemanticMap(options);

    int stoneRoadCells = 0;
    int integratedStoneRoadCells = 0;
    int candidatePreviewCells = 0;
    int unsupportedGroundCells = 0;
    for (int y = 0; y < result.map.height; ++y)
    {
        for (int x = 0; x < result.map.width; ++x)
        {
            const MapCellModel *cell = mapCellAt(result.map, x, y);
            if (!cell || cell->wayKind != MapWayKind::StoneRoad)
                continue;

            ++stoneRoadCells;
            if (cell->kind != MapTileKind::Road &&
                cell->kind != MapTileKind::Stone &&
                cell->kind != MapTileKind::Bridge)
            {
                ++unsupportedGroundCells;
            }

            const MapStyleContextualAssetRequest request = mapStyleContextualAssetRequestForWayVisual(makeMapWayRenderContext(result.map, x, y));
            if (request.assetClass != MapStyleContextualAssetClass::IntegratedWayGround)
                continue;

            ++integratedStoneRoadCells;
            const MapStyleContextualAssetResolution resolution = resolveMapStyleContextualAsset(request);
            if (request.owner == "way_stone_road:stone_paved" && resolution.hasCandidateAsset)
                ++candidatePreviewCells;
        }
    }

    bool ok = true;
    ok &= require(stoneRoadCells > 0, "App-default City generation should create stone-road way cells");
    ok &= require(unsupportedGroundCells == 0, "App-default City stone roads should sit on stone-paved-compatible ground instead of Empty/Settlement fallback ground");
    ok &= require(integratedStoneRoadCells > 0, "App-default City generation should produce integrated stone-road visual requests");
    ok &= require(candidatePreviewCells == integratedStoneRoadCells, "App-default City stone-road visual requests should resolve to candidate preview atlas assets");
    ok &= require(!result.map.routes.empty(), "App-default City generation should still keep route metadata without relying on it for road visuals");
    return ok;
}

bool testRoadGenerationPreservesGroundContext()
{
    MapGenerationOptions options;
    options.scope = MapScope::Local;
    options.width = 34;
    options.height = 22;
    options.seed = 9191u;
    options.waterLevel = 0;
    options.mountainDensity = 0;
    options.forestDensity = 0;
    options.settlementDensity = 0;
    options.routeDensity = 70;
    options.roadNetwork = MapRoadNetworkMode::Connected;
    options.tacticalDensity = 0;

    const MapGenerationResult result = generateSemanticMap(options);

    bool ok = true;
    ok &= require(countRoadWays(result.map) > 0, "Ground-context road stress should generate road way metadata");
    ok &= require(countTerrain(result.map, MapTileKind::Road) == 0, "Generated dry roads should preserve underlying terrain instead of replacing it with generic Road terrain");
    ok &= require(countRoadWaysOnTerrain(result.map, MapTileKind::Empty) > 0, "Generated dry roads on grass should remain grass-ground way contexts");
    ok &= require(countDistinctGeneratedRoadKinds(result.map) == 1, "Generated dry routes should stay in a single road family");
    return ok;
}

bool testRiverGenerationInfluencesDiagonalCorners()
{
    bool ok = true;
    bool sawDiagonalPair = false;
    for (unsigned int seed = 900u; seed < 930u; ++seed)
    {
        MapGenerationOptions options;
        options.width = 32;
        options.height = 24;
        options.seed = seed;
        options.waterLevel = 35;
        options.routeDensity = 0;
        options.roadNetwork = MapRoadNetworkMode::None;
        options.mountainDensity = 0;
        options.forestDensity = 0;
        options.settlementDensity = 0;
        options.tacticalDensity = 0;

        const MapGenerationResult result = generateSemanticMap(options);
        const RiverDiagonalCornerReport report = countRiverDiagonalCornerGaps(result.map);
        sawDiagonalPair = sawDiagonalPair || report.diagonalPairs > 0;
        ok &= require(report.diagonalOnlyPairs == 0, "Generated rivers should not leave diagonal-only river pairs without crossed corner influence");
    }

    ok &= require(sawDiagonalPair, "River diagonal-corner stress should exercise at least one diagonal river step");
    return ok;
}

bool testCandidatePreviewPolicy()
{
    bool ok = true;
    ok &= require(mapStyleRuntimeCandidatePreviewAllowed(MapStyleContextualAssetClass::IntegratedWayGround), "Replacement way candidates should be available for explicit runtime review preview");
    ok &= require(mapStyleRuntimeCandidatePreviewAllowed(MapStyleContextualAssetClass::StructureContextMatrix), "Replacement structure context candidates should be available for explicit runtime review preview");
    ok &= require(mapStyleRuntimeCandidatePreviewAllowed(MapStyleContextualAssetClass::BridgeSequence), "Bridge sequence candidates may still preview for app-scale bridge review");
    ok &= require(mapStyleRuntimeCandidatePreviewAllowed(MapStyleContextualAssetClass::ComponentRoleSet), "Component role candidates may still preview for app-scale component review");
    return ok;
}

bool testBridgeSequenceRotation()
{
    MapModel horizontal;
    horizontal.width = 3;
    horizontal.height = 1;
    resizeMapCells(horizontal);
    mapGenClear(horizontal, MapTileKind::Empty);
    mapGenSetWay(horizontal, 0, 0, MapWayKind::DirtRoad);
    mapGenSetCell(horizontal, 1, 0, MapTileKind::Bridge);
    mapGenSetWay(horizontal, 1, 0, MapWayKind::DirtRoad);
    mapGenSetWay(horizontal, 2, 0, MapWayKind::DirtRoad);

    MapModel vertical;
    vertical.width = 1;
    vertical.height = 3;
    resizeMapCells(vertical);
    mapGenClear(vertical, MapTileKind::Empty);
    mapGenSetWay(vertical, 0, 0, MapWayKind::DirtRoad);
    mapGenSetCell(vertical, 0, 1, MapTileKind::Bridge);
    mapGenSetWay(vertical, 0, 1, MapWayKind::DirtRoad);
    mapGenSetWay(vertical, 0, 2, MapWayKind::DirtRoad);

    bool ok = true;
    ok &= require(mapStyleBridgeSequenceRotationQuarterTurns(makeMapWayRenderContext(horizontal, 1, 0)) == 1, "East-west bridge sequence previews should rotate horizontal bridge art");
    ok &= require(mapStyleBridgeSequenceRotationQuarterTurns(makeMapWayRenderContext(vertical, 0, 1)) == 0, "North-south bridge sequence previews should keep the source bridge orientation");
    return ok;
}

bool testCaveAndTacticalGeneration()
{
    MapGenerationOptions cave;
    cave.scope = MapScope::Dungeon;
    cave.caveNetwork = true;
    cave.width = 34;
    cave.height = 24;
    cave.seed = 700u;
    cave.tacticalDensity = 45;

    MapGenerationOptions noTactics;
    noTactics.scope = MapScope::Local;
    noTactics.width = 34;
    noTactics.height = 24;
    noTactics.seed = 701u;
    noTactics.waterLevel = 0;
    noTactics.mountainDensity = 0;
    noTactics.forestDensity = 0;
    noTactics.routeDensity = 0;
    noTactics.tacticalDensity = 0;

    MapGenerationOptions withTactics = noTactics;
    withTactics.tacticalDensity = 80;

    const MapGenerationResult caveResult = generateSemanticMap(cave);
    const MapGenerationResult repeatedCaveResult = generateSemanticMap(cave);
    MapGenerationOptions dungeon;
    dungeon.scope = MapScope::Dungeon;
    dungeon.width = 34;
    dungeon.height = 24;
    dungeon.seed = 702u;
    const MapGenerationResult dungeonResult = generateSemanticMap(dungeon);
    const MapGenerationResult repeatedDungeonResult = generateSemanticMap(dungeon);
    const MapGenerationResult noTacticsResult = generateSemanticMap(noTactics);
    const MapGenerationResult withTacticsResult = generateSemanticMap(withTactics);

    bool ok = true;
    ok &= require(countTerrain(caveResult.map, MapTileKind::Cave) > 0, "Cave generator should create cave terrain, not only dungeon rooms");
    ok &= require(countWays(caveResult.map, MapWayKind::CaveTunnel) > 0, "Cave generator should create cave tunnel way cells");
    ok &= require(countWays(caveResult.map, MapWayKind::DungeonCorridor) == 0, "Cave generator should not label tunnels as dungeon corridors");
    const int caveChambers = countGeneratedFeaturesWithMaterial(caveResult.map, "fantasy_neutral.cave.chamber");
    const int caveTunnels = countGeneratedFeaturesWithMaterial(caveResult.map, "fantasy_neutral.cave.tunnel");
    ok &= require(caveChambers >= 4 && caveTunnels == caveChambers - 1,
                   "Cave generator should expose a stable chamber-and-tunnel feature graph");
    ok &= require(countTerrain(caveResult.map, MapTileKind::Stairs) >= 2,
                  "Cave generator should expose distinct entrance and exit stairs");
    ok &= require(generatedFeatureGraphsEqual(caveResult.map.generation.graph, repeatedCaveResult.map.generation.graph),
                  "Cave feature graph should be deterministic for a stable seed");
    ok &= require(countTerrain(dungeonResult.map, MapTileKind::Stairs) >= 2, "Dungeon room generator should create entrance and exit stairs");
    ok &= require(countTerrain(dungeonResult.map, MapTileKind::Landmark) == 0, "Dungeon room generator should avoid guard-tower landmark contexts until dungeon landmark art exists");
    const int dungeonRooms = countGeneratedFeaturesWithMaterial(dungeonResult.map, "fantasy_neutral.dungeon.room");
    const int dungeonCorridors = countGeneratedFeaturesWithMaterial(dungeonResult.map, "fantasy_neutral.dungeon.corridor");
    ok &= require(dungeonRooms >= 3 && dungeonCorridors == dungeonRooms - 1,
                   "Dungeon generator should expose a stable room-and-corridor feature graph");
    ok &= require(countOverlappingInteriorRegions(dungeonResult.map,
                                                   "fantasy_neutral.dungeon.room") == 0,
                  "Dungeon rooms should remain visually distinct instead of collapsing into one overlapping floor blob");
    ok &= require(generatedFeatureGraphsEqual(dungeonResult.map.generation.graph, repeatedDungeonResult.map.generation.graph),
                  "Dungeon feature graph should be deterministic for a stable seed");
    ok &= require(countTacticalFeatures(withTacticsResult.map) > countTacticalFeatures(noTacticsResult.map), "Tactical density should increase generated tactical features");
    return ok;
}

bool testStructureWaterBuffer()
{
    MapGenerationOptions options;
    options.scope = MapScope::Local;
    options.width = 36;
    options.height = 24;
    options.seed = 4404u;
    options.waterLevel = 55;
    options.mountainDensity = 0;
    options.forestDensity = 10;
    options.settlementDensity = 80;
    options.routeDensity = 0;
    options.roadNetwork = MapRoadNetworkMode::None;
    options.tacticalDensity = 80;

    const MapGenerationResult result = generateSemanticMap(options);

    bool ok = true;
    ok &= require(countTerrain(result.map, MapTileKind::Water) > 0, "Water-buffer stress should create water terrain");
    ok &= require(countTerrain(result.map, MapTileKind::Settlement) > 0, "Water-buffer stress should still place settlements");
    ok &= require(countWaterAdjacentStructures(result.map) == 0, "Generated generic structures should avoid immediate waterfront when no waterfront structure art exists");
    return ok;
}

bool testGeneratedArtContractWayNormalization()
{
    MapGenerationOptions options;
    options.scope = MapScope::World;
    options.width = 42;
    options.height = 28;
    options.seed = 5151u;
    options.waterLevel = 45;
    options.mountainDensity = 12;
    options.forestDensity = 8;
    options.settlementDensity = 36;
    options.routeDensity = 45;
    options.roadNetwork = MapRoadNetworkMode::Sparse;
    options.tacticalDensity = 20;

    const MapGenerationResult result = generateSemanticMap(options);

    bool ok = true;
    ok &= require(countTerrain(result.map, MapTileKind::Bridge) > 0, "Contract normalization stress should create bridge crossings");
    ok &= require(countStructureLikeTiles(result.map) > 0, "Contract normalization stress should create structures");
    ok &= require(countTrailBridges(result.map) == 0, "Generated bridge spans should avoid unsupported trail approach-way context");
    ok &= require(countTrailAdjacentStructures(result.map) == 0, "Generated structure entrances should avoid unsupported trail entrance context");
    const BridgeContractGapCounts bridgeGaps = countBridgeContractGaps(result.map);
    ok &= require(bridgeGaps.missingApproachComponents == 0, "Generated bridge components should have two cardinal approach endpoints");
    ok &= require(bridgeGaps.missingCrossingComponents == 0, "Generated bridge components should retain cardinal crossing context");
    return ok;
}

bool testPostTacticalStructureApproaches()
{
    MapGenerationOptions options;
    options.scope = MapScope::Local;
    options.width = 38;
    options.height = 26;
    options.seed = 6262u;
    options.waterLevel = 0;
    options.mountainDensity = 0;
    options.forestDensity = 10;
    options.settlementDensity = 18;
    options.routeDensity = 78;
    options.roadNetwork = MapRoadNetworkMode::Connected;
    options.tacticalDensity = 90;

    const MapGenerationResult result = generateSemanticMap(options);

    bool ok = true;
    ok &= require(countTerrain(result.map, MapTileKind::Landmark) > 0, "Post-tactical approach stress should create landmarks");
    ok &= require(countStructuresWithoutApproach(result.map) == 0, "Structures placed by tactical generation should receive route approach semantics");
    return ok;
}

bool testGenerationProgressCallback()
{
    std::vector<MapGenerationProgress> progress;
    MapGenerationOptions options;
    options.width = 32;
    options.height = 22;
    options.seed = 777u;
    options.waterLevel = 30;
    options.routeDensity = 60;
    options.progressCallback = [&progress](const MapGenerationProgress &update) {
        progress.push_back(update);
    };

    generateSemanticMap(options);

    bool monotonic = true;
    int previousStep = -1;
    int expectedTotal = 0;
    bool sawRoutes = false;
    for (const MapGenerationProgress &update : progress)
    {
        if (expectedTotal == 0)
            expectedTotal = update.totalSteps;
        monotonic = monotonic && update.totalSteps == expectedTotal && update.step >= previousStep;
        previousStep = update.step;
        sawRoutes = sawRoutes || update.stage == "Drawing primary routes";
    }

    bool ok = true;
    ok &= require(!progress.empty(), "Generation progress callback should emit stage updates");
    ok &= require(progress.front().stage == "Preparing map grid", "Generation progress should begin with map preparation");
    ok &= require(progress.back().stage == "Complete", "Generation progress should end with completion");
    ok &= require(monotonic, "Generation progress should move monotonically through a stable step count");
    ok &= require(expectedTotal == 12, "Overworld generation progress should use the documented 12-step sequence");
    ok &= require(sawRoutes, "Overworld generation progress should expose route drawing as a visible stage");
    return ok;
}

bool testCaveGenerationProgressCallback()
{
    std::vector<MapGenerationProgress> progress;
    MapGenerationOptions options;
    options.scope = MapScope::Dungeon;
    options.caveNetwork = true;
    options.width = 32;
    options.height = 22;
    options.seed = 778u;
    options.tacticalDensity = 45;
    options.progressCallback = [&progress](const MapGenerationProgress &update) {
        progress.push_back(update);
    };

    generateSemanticMap(options);

    const std::vector<std::string> expectedStages = {
        "Preparing map grid",
        "Building cave network",
        "Connecting cave tunnels",
        "Placing tactical features",
        "Checking style coverage",
        "Complete"};
    std::size_t nextExpectedStage = 0;
    bool monotonic = true;
    int previousStep = -1;
    int expectedTotal = 0;
    for (const MapGenerationProgress &update : progress)
    {
        if (expectedTotal == 0)
            expectedTotal = update.totalSteps;
        monotonic = monotonic && update.totalSteps == expectedTotal && update.step >= previousStep;
        previousStep = update.step;
        if (nextExpectedStage < expectedStages.size() && update.stage == expectedStages[nextExpectedStage])
            ++nextExpectedStage;
    }

    bool ok = true;
    ok &= require(!progress.empty(), "Cave generation progress callback should emit stage updates");
    ok &= require(progress.front().stage == "Preparing map grid", "Cave generation progress should begin with map preparation");
    ok &= require(progress.back().stage == "Complete", "Cave generation progress should end with completion");
    ok &= require(monotonic, "Cave generation progress should move monotonically through a stable step count");
    ok &= require(expectedTotal == 6, "Cave generation progress should use the documented 6-step sequence");
    ok &= require(nextExpectedStage == expectedStages.size(), "Cave generation progress should expose every visible stage without skipping tunnel or tactical work");
    return ok;
}

bool testDeterministicQualitySearch()
{
    MapGenerationOptions draft;
    draft.scope = MapScope::World;
    draft.width = 42;
    draft.height = 28;
    draft.seed = 20260823u;
    draft.waterLevel = 42;
    draft.mountainDensity = 38;
    draft.forestDensity = 44;
    draft.settlementDensity = 36;
    draft.routeDensity = 64;
    draft.tacticalDensity = 30;
    draft.roadNetwork = MapRoadNetworkMode::Connected;
    draft.qualityAttempts = 1;

    MapGenerationOptions polished = draft;
    polished.qualityAttempts = 8;

    const MapGenerationResult firstCandidate = generateSemanticMap(draft);
    const MapGenerationResult selected = generateSemanticMap(polished);
    const MapGenerationResult repeated = generateSemanticMap(polished);

    std::cout << "quality-search base=" << firstCandidate.quality.score
              << " selected=" << selected.quality.score
              << " candidate=" << (selected.quality.selectedCandidateIndex + 1) << "/" << selected.quality.candidatesEvaluated
              << " connectivity=" << selected.quality.traversableConnectivityPercent
              << " ways=" << selected.quality.wayConnectivityPercent
              << " access=" << selected.quality.structureAccessPercent
              << " coherence=" << selected.quality.coherentTerrainPercent << std::endl;

    bool ok = true;
    ok &= require(selected.quality.candidatesEvaluated == 8, "Polished quality search should evaluate eight deterministic candidates");
    ok &= require(selected.quality.score >= firstCandidate.quality.score, "Quality search must never return a lower-scoring map than its first candidate");
    ok &= require(selected.quality.selectedSeed == repeated.quality.selectedSeed, "Quality search should select the same derived seed on repeat runs");
    ok &= require(selected.quality.score == repeated.quality.score, "Quality search should report a stable score on repeat runs");
    ok &= require(mapsHaveSameCells(selected.map, repeated.map), "Quality search should produce identical semantic cells for the same base seed and settings");
    ok &= require(selected.quality.traversableConnectivityPercent >= 0 && selected.quality.traversableConnectivityPercent <= 100, "Quality connectivity metric must be normalized");
    ok &= require(selected.quality.wayConnectivityPercent >= 0 && selected.quality.wayConnectivityPercent <= 100, "Quality way metric must be normalized");
    ok &= require(selected.quality.structureAccessPercent >= 0 && selected.quality.structureAccessPercent <= 100, "Quality access metric must be normalized");

    int demonstratedImprovement = 0;
    std::uint32_t demonstratedSeed = 0;
    for (std::uint32_t seed = 3100u; seed < 3116u && demonstratedImprovement == 0; ++seed)
    {
        MapGenerationOptions sample = draft;
        sample.width = 28;
        sample.height = 20;
        sample.seed = seed;
        sample.qualityAttempts = 1;
        const MapGenerationResult first = generateSemanticMap(sample);
        sample.qualityAttempts = 4;
        const MapGenerationResult best = generateSemanticMap(sample);
        demonstratedImprovement = best.quality.score - first.quality.score;
        if (demonstratedImprovement > 0)
            demonstratedSeed = seed;
    }
    std::cout << "quality-search sample-improvement=" << demonstratedImprovement
              << " seed=" << demonstratedSeed << std::endl;
    ok &= require(demonstratedImprovement > 0, "Balanced quality search should improve at least one deterministic stress sample");
    return ok;
}

bool testQualityAnalysisDetectsDisconnectedDungeon()
{
    MapModel disconnected;
    disconnected.scope = MapScope::Dungeon;
    disconnected.width = 6;
    disconnected.height = 4;
    resizeMapCells(disconnected);
    mapGenClear(disconnected, MapTileKind::Wall);
    mapGenSetCell(disconnected, 1, 1, MapTileKind::Stairs);
    mapGenSetCell(disconnected, 4, 2, MapTileKind::Stairs);

    const MapGenerationQualityReport before = analyzeMapGenerationQuality(disconnected);
    mapGenSetCell(disconnected, 2, 1, MapTileKind::Room);
    mapGenSetCell(disconnected, 3, 1, MapTileKind::Room);
    mapGenSetCell(disconnected, 4, 1, MapTileKind::Room);
    const MapGenerationQualityReport after = analyzeMapGenerationQuality(disconnected);

    bool ok = true;
    ok &= require(!before.criticalRouteConnected, "Quality analysis should detect disconnected dungeon stairs");
    ok &= require(before.traversableConnectivityPercent < 100, "Disconnected walkable dungeon islands should reduce connectivity");
    ok &= require(after.criticalRouteConnected, "Quality analysis should recognize a newly connected dungeon route");
    ok &= require(after.traversableConnectivityPercent == 100, "Connected dungeon floor should report full traversable connectivity");
    ok &= require(after.score > before.score, "Connecting the critical dungeon route should improve the quality score");
    return ok;
}

bool testV2FeatureGraphAndRankedDesigns()
{
    MapGenerationOptions options;
    options.generatorVersion = 2;
    options.scope = MapScope::World;
    options.width = 48;
    options.height = 32;
    options.seed = 20260823u;
    options.landscape = MapLandscapeProfile::RiverValley;
    options.purpose = MapGenerationPurpose::TravelArea;
    options.requestedSettlements = 3;
    options.requestedLandmarks = 4;
    options.edgeConnectionCount = 2;
    options.waterLevel = 42;
    options.forestDensity = 38;
    options.mountainDensity = 30;
    options.routeDensity = 60;
    options.qualityAttempts = 8;

    const std::vector<MapGenerationResult> candidates = generateSemanticMapCandidates(options, 3);
    const std::vector<MapGenerationResult> repeated = generateSemanticMapCandidates(options, 3);
    bool ok = true;
    ok &= require(candidates.size() == 3, "V2 generation should expose three ranked designs");
    ok &= require(repeated.size() == candidates.size(), "V2 ranked design count should be deterministic");
    if (candidates.empty() || repeated.empty())
        return false;
    const MapModel &map = candidates.front().map;
    std::cout << "v2-quality=" << candidates.front().quality.score
              << " critical=" << candidates.front().quality.criticalIssueCount
              << " roads=" << candidates.front().quality.roadEfficiencyPercent
              << " hydrology=" << candidates.front().quality.hydrologyPercent
              << " bridges=" << candidates.front().quality.bridgeValidityPercent
              << " composition=" << candidates.front().quality.compositionPercent << std::endl;
    for (const MapGenerationIssue &issue : candidates.front().quality.issues)
        std::cout << "v2-issue=" << issue.code << " severity=" << static_cast<int>(issue.severity) << std::endl;
    ok &= require(map.generation.enabled, "V2 maps should persist generation metadata");
    ok &= require(map.generation.continuousRendering, "V2 maps should opt into continuous rendering");
    ok &= require(map.generation.recipe.generatorVersion == 2, "V2 recipe should persist its generator version");
    ok &= require(!map.generation.graph.features.empty(), "V2 maps should contain an editable feature graph");
    bool sameGraph = map.generation.graph.features.size() == repeated.front().map.generation.graph.features.size();
    for (std::size_t index = 0; sameGraph && index < map.generation.graph.features.size(); ++index)
    {
        const MapGeneratedFeature &left = map.generation.graph.features[index];
        const MapGeneratedFeature &right = repeated.front().map.generation.graph.features[index];
        sameGraph = left.id == right.id && left.kind == right.kind && left.points.size() == right.points.size();
        for (std::size_t point = 0; sameGraph && point < left.points.size(); ++point)
            sameGraph = left.points[point].x == right.points[point].x && left.points[point].y == right.points[point].y;
    }
    ok &= require(sameGraph, "V2 feature graph should be deterministic for the same recipe and seed");
    ok &= require(mapsHaveSameCells(map, repeated.front().map), "V2 cell projection should be deterministic");
    for (std::size_t index = 1; index < candidates.size(); ++index)
    {
        ok &= require(candidates[index - 1].quality.criticalIssueCount <= candidates[index].quality.criticalIssueCount ||
                          candidates[index - 1].quality.score >= candidates[index].quality.score,
                      "V2 designs should be ranked by critical issues and quality");
    }
    return ok;
}

bool testV2OutdoorGenerationProperties()
{
    bool ok = true;
    // Five landscape recipes x 200 deterministic seeds = 1,000 outdoor
    // property samples, matching the V2 release acceptance gate.
    for (std::uint32_t seed = 7000u; seed < 8000u; ++seed)
    {
        MapGenerationOptions options;
        options.generatorVersion = 2;
        options.scope = MapScope::World;
        options.width = 40;
        options.height = 28;
        options.seed = seed;
        options.landscape = static_cast<MapLandscapeProfile>((seed - 7000u) % 5u);
        options.requestedSettlements = 3;
        options.requestedLandmarks = 3;
        options.edgeConnectionCount = 2;
        options.waterLevel = options.landscape == MapLandscapeProfile::LakeCountry ? 72 : 38;
        options.qualityAttempts = 1;
        const MapGenerationResult generated = generateSemanticMap(options);
        const MapModel &map = generated.map;
        bool riverGeometryContinuous = true;
        for (const MapGeneratedFeature &feature : map.generation.graph.features)
        {
            if (feature.kind != MapGeneratedFeatureKind::River)
                continue;
            for (std::size_t index = 1; index < feature.points.size(); ++index)
            {
                const float dx = feature.points[index].x - feature.points[index - 1].x;
                const float dy = feature.points[index].y - feature.points[index - 1].y;
                riverGeometryContinuous = riverGeometryContinuous && std::sqrt(dx * dx + dy * dy) <= 3.2f;
            }
        }
        ok &= require(riverGeometryContinuous, "V2 main river geometry should remain continuous through bridge crossings");
        for (const MapCellModel &cell : map.cells)
            ok &= require(!(cell.kind == MapTileKind::Building && mapGenWayKindIsRoadLike(cell.wayKind)), "V2 roads must not run underneath buildings");
        if (generated.quality.bridgeValidityPercent != 100 || generated.quality.hydrologyPercent < 80)
        {
            std::cerr << "V2 property failure seed=" << seed << " landscape=" << static_cast<int>(options.landscape)
                      << " hydrology=" << generated.quality.hydrologyPercent
                      << " bridges=" << generated.quality.bridgeValidityPercent
                      << " access=" << generated.quality.structureAccessPercent << '\n';
            for (const MapGeneratedFeature &feature : map.generation.graph.features)
            {
                if (feature.kind != MapGeneratedFeatureKind::Bridge || feature.points.size() < 2)
                    continue;
                const float centerX = (feature.points.front().x + feature.points.back().x) * 0.5f;
                const float centerY = (feature.points.front().y + feature.points.back().y) * 0.5f;
                float roadX = 0.0f, roadY = 0.0f, riverX = 0.0f, riverY = 0.0f;
                const bool road = mapGenV2NearestFeatureTangent(map.generation.graph, MapGeneratedFeatureKind::Road, centerX, centerY, 2.0f, roadX, roadY);
                const bool river = mapGenV2NearestFeatureTangent(map.generation.graph, MapGeneratedFeatureKind::River, centerX, centerY, 2.0f, riverX, riverY);
                std::cerr << " bridge=" << feature.id << " center=" << centerX << ',' << centerY
                          << " road=" << road << '(' << roadX << ',' << roadY << ')'
                          << " river=" << river << '(' << riverX << ',' << riverY << ')'
                          << " dot=" << std::abs(roadX * riverX + roadY * riverY) << '\n';
            }
        }
        ok &= require(generated.quality.hydrologyPercent >= 80, "V2 hydrology score should remain usable across deterministic seeds");
        ok &= require(generated.quality.bridgeValidityPercent == 100, "V2 bridges should remain valid across deterministic seeds");
        if (generated.quality.structureAccessPercent < 100)
            ok &= require(generated.quality.criticalIssueCount > 0, "V2 candidates with unreachable settlements must be rejected as critical");
        ok &= require(generated.quality.roadEfficiencyPercent >= 45, "V2 cost-aware roads should avoid extreme detours");
        if (!ok)
            break;
    }
    return ok;
}

bool testWorldBriefAndNamedTerritories()
{
    bool ok = true;
    const MapWorldBrief brief = MapWorldBriefInterpreter::interpret(
        "Es gibt 3 Nationen mit name X, Y und Z und es gibt eine große Hauptstadt names Typo.");
    ok &= require(brief.statedNationCount == 3, "World brief should detect the stated nation count");
    ok &= require(brief.nationNames == std::vector<std::string>({"X", "Y", "Z"}), "World brief should preserve nation names");
    ok &= require(brief.capitalName == "Typo", "World brief should detect the named capital");
    const MapWorldBrief localizedExampleBrief = MapWorldBriefInterpreter::interpret(
        "Es gibt drei Nationen namens Arven, Beloria und Cyndor sowie eine große Hauptstadt namens Typo.");
    ok &= require(localizedExampleBrief.statedNationCount == 3, "Localized world-brief example should detect three nations");
    ok &= require(localizedExampleBrief.nationNames == std::vector<std::string>({"Arven", "Beloria", "Cyndor"}),
                  "Localized world-brief example should stop nation names before the capital clause");
    ok &= require(localizedExampleBrief.capitalName == "Typo", "Localized world-brief example should preserve the capital name");
    const MapWorldBrief fullerBrief = MapWorldBriefInterpreter::interpret(
        "Es gibt 3 Nationen namens X, Y und Z. Städte namens Auenheim und Kiesfurt. Die Hauptstadt namens Typo.");
    ok &= require(fullerBrief.cityNames == std::vector<std::string>({"Auenheim", "Kiesfurt"}), "World brief should preserve named cities");

    MapGenerationOptions options;
    options.generatorVersion = 2;
    options.scope = MapScope::World;
    options.width = 48;
    options.height = 32;
    options.seed = 2811;
    options.nationNames = brief.nationNames;
    options.requestedSettlements = 1;
    MapGenerationConstraint capital;
    capital.id = "brief_capital";
    capital.label = brief.capitalName;
    capital.featureKind = MapGeneratedFeatureKind::Settlement;
    capital.x = 24.0f;
    capital.y = 16.0f;
    capital.locked = true;
    options.constraints.push_back(capital);
    const MapGenerationResult generated = generateSemanticMap(options);
    int territories = 0;
    bool namedCapital = false;
    for (const MapGeneratedFeature &feature : generated.map.generation.graph.features)
    {
        territories += feature.kind == MapGeneratedFeatureKind::TerrainRegion && feature.materialId.find("territory.") != std::string::npos ? 1 : 0;
        namedCapital = namedCapital || (feature.kind == MapGeneratedFeatureKind::Settlement && feature.name == "Typo" && feature.locked);
    }
    ok &= require(territories == 3, "World generation should create one editable territory for every named nation");
    ok &= require(namedCapital, "World generation should preserve the capital name and lock its placement");
    return ok;
}

MapGenerationOptions noteDrivenWorldOptions(const MapWorldBrief &brief, const std::string &sourceNote,
                                             int width = 72, int height = 48,
                                             std::uint32_t seed = 41005u)
{
    MapGenerationOptions options;
    options.generatorVersion = 4;
    options.scope = MapScope::World;
    options.scale = MapLevel::World;
    options.creationIntent = MapCreationIntent::WorldOverview;
    options.sizePreset = width == 48 && height == 32 ? MapSizePreset::Small :
                         (width == 72 && height == 48 ? MapSizePreset::Standard :
                          (width == 96 && height == 64 ? MapSizePreset::Large : MapSizePreset::Custom));
    options.purpose = MapGenerationPurpose::Region;
    options.width = width;
    options.height = height;
    options.seed = seed;
    options.waterLevel = 18;
    options.mountainDensity = 12;
    options.forestDensity = 20;
    options.settlementDensity = 3;
    options.routeDensity = 28;
    options.roadNetwork = MapRoadNetworkMode::Connected;
    options.edgeConnectionCount = 4;
    const int namedSettlementCount = static_cast<int>(brief.cityNames.size()) + (brief.capitalName.empty() ? 0 : 1);
    // The ordinary World flow fills gaps for a GM who only knows the named
    // capital. Automatic places must still receive usable fantasy names.
    options.requestedSettlements = std::max(4, namedSettlementCount);
    options.requestedLandmarks = 0;
    options.qualityAttempts = 8;
    options.nationNames = brief.nationNames;
    options.facts = brief.facts;
    options.knownFacts.push_back(sourceNote);
    for (const std::string &nationName : brief.nationNames)
        options.knownFacts.push_back("Nation: " + nationName);

    if (!brief.capitalName.empty())
    {
        MapGenerationConstraint capital;
        capital.id = "brief_capital";
        capital.label = brief.capitalName;
        capital.featureKind = MapGeneratedFeatureKind::Settlement;
        capital.x = static_cast<float>(options.width) * 0.52f;
        capital.y = static_cast<float>(options.height) * 0.48f;
        capital.required = true;
        capital.locked = true;
        options.constraints.push_back(std::move(capital));
    }
    for (std::size_t index = 0; index < brief.cityNames.size(); ++index)
    {
        MapGenerationConstraint city;
        city.id = "brief_city_" + std::to_string(index);
        city.label = brief.cityNames[index];
        city.featureKind = MapGeneratedFeatureKind::Settlement;
        const float fraction = static_cast<float>(index + 1) / static_cast<float>(brief.cityNames.size() + 1);
        city.x = static_cast<float>(options.width) * (0.2f + fraction * 0.6f);
        city.y = static_cast<float>(options.height) * (index % 2 == 0 ? 0.36f : 0.64f);
        city.required = true;
        city.locked = true;
        options.constraints.push_back(std::move(city));
    }
    return options;
}

bool testNoteDrivenWorldV4QaContract()
{
    bool ok = true;
    const std::string englishNote =
        "Three nations named Alderreach, Veyra, and Kharad share a great river. There is a large capital named Crosswater.";
    const std::string germanNote =
        "Drei Nationen namens Alderreach, Veyra und Kharad teilen sich einen gro\xC3\x9F" "en Fluss. Es gibt eine gro\xC3\x9F" "e Hauptstadt namens Crosswater.";
    const std::vector<std::string> expectedNations = {"Alderreach", "Veyra", "Kharad"};

    const MapWorldBrief englishBrief = MapWorldBriefInterpreter::interpret(englishNote);
    const MapWorldBrief germanBrief = MapWorldBriefInterpreter::interpret(germanNote);
    for (const MapWorldBrief *brief : {&englishBrief, &germanBrief})
    {
        ok &= require(brief->statedNationCount == 3, "DE/EN idea text should parse the exact nation count");
        ok &= require(brief->nationNames == expectedNations, "DE/EN idea text should preserve Alderreach, Veyra, and Kharad");
        ok &= require(brief->capitalName == "Crosswater", "DE/EN idea text should preserve Crosswater as the capital");
        ok &= require(brief->requiresSharedGreatRiver, "DE/EN idea text should preserve the shared-great-river fact");
    }
    ok &= require(englishBrief.facts.size() == germanBrief.facts.size() && !englishBrief.facts.empty(),
                  "DE/EN idea text should produce the same typed fact count");
    for (std::size_t index = 0; index < std::min(englishBrief.facts.size(), germanBrief.facts.size()); ++index)
    {
        const MapGenerationFact &englishFact = englishBrief.facts[index];
        const MapGenerationFact &germanFact = germanBrief.facts[index];
        ok &= require(englishFact.id == germanFact.id && englishFact.type == germanFact.type &&
                          englishFact.strength == germanFact.strength && englishFact.label == germanFact.label &&
                          englishFact.value == germanFact.value && englishFact.entityRefs == germanFact.entityRefs,
                      "Equivalent DE/EN briefs should normalize to identical semantic facts");
    }

    const MapGenerationOptions englishOptions = noteDrivenWorldOptions(englishBrief, englishNote);
    const MapGenerationOptions germanOptions = noteDrivenWorldOptions(germanBrief, germanNote);
    const MapGenerationResult generated = generateSemanticMap(germanOptions);
    const MapGenerationResult generatedFromEnglish = generateSemanticMap(englishOptions);
    ok &= require(generatedFeatureGraphsEqual(generated.map.generation.graph, generatedFromEnglish.map.generation.graph),
                  "Equivalent DE/EN world facts should generate the same deterministic feature graph");

    std::set<std::string> territoryNames;
    bool namedCapital = false;
    bool genericFeatureName = false;
    int roadFeatureCount = 0;
    int riverFeatureCount = 0;
    bool riverGeometryContinuous = true;
    for (const MapGeneratedFeature &feature : generated.map.generation.graph.features)
    {
        if (feature.kind == MapGeneratedFeatureKind::TerrainRegion &&
            feature.materialId.find("territory.") != std::string::npos)
            territoryNames.insert(feature.name);
        if (feature.kind == MapGeneratedFeatureKind::Settlement && feature.name == "Crosswater" && feature.locked)
            namedCapital = true;
        if ((feature.kind == MapGeneratedFeatureKind::Settlement && feature.name.rfind("Settlement ", 0) == 0) ||
            (feature.kind == MapGeneratedFeatureKind::Landmark && feature.name.rfind("Landmark ", 0) == 0))
            genericFeatureName = true;
        roadFeatureCount += feature.kind == MapGeneratedFeatureKind::Road || feature.kind == MapGeneratedFeatureKind::Trail ? 1 : 0;
        if (feature.kind == MapGeneratedFeatureKind::River)
        {
            ++riverFeatureCount;
            riverGeometryContinuous = riverGeometryContinuous && feature.points.size() >= 2;
            for (std::size_t pointIndex = 1; pointIndex < feature.points.size(); ++pointIndex)
            {
                const float dx = feature.points[pointIndex].x - feature.points[pointIndex - 1].x;
                const float dy = feature.points[pointIndex].y - feature.points[pointIndex - 1].y;
                riverGeometryContinuous = riverGeometryContinuous && std::sqrt(dx * dx + dy * dy) <= 3.2f;
            }
        }
    }
    ok &= require(territoryNames == std::set<std::string>(expectedNations.begin(), expectedNations.end()),
                  "A note-driven V4 world should expose exactly the three named nation territories");
    ok &= require(namedCapital, "A note-driven V4 world should preserve and lock the named capital");
    ok &= require(!genericFeatureName, "A note-driven V4 world must not expose generic Settlement/Landmark names");
    ok &= require(roadFeatureCount >= 3 && countRoadWays(generated.map) >= generated.map.width / 3,
                  "A note-driven V4 world should contain a visible connected road network");
    ok &= require(riverFeatureCount > 0 && riverGeometryContinuous,
                  "A note-driven V4 world should contain a continuous river network");
    ok &= require(generated.quality.wayConnectivityPercent >= 80 && generated.quality.roadEfficiencyPercent >= 45,
                  "A note-driven V4 world road network should meet the usable connectivity and efficiency floor");
    ok &= require(generated.quality.hydrologyPercent >= 80,
                  "A note-driven V4 world river network should meet the hydrology floor");
    ok &= require(generated.validation.eligible && generated.validation.criticalIssueCount == 0,
                  "A note-driven V4 world should pass hard validation independently of its quality score");
    ok &= require(generated.validation.facts.size() == generated.map.generation.recipe.facts.size(),
                  "Hard validation should expose one fulfillment record for every typed fact");
    bool sharedRiverHasEvidence = false;
    bool capitalHasEvidence = false;
    for (const MapGenerationFactValidation &factResult : generated.validation.facts)
    {
        const auto source = std::find_if(generated.map.generation.recipe.facts.begin(),
                                         generated.map.generation.recipe.facts.end(),
                                         [&](const MapGenerationFact &fact) { return fact.id == factResult.factId; });
        if (source != generated.map.generation.recipe.facts.end() &&
            source->strength == MapGenerationFactStrength::Must)
            ok &= require(factResult.fulfillment == MapGenerationFactFulfillment::Fulfilled,
                          "Every selectable required fact should be explicitly reported as fulfilled");
        if (source != generated.map.generation.recipe.facts.end() &&
            source->type == MapGenerationFactType::SharedRiver)
            sharedRiverHasEvidence = !factResult.affectedFeatureIds.empty() && !factResult.positions.empty();
        if (source != generated.map.generation.recipe.facts.end() &&
            source->type == MapGenerationFactType::Capital)
            capitalHasEvidence = !factResult.affectedFeatureIds.empty() && !factResult.positions.empty();
    }
    ok &= require(sharedRiverHasEvidence,
                  "A fulfilled shared-river promise should identify the generated river and a map position");
    ok &= require(capitalHasEvidence,
                  "A fulfilled capital promise should identify the generated settlement and a map position");

    bool worldHasStampedVegetationFeature = false;
    bool worldHasTacticalFeature = false;
    for (const MapGeneratedFeature &feature : generated.map.generation.graph.features)
    {
        worldHasStampedVegetationFeature = worldHasStampedVegetationFeature || feature.kind == MapGeneratedFeatureKind::Vegetation;
        worldHasTacticalFeature = worldHasTacticalFeature || feature.kind == MapGeneratedFeatureKind::TacticalSetpiece;
    }
    ok &= require(!worldHasStampedVegetationFeature,
                  "World-scale forests should remain macro terrain and must not create vegetation-stamp features");
    ok &= require(!worldHasTacticalFeature, "World-scale maps must not reference tactical feature roles");
    return ok;
}

bool testSharedGreatRiverAtEditorAndPresetSizes()
{
    bool ok = true;
    const std::string note =
        "Drei Nationen namens Alderreach, Veyra und Kharad teilen sich einen gro\xC3\x9F" "en Fluss. "
        "Es gibt eine gro\xC3\x9F" "e Hauptstadt namens Crosswater.";
    const MapWorldBrief brief = MapWorldBriefInterpreter::interpret(note);
    struct SizeCase { int width; int height; std::uint32_t seed; const char *label; };
    const SizeCase sizes[] = {
        {24, 18, 240529u, "legacy editor 24x18"},
        {48, 32, 41005u, "small world preset"},
        {72, 48, 41006u, "standard world preset"},
        {96, 64, 41007u, "large world preset"}};
    for (const SizeCase &size : sizes)
    {
        MapGenerationOptions options = noteDrivenWorldOptions(brief, note, size.width, size.height, size.seed);
        options.qualityAttempts = 12;
        const MapGenerationBatchResult batch = generateSemanticMapCandidateBatch(options, 3);
        ok &= require(batch.succeeded(), std::string("Shared-river generation should find an eligible design at ") + size.label);
        ok &= require(batch.failure == MapGenerationBatchFailureCode::None,
                      std::string("A valid shared-river batch should not expose a failure at ") + size.label);
        ok &= require(batch.recommendedIndex == 0 &&
                          batch.recommendedIndex < static_cast<int>(batch.candidates.size()),
                      std::string("A successful batch should explicitly recommend its best valid design at ") + size.label);
        for (const MapGenerationResult &candidate : batch.candidates)
        {
            ok &= require(candidate.validation.eligible && candidate.validation.blockers.empty(),
                          "Every selectable candidate must pass hard validation");
            ok &= require(candidate.quality.criticalIssueCount == 0 && mapGenV4SharedGreatRiverIsValid(candidate.map),
                          "Every selectable shared-river candidate must contain one valid continuous river");
            ok &= require(candidate.quality.score >= 85 && mapGenQualityMeetsCandidateFloor(candidate.map, candidate.quality),
                          "Every selectable candidate must meet the total and per-category quality floor");
        }
    }

    MapGenerationOptions impossible = noteDrivenWorldOptions(brief, note, 4, 4, 9191u);
    impossible.qualityAttempts = 3;
    const MapGenerationBatchResult failed = generateSemanticMapCandidateBatch(impossible, 3);
    ok &= require(!failed.succeeded() && failed.candidates.empty(),
                  "An impossible required fact must never leak into the selectable candidate list");
    ok &= require(failed.recommendedIndex == -1,
                  "A failed batch must never expose a recommended candidate index");
    ok &= require(failed.failure == MapGenerationBatchFailureCode::NoEligibleCandidate &&
                      !failed.rejectedCandidates.empty(),
                  "An impossible request should expose a structured no-eligible-candidate result");
    for (const MapGenerationResult &rejected : failed.rejectedCandidates)
    {
        ok &= require(!rejected.validation.eligible && !rejected.validation.blockers.empty(),
                      "Rejected diagnostic candidates should explain their hard validation blockers");
        ok &= require(rejected.quality.score == 0,
                      "Rejected candidates must not expose a normal user-facing quality score");
    }
    return ok;
}

bool testV5WorldCompositionRejectsScreenshotRegressions()
{
    bool ok = true;
    const std::string note =
        "Three nations named Alderreach, Veyra, and Kharad share a great river. "
        "There is a large capital named Crosswater.";
    const MapWorldBrief brief = MapWorldBriefInterpreter::interpret(note);
    MapGenerationResult reference;
    bool sawCapitalAwayFromRiver = false;
    std::set<std::string> coastProfiles;
    for (const std::uint32_t seed : {240529u, 930000u, 930002u})
    {
        MapGenerationOptions options = noteDrivenWorldOptions(brief, note, 72, 48, seed);
        options.generatorVersion = 5;
        options.requestedSettlements = 3;
        options.requestedLandmarks = 1;
        options.qualityAttempts = 24;
        const MapGenerationBatchResult batch = generateSemanticMapCandidateBatch(options, 3);
        if (!batch.succeeded())
        {
            std::cerr << "V5 screenshot seed " << seed << " rejected "
                      << batch.rejectedCandidates.size() << " candidate(s):";
            const std::size_t diagnosticCount =
                std::min<std::size_t>(batch.rejectedCandidates.size(), 24);
            for (std::size_t index = 0; index < diagnosticCount; ++index)
            {
                const MapGenerationResult &rejected = batch.rejectedCandidates[index];
                std::cerr << " [seed=" << rejected.map.generation.recipe.seed
                          << " score=" << rejected.quality.score
                          << " territory="
                          << mapGenV5WorldTerritoryGeographyPercent(rejected.map)
                          << " naturalism="
                          << mapGenV5WorldTerritoryNaturalismPercent(rejected.map)
                          << " straight-rays="
                          << mapGenV5WorldTerritoryStraightRayPercent(rejected.map)
                          << " road=" << mapGenV5WorldRoadNetworkPercent(rejected.map)
                          << "/" << mapGenV5WorldRoadTerrainPercent(rejected.map)
                          << " river=" << mapGenV5WorldRiverCompositionPercent(rejected.map)
                          << " settlements=" << mapGenV5WorldSettlementSpreadPercent(rejected.map)
                          << " mountains=" << mapGenV5WorldMountainCompositionPercent(rejected.map)
                          << " coast=" << mapGenV5WorldCoastCompositionPercent(rejected.map)
                          << " issues=";
                for (const MapGenerationIssue &issue : rejected.quality.issues)
                    if (issue.severity == MapGenerationIssueSeverity::Critical)
                        std::cerr << issue.code << ',';
                std::cerr << ']';
            }
            std::cerr << '\n';
        }
        ok &= require(batch.succeeded() && !batch.candidates.empty(),
                      "V5 should replace the sparse/linear screenshot regression with an eligible world");
        for (const MapGenerationResult &candidate : batch.candidates)
        {
            const int settlementCount = static_cast<int>(std::count_if(
                candidate.map.generation.graph.features.begin(),
                candidate.map.generation.graph.features.end(),
                [](const MapGeneratedFeature &feature) {
                    return feature.kind == MapGeneratedFeatureKind::Settlement;
                }));
            const int riverCount = static_cast<int>(std::count_if(
                candidate.map.generation.graph.features.begin(),
                candidate.map.generation.graph.features.end(),
                [](const MapGeneratedFeature &feature) {
                    return feature.kind == MapGeneratedFeatureKind::River;
                }));
            const int mountainRangeCount = static_cast<int>(std::count_if(
                candidate.map.generation.graph.features.begin(),
                candidate.map.generation.graph.features.end(),
                [](const MapGeneratedFeature &feature) {
                    return feature.kind == MapGeneratedFeatureKind::MountainRange;
                }));
            ok &= require(settlementCount >= 4,
                          "A three-nation V5 overview should add at least one secondary principal place instead of repeating a sparse three-node template");
            ok &= require(riverCount >= 1 && riverCount <= 2 &&
                              mapGenV5WorldRiverCompositionPercent(candidate.map) >= 80,
                          "A standard shared-river world should contain one main stem and at most one natural tributary");
            ok &= require(mapGenV5WorldRoadNetworkPercent(candidate.map) >= 80,
                          "A selectable V5 world must contain a substantial principal road network");
            ok &= require(mapGenV5WorldRoadTerrainPercent(candidate.map) >= 80,
                          "A selectable V5 world road must stay on readable inland terrain except at explicit crossings");
            ok &= require(mapGenV5WorldTerritoryGeographyPercent(candidate.map) >= 80,
                          "V5 nation boundaries must follow land rather than the rectangular ocean canvas");
            ok &= require(mapGenV5WorldTerritoryNaturalismPercent(candidate.map) >= 80,
                          "V5 nation boundaries must not collapse into long straight Voronoi rays");
            ok &= require(mapGenV5WorldSettlementSpreadPercent(candidate.map) >= 80,
                          "V5 principal places must not collapse onto one river/road line");
            ok &= require(mapGenV5WorldMountainCompositionPercent(candidate.map) >= 80,
                          "V5 world mountains must remain scale-correct connected ranges");
            ok &= require(mapGenV5WorldCoastCompositionPercent(candidate.map) >= 80,
                          "V5 world coastlines must contain readable bays, capes, or offshore islands rather than a broad oval");
            ok &= require(mapGenV5WorldLargestLandMotifPercent(candidate.map) >= 22,
                          "Every selectable shared-river world needs one substantial connected continental motif");
            ok &= require(mapGenV5WorldBiomeCompositionPercent(candidate.map) >= 80,
                          "V5 world forests must read as a few connected macro-biomes");
            ok &= require(mountainRangeCount <= 2,
                          "A normal V5 world must not fragment mountains into repeated short ranges");
            ok &= require(candidate.validation.eligible && candidate.quality.criticalIssueCount == 0,
                          "Every offered V5 world must pass the strengthened geographic gates");
            const auto capital = std::find_if(
                candidate.map.generation.graph.features.begin(),
                candidate.map.generation.graph.features.end(),
                [](const MapGeneratedFeature &feature) {
                    return feature.id == "brief_capital" && !feature.points.empty();
                });
            const auto mainRiver = std::find_if(
                candidate.map.generation.graph.features.begin(),
                candidate.map.generation.graph.features.end(),
                [](const MapGeneratedFeature &feature) {
                    return feature.kind == MapGeneratedFeatureKind::River && feature.locked;
                });
            if (capital != candidate.map.generation.graph.features.end() &&
                mainRiver != candidate.map.generation.graph.features.end())
            {
                float nearestSquared = std::numeric_limits<float>::max();
                for (const MapFeaturePoint &point : mainRiver->points)
                {
                    const float deltaX = capital->points.front().x - point.x;
                    const float deltaY = capital->points.front().y - point.y;
                    nearestSquared = std::min(nearestSquared, deltaX * deltaX + deltaY * deltaY);
                }
                sawCapitalAwayFromRiver = sawCapitalAwayFromRiver || nearestSquared > 25.0f;
            }
            std::string coastProfile;
            for (int y = 0; y < candidate.map.height; ++y)
            {
                int firstLand = candidate.map.width;
                int lastLand = -1;
                for (int x = 0; x < candidate.map.width; ++x)
                {
                    const MapCellModel *cell = mapCellAt(candidate.map, x, y);
                    const bool land = cell && !(cell->kind == MapTileKind::Water &&
                                                cell->wayKind != MapWayKind::River &&
                                                cell->wayKind != MapWayKind::Canal);
                    if (!land)
                        continue;
                    firstLand = std::min(firstLand, x);
                    lastLand = std::max(lastLand, x);
                }
                coastProfile += std::to_string(firstLand) + ":" +
                                std::to_string(lastLand) + ";";
            }
            coastProfiles.insert(std::move(coastProfile));
        }
        if (!batch.candidates.empty() && reference.map.cells.empty())
            reference = batch.candidates.front();
    }
    ok &= require(sawCapitalAwayFromRiver,
                  "An independently named capital must not be forced onto the shared river in every design");
    ok &= require(coastProfiles.size() >= 3,
                  "The screenshot regression seeds must produce structurally different coastline profiles");

    if (!reference.map.cells.empty())
    {
        MapModel shortRoad = reference.map;
        auto &shortFeatures = shortRoad.generation.graph.features;
        shortFeatures.erase(std::remove_if(shortFeatures.begin(), shortFeatures.end(),
                                           [](const MapGeneratedFeature &feature) {
                                               return feature.kind == MapGeneratedFeatureKind::Road ||
                                                      feature.kind == MapGeneratedFeatureKind::Trail;
                                           }),
                            shortFeatures.end());
        MapGeneratedFeature stub;
        stub.id = "regression_short_world_road";
        stub.kind = MapGeneratedFeatureKind::Road;
        stub.name = "Road stub";
        stub.materialId = "fantasy_neutral.road.dirt";
        stub.points = {{10.5f, 10.5f}, {11.5f, 10.5f}};
        shortFeatures.push_back(stub);
        const MapGenerationQualityReport shortRoadQuality = analyzeMapGenerationQuality(shortRoad);
        ok &= require(shortRoadQuality.wayConnectivityPercent < 80 &&
                          std::any_of(shortRoadQuality.issues.begin(), shortRoadQuality.issues.end(),
                                      [](const MapGenerationIssue &issue) {
                                          return issue.code == "road.world_network_too_small" &&
                                                 issue.severity == MapGenerationIssueSeverity::Critical;
                                      }),
                      "The former two-cell world road must be rejected instead of scoring 100");

        MapModel rectangularTerritories = reference.map;
        for (MapGeneratedFeature &feature : rectangularTerritories.generation.graph.features)
            if (feature.kind == MapGeneratedFeatureKind::TerrainRegion &&
                feature.materialId.rfind("fantasy_neutral.territory.", 0) == 0)
                feature.points = {{0.35f, 0.35f},
                                  {static_cast<float>(rectangularTerritories.width) - 0.35f, 0.35f},
                                  {static_cast<float>(rectangularTerritories.width) - 0.35f,
                                   static_cast<float>(rectangularTerritories.height) - 0.35f},
                                  {0.35f, static_cast<float>(rectangularTerritories.height) - 0.35f}};
        const MapGenerationQualityReport rectangleQuality = analyzeMapGenerationQuality(rectangularTerritories);
        ok &= require(mapGenV5WorldTerritoryGeographyPercent(rectangularTerritories) < 80 &&
                          std::any_of(rectangleQuality.issues.begin(), rectangleQuality.issues.end(),
                                      [](const MapGenerationIssue &issue) {
                                          return issue.code == "territory.crosses_open_ocean" &&
                                                 issue.severity == MapGenerationIssueSeverity::Critical;
                                      }),
                      "Ocean-spanning rectangular nation outlines must fail the V5 quality gate");

        MapModel radialTerritories = reference.map;
        const float left = 4.0f;
        const float right = static_cast<float>(radialTerritories.width) - 4.0f;
        const float top = 4.0f;
        const float bottom = static_cast<float>(radialTerritories.height) - 4.0f;
        const float centerX = static_cast<float>(radialTerritories.width) * 0.5f;
        const float centerY = static_cast<float>(radialTerritories.height) * 0.5f;
        const std::vector<std::vector<MapFeaturePoint>> radialWedges = {
            {{centerX, centerY}, {left, top}, {right, top}},
            {{centerX, centerY}, {right, top}, {right, bottom}, {centerX, bottom}},
            {{centerX, centerY}, {centerX, bottom}, {left, bottom}, {left, top}}};
        int radialIndex = 0;
        for (MapGeneratedFeature &feature : radialTerritories.generation.graph.features)
        {
            if (feature.kind != MapGeneratedFeatureKind::TerrainRegion ||
                feature.materialId.rfind("fantasy_neutral.territory.", 0) != 0)
                continue;
            if (radialIndex < static_cast<int>(radialWedges.size()))
                feature.points = radialWedges[static_cast<std::size_t>(radialIndex)];
            ++radialIndex;
        }
        const MapGenerationQualityReport radialQuality =
            analyzeMapGenerationQuality(radialTerritories);
        ok &= require(radialIndex >= 3 &&
                          mapGenV5WorldTerritoryNaturalismPercent(radialTerritories) < 80 &&
                          mapGenV5WorldTerritoryGeographyPercent(radialTerritories) < 80 &&
                          std::any_of(radialQuality.issues.begin(), radialQuality.issues.end(),
                                      [](const MapGenerationIssue &issue) {
                                          return issue.code == "territory.boundaries_too_geometric" &&
                                                 issue.severity == MapGenerationIssueSeverity::Critical;
                                      }),
                      "Long straight Voronoi pie-slice borders must fail the World geography gate");

        MapModel collinearSettlements = reference.map;
        int settlementIndex = 0;
        for (MapGeneratedFeature &feature : collinearSettlements.generation.graph.features)
            if (feature.kind == MapGeneratedFeatureKind::Settlement && !feature.points.empty())
            {
                feature.points.front().x = 18.0f + static_cast<float>(settlementIndex * 14);
                feature.points.front().y = 24.0f;
                ++settlementIndex;
            }
        const MapGenerationQualityReport collinearQuality = analyzeMapGenerationQuality(collinearSettlements);
        ok &= require(mapGenV5WorldSettlementSpreadPercent(collinearSettlements) < 80 &&
                          std::any_of(collinearQuality.issues.begin(), collinearQuality.issues.end(),
                                      [](const MapGenerationIssue &issue) {
                                          return issue.code == "composition.world_settlements_collinear" &&
                                                 issue.severity == MapGenerationIssueSeverity::Critical;
                                      }),
                      "A single-line river/city composition must not receive a publishable quality score");

        MapModel rightAngleRoads = reference.map;
        auto &rightAngleFeatures = rightAngleRoads.generation.graph.features;
        rightAngleFeatures.erase(std::remove_if(rightAngleFeatures.begin(), rightAngleFeatures.end(),
                                                [](const MapGeneratedFeature &feature) {
                                                    return feature.kind == MapGeneratedFeatureKind::Road ||
                                                           feature.kind == MapGeneratedFeatureKind::Trail;
                                                }),
                                 rightAngleFeatures.end());
        for (int roadIndex = 0; roadIndex < 2; ++roadIndex)
        {
            MapGeneratedFeature road;
            road.id = "regression_right_angle_" + std::to_string(roadIndex);
            road.kind = MapGeneratedFeatureKind::Road;
            road.name = "Right-angle road";
            road.materialId = "fantasy_neutral.road.dirt";
            road.points = roadIndex == 0
                              ? std::vector<MapFeaturePoint>{{2.0f, 4.0f}, {34.0f, 4.0f}, {34.0f, 25.0f}}
                              : std::vector<MapFeaturePoint>{{34.0f, 25.0f}, {62.0f, 25.0f}, {62.0f, 40.0f}};
            rightAngleFeatures.push_back(std::move(road));
        }
        ok &= require(mapGenV5WorldRoadNetworkPercent(rightAngleRoads) < 80,
                      "Long right-angle world roads must fail the geographic road gate");

        MapModel wetRoad = reference.map;
        MapCellCoordinate openWater{-1, -1};
        for (int y = 0; y < wetRoad.height && openWater.x < 0; ++y)
            for (int x = 0; x < wetRoad.width; ++x)
                if (mapGenV5WorldOpenWaterAt(wetRoad, x, y))
                {
                    openWater = {x, y};
                    break;
                }
        if (openWater.x >= 0)
        {
            MapGeneratedFeature invalidRoad;
            invalidRoad.id = "regression_open_water_road";
            invalidRoad.kind = MapGeneratedFeatureKind::Road;
            invalidRoad.name = "Open-water road";
            invalidRoad.materialId = "fantasy_neutral.road.dirt";
            invalidRoad.points = {
                {static_cast<float>(openWater.x) + 0.10f,
                 static_cast<float>(openWater.y) + 0.10f},
                {static_cast<float>(openWater.x) + 0.90f,
                 static_cast<float>(openWater.y) + 0.90f}};
            wetRoad.generation.graph.features.push_back(std::move(invalidRoad));
            const MapGenerationQualityReport wetRoadQuality =
                analyzeMapGenerationQuality(wetRoad);
            ok &= require(mapGenV5WorldRoadTerrainPercent(wetRoad) < 80 &&
                              std::any_of(wetRoadQuality.issues.begin(),
                                          wetRoadQuality.issues.end(),
                                          [](const MapGenerationIssue &issue) {
                                              return issue.code == "road.world_terrain_conflict" &&
                                                     issue.severity == MapGenerationIssueSeverity::Critical;
                                          }),
                          "A smoothed world road over open sea must fail the terrain gate");
        }
        else
            ok &= require(false, "The world-road water regression requires an open ocean cell");

        MapModel coastalSource = reference.map;
        const auto coastalMain = std::find_if(
            coastalSource.generation.graph.features.begin(),
            coastalSource.generation.graph.features.end(),
            [](const MapGeneratedFeature &feature) {
                return feature.kind == MapGeneratedFeatureKind::River &&
                       feature.locked && feature.width >= 1.35f &&
                       !feature.points.empty();
            });
        if (coastalMain != coastalSource.generation.graph.features.end())
        {
            const int sourceX = std::clamp(
                static_cast<int>(std::floor(coastalMain->points.front().x)),
                0, coastalSource.width - 1);
            const int sourceY = std::clamp(
                static_cast<int>(std::floor(coastalMain->points.front().y)),
                0, coastalSource.height - 1);
            bool openedBay = false;
            for (int offsetY = -1; offsetY <= 1 && !openedBay; ++offsetY)
                for (int offsetX = -1; offsetX <= 1 && !openedBay; ++offsetX)
                {
                    if (offsetX == 0 && offsetY == 0)
                        continue;
                    MapCellModel *cell = mapCellAt(
                        coastalSource, sourceX + offsetX, sourceY + offsetY);
                    if (!cell || cell->wayKind == MapWayKind::River)
                        continue;
                    cell->kind = MapTileKind::Water;
                    cell->wayKind = MapWayKind::None;
                    cell->elevation = 0;
                    openedBay = true;
                }
            std::string failureCode;
            ok &= require(openedBay &&
                              !mapGenV4SharedGreatRiverIsValid(coastalSource, &failureCode) &&
                              (failureCode == "river.shared.source_too_coastal" ||
                               failureCode == "river.shared.source_touches_ocean"),
                          "A shared river with ocean at its source must fail instead of reading coast-to-coast");
        }
        else
            ok &= require(false, "The coastal-source regression requires a locked main river");

        MapModel fragmentedMountains = reference.map;
        for (MapCellModel &cell : fragmentedMountains.cells)
            if (cell.kind == MapTileKind::Mountain)
                cell.kind = MapTileKind::Hill;
        const MapCellCoordinate fragments[] = {
            {8, 8}, {9, 8}, {24, 9}, {25, 9}, {45, 31}, {46, 31}, {61, 35}, {62, 35}};
        for (const MapCellCoordinate coordinate : fragments)
            if (MapCellModel *cell = mapCellAt(fragmentedMountains, coordinate.x, coordinate.y))
                cell->kind = MapTileKind::Mountain;
        ok &= require(mapGenV5WorldMountainCompositionPercent(fragmentedMountains) < 80,
                      "Tiny final-raster mountain fragments must fail even when an older range feature remains");

        MapModel ovalCoast = reference.map;
        for (int y = 0; y < ovalCoast.height; ++y)
            for (int x = 0; x < ovalCoast.width; ++x)
            {
                MapCellModel *cell = mapCellAt(ovalCoast, x, y);
                if (!cell)
                    continue;
                const float normalizedX =
                    (static_cast<float>(x) + 0.5f - ovalCoast.width * 0.5f) /
                    (ovalCoast.width * 0.40f);
                const float normalizedY =
                    (static_cast<float>(y) + 0.5f - ovalCoast.height * 0.5f) /
                    (ovalCoast.height * 0.36f);
                cell->kind = normalizedX * normalizedX + normalizedY * normalizedY <= 1.0f
                                 ? MapTileKind::Empty
                                 : MapTileKind::Water;
                cell->wayKind = MapWayKind::None;
            }
        ok &= require(mapGenV5WorldCoastCompositionPercent(ovalCoast) < 80,
                      "A smooth central oval must fail the V5 world coastline gate");

        MapModel tinyLandMotif = reference.map;
        for (int y = 0; y < tinyLandMotif.height; ++y)
            for (int x = 0; x < tinyLandMotif.width; ++x)
            {
                MapCellModel *cell = mapCellAt(tinyLandMotif, x, y);
                if (!cell)
                    continue;
                cell->kind = x >= tinyLandMotif.width / 2 - 4 &&
                                     x <= tinyLandMotif.width / 2 + 4 &&
                                     y >= tinyLandMotif.height / 2 - 3 &&
                                     y <= tinyLandMotif.height / 2 + 3
                                 ? MapTileKind::Empty
                                 : MapTileKind::Water;
                cell->wayKind = MapWayKind::None;
            }
        ok &= require(mapGenV5WorldLargestLandMotifPercent(tinyLandMotif) < 22 &&
                          mapGenV5WorldCoastCompositionPercent(tinyLandMotif) < 80,
                      "A small decorative island must not pass as the primary world land motif");

        MapModel parallelTributary = reference.map;
        auto &parallelFeatures = parallelTributary.generation.graph.features;
        parallelFeatures.erase(
            std::remove_if(parallelFeatures.begin(), parallelFeatures.end(),
                           [](const MapGeneratedFeature &feature) {
                               return feature.kind == MapGeneratedFeatureKind::River && !feature.locked;
                           }),
            parallelFeatures.end());
        const auto main = std::find_if(
            parallelFeatures.begin(), parallelFeatures.end(),
            [](const MapGeneratedFeature &feature) {
                return feature.kind == MapGeneratedFeatureKind::River && feature.locked &&
                       feature.points.size() >= 10;
            });
        if (main != parallelFeatures.end())
        {
            MapGeneratedFeature branch;
            branch.id = "regression_parallel_tributary";
            branch.kind = MapGeneratedFeatureKind::River;
            branch.name = "Parallel tributary";
            branch.materialId = "fantasy_neutral.water.creek";
            branch.width = 0.78f;
            const std::size_t joinIndex = main->points.size() * 2 / 3;
            for (std::size_t index = 0; index < joinIndex; ++index)
            {
                const std::size_t before = index > 0 ? index - 1 : index;
                const std::size_t after = std::min(main->points.size() - 1, index + 1);
                const float tangentX = main->points[after].x - main->points[before].x;
                const float tangentY = main->points[after].y - main->points[before].y;
                const float length = std::max(
                    0.001f, std::sqrt(tangentX * tangentX + tangentY * tangentY));
                branch.points.push_back({main->points[index].x - tangentY / length * 2.4f,
                                         main->points[index].y + tangentX / length * 2.4f});
            }
            branch.points.push_back(main->points[joinIndex]);
            parallelFeatures.push_back(std::move(branch));
            ok &= require(mapGenV5WorldRiverCompositionPercent(parallelTributary) < 80,
                          "A tributary that shadows the main river must fail the V5 rail-pattern gate");

            MapModel rulerTributary = reference.map;
            auto &rulerFeatures = rulerTributary.generation.graph.features;
            rulerFeatures.erase(
                std::remove_if(rulerFeatures.begin(), rulerFeatures.end(),
                               [](const MapGeneratedFeature &feature) {
                                   return feature.kind == MapGeneratedFeatureKind::River &&
                                          !feature.locked;
                               }),
                rulerFeatures.end());
            const auto rulerMain = std::find_if(
                rulerFeatures.begin(), rulerFeatures.end(),
                [](const MapGeneratedFeature &feature) {
                    return feature.kind == MapGeneratedFeatureKind::River &&
                           feature.locked && feature.points.size() >= 10;
                });
            if (rulerMain != rulerFeatures.end())
            {
                const std::size_t rulerJoinIndex = rulerMain->points.size() / 2;
                const MapFeaturePoint rulerJoin = rulerMain->points[rulerJoinIndex];
                const MapFeaturePoint rulerSource{
                    std::clamp(rulerJoin.x - 12.0f, 1.0f,
                               static_cast<float>(rulerTributary.width - 2)),
                    std::clamp(rulerJoin.y - 9.0f, 1.0f,
                               static_cast<float>(rulerTributary.height - 2))};
                MapGeneratedFeature ruler;
                ruler.id = "regression_ruler_tributary";
                ruler.kind = MapGeneratedFeatureKind::River;
                ruler.name = "Ruler tributary";
                ruler.materialId = "fantasy_neutral.water.creek";
                ruler.width = 1.02f;
                for (int pointIndex = 0; pointIndex <= 12; ++pointIndex)
                {
                    const float amount = static_cast<float>(pointIndex) / 12.0f;
                    ruler.points.push_back({
                        rulerSource.x + (rulerJoin.x - rulerSource.x) * amount,
                        rulerSource.y + (rulerJoin.y - rulerSource.y) * amount});
                }
                rulerFeatures.push_back(std::move(ruler));
                ok &= require(
                    mapGenV5WorldRiverCompositionPercent(rulerTributary) < 80,
                    "An almost perfectly straight optional world tributary must be omitted or rejected");
            }
            else
                ok &= require(false, "The straight-tributary regression requires a substantial main river");
        }
        else
            ok &= require(false, "The V5 coastline regression fixture requires a substantial main river");
    }
    return ok;
}

bool testV5ConstraintWorldAvoidsCoastalRoadAndKeepsPrincipalPlacesConnected()
{
    MapCreationRequest request;
    request.intent = MapCreationIntent::WorldOverview;
    request.sizePreset = MapSizePreset::Small;
    request.seed = 731676u;
    request.generatorVersion = 5;
    request.recipeVersion = 3;
    request.styleId = "fantasy_neutral";
    request.styleVersion = 3;
    request.styleSnapshotId = mapStyleCurrentProductionSnapshotId();
    request.quality = MapGenerationQualityLevel::Draft;
    request.contentLanguage = "de";
    request.nameCulture = "neutral_fantasy";
    request.allowAdditionalNations = false;

    const auto addFact = [&](const std::string &id, MapGenerationFactType type,
                             MapGenerationFactStrength strength,
                             const std::string &label,
                             const std::string &value = std::string()) {
        MapGenerationFact fact;
        fact.id = id;
        fact.type = type;
        fact.strength = strength;
        fact.label = label;
        fact.value = value.empty() ? label : value;
        fact.sourceText = "V5 constraint-world regression";
        fact.confidence = 1.0f;
        fact.confirmed = true;
        request.facts.push_back(std::move(fact));
    };
    addFact("qa_preferred_landscape", MapGenerationFactType::Forest,
            MapGenerationFactStrength::Prefer, "forest");
    for (const std::string &nation : {"Aster", "Bram", "Cyr"})
        addFact("qa_nation_" + nation, MapGenerationFactType::Nation,
                MapGenerationFactStrength::Must, nation);
    addFact("qa_nation_count", MapGenerationFactType::NationCount,
            MapGenerationFactStrength::Must, "3", "3");
    MapGenerationFact sharedRiver;
    sharedRiver.id = "qa_shared_river";
    sharedRiver.type = MapGenerationFactType::SharedRiver;
    sharedRiver.strength = MapGenerationFactStrength::Must;
    sharedRiver.label = "Shared great river";
    sharedRiver.value = "river";
    sharedRiver.entityRefs = {"Aster", "Bram", "Cyr"};
    sharedRiver.sourceText = "Three named nations must share one great river.";
    sharedRiver.confidence = 1.0f;
    sharedRiver.confirmed = true;
    request.facts.push_back(std::move(sharedRiver));
    addFact("qa_capital", MapGenerationFactType::Capital,
            MapGenerationFactStrength::Must, "QA Capital");
    addFact("qa_excluded_lava", MapGenerationFactType::ExcludedElement,
            MapGenerationFactStrength::Must, "lava");

    MapGenerationConstraint capital;
    capital.id = "qa_capital";
    capital.label = "QA Capital";
    capital.featureKind = MapGeneratedFeatureKind::Settlement;
    capital.x = 48.0f * 0.52f;
    capital.y = 32.0f * 0.48f;
    capital.required = true;
    capital.locked = true;
    request.constraints.push_back(std::move(capital));

    MapGenerationOptions options = mapGenerationOptionsFromCreationRequest(request);
    options.qualityAttempts = 12;
    options.landscape = MapLandscapeProfile::LakeCountry;
    options.waterLevel = 60;
    options.mountainDensity = 30;
    options.forestDensity = 29;
    options.routeDensity = 73;
    options.tacticalDensity = 24;
    options.requestedSettlements = 4;
    options.requestedLandmarks = 2;
    options.edgeConnectionCount = 4;
    options.roadNetwork = MapRoadNetworkMode::Connected;
    mapGenApplyFactsToOptions(options);

    const MapGenerationBatchResult batch =
        generateSemanticMapCandidateBatch(options, 1);
    bool ok = true;
    ok &= require(batch.succeeded() && batch.candidates.size() == 1,
                  "The fixed 12-attempt constraint-world release case must produce a selectable map");
    if (batch.candidates.empty())
        return false;

    const MapGenerationResult &candidate = batch.candidates.front();
    ok &= require(candidate.validation.eligible &&
                      candidate.quality.criticalIssueCount == 0,
                  "The constraint-world candidate must pass hard validation");
    ok &= require(mapGenV5WorldRoadTerrainPercent(candidate.map) >= 80,
                  "Short world roads must leave the shoreline instead of consuming both endpoint exemptions");
    ok &= require(mapGenV5WorldTerritoryNaturalismPercent(candidate.map) >= 80,
                  "Constraint-world borders must follow seeded geographic friction rather than straight bisectors");

    std::vector<MapCellCoordinate> settlements;
    for (const MapGeneratedFeature &feature : candidate.map.generation.graph.features)
        if (feature.kind == MapGeneratedFeatureKind::Settlement &&
            !feature.points.empty())
            settlements.push_back({
                std::clamp(static_cast<int>(std::floor(feature.points.front().x)),
                           0, candidate.map.width - 1),
                std::clamp(static_cast<int>(std::floor(feature.points.front().y)),
                           0, candidate.map.height - 1)});
    std::vector<unsigned char> reachable(candidate.map.cells.size(), 0);
    std::queue<MapCellCoordinate> pending;
    if (!settlements.empty())
    {
        pending.push(settlements.front());
        reachable[static_cast<std::size_t>(mapCellIndex(
            candidate.map, settlements.front().x, settlements.front().y))] = 1;
    }
    while (!pending.empty())
    {
        const MapCellCoordinate current = pending.front();
        pending.pop();
        for (int offsetY = -1; offsetY <= 1; ++offsetY)
            for (int offsetX = -1; offsetX <= 1; ++offsetX)
            {
                if (offsetX == 0 && offsetY == 0)
                    continue;
                const int nextX = current.x + offsetX;
                const int nextY = current.y + offsetY;
                if (!isMapCellInside(candidate.map, nextX, nextY))
                    continue;
                const int nextIndex = mapCellIndex(candidate.map, nextX, nextY);
                if (reachable[static_cast<std::size_t>(nextIndex)])
                    continue;
                const MapCellModel *cell = mapCellAt(candidate.map, nextX, nextY);
                if (!cell || (!mapGenWayKindIsRoadLike(cell->wayKind) &&
                              cell->kind != MapTileKind::Settlement &&
                              cell->kind != MapTileKind::Bridge))
                    continue;
                reachable[static_cast<std::size_t>(nextIndex)] = 1;
                pending.push({nextX, nextY});
            }
    }
    ok &= require(settlements.size() >= 4 &&
                      std::all_of(settlements.begin(), settlements.end(),
                                  [&](const MapCellCoordinate settlement) {
                                      return reachable[static_cast<std::size_t>(
                                                 mapCellIndex(candidate.map,
                                                              settlement.x,
                                                              settlement.y))] != 0;
                                  }),
                  "Every principal place must join the same road component; a one-cell decorative approach is not enough");
    return ok;
}

MapGenerationOptions releaseQaWorldRegressionOptions(int localIndex,
                                                      std::uint32_t seed)
{
    MapCreationRequest request;
    request.intent = MapCreationIntent::WorldOverview;
    switch (localIndex % 4)
    {
    case 0: request.sizePreset = MapSizePreset::Small; break;
    case 1: request.sizePreset = MapSizePreset::Standard; break;
    case 2: request.sizePreset = MapSizePreset::Large; break;
    default:
        request.sizePreset = MapSizePreset::Custom;
        request.dimensions = {
            40 + static_cast<int>(seed % 41u),
            30 + static_cast<int>((seed / 7u) % 31u)};
        break;
    }
    request.seed = seed;
    request.generatorVersion = 5;
    request.recipeVersion = 3;
    request.styleId = "fantasy_neutral";
    request.styleVersion = 3;
    request.styleSnapshotId = mapStyleCurrentProductionSnapshotId();
    request.quality = MapGenerationQualityLevel::Draft;
    request.contentLanguage = localIndex % 2 == 0 ? "de" : "en";
    request.nameCulture = localIndex % 3 == 0
                              ? "germanic_central"
                              : "neutral_fantasy";

    MapGenerationFact preference;
    preference.id = "qa_preferred_landscape";
    preference.type = localIndex % 2 == 0
                          ? MapGenerationFactType::Forest
                          : MapGenerationFactType::MountainRange;
    preference.strength = MapGenerationFactStrength::Prefer;
    preference.label = preference.type == MapGenerationFactType::Forest
                           ? "forest"
                           : "mountain range";
    preference.value = preference.label;
    preference.sourceText = "QA typed preference";
    preference.confidence = 1.0f;
    preference.confirmed = true;
    request.facts.push_back(std::move(preference));

    if (localIndex >= 80 && localIndex < 90)
    {
        const auto addMustFact = [&](std::string id,
                                     MapGenerationFactType type,
                                     std::string label,
                                     std::string value = {}) {
            MapGenerationFact fact;
            fact.id = std::move(id);
            fact.type = type;
            fact.strength = MapGenerationFactStrength::Must;
            fact.label = std::move(label);
            fact.value = value.empty() ? fact.label : std::move(value);
            fact.sourceText = "QA typed constraint band";
            fact.confidence = 1.0f;
            fact.confirmed = true;
            request.facts.push_back(std::move(fact));
        };
        static constexpr const char *nationNames[] = {"Aster", "Bram", "Cyr"};
        for (int nationIndex = 0; nationIndex < 3; ++nationIndex)
            addMustFact("qa_nation_" + std::to_string(nationIndex),
                        MapGenerationFactType::Nation,
                        nationNames[nationIndex]);
        addMustFact("qa_nation_count", MapGenerationFactType::NationCount,
                    "3", "3");
        MapGenerationFact sharedRiver;
        sharedRiver.id = "qa_shared_river";
        sharedRiver.type = MapGenerationFactType::SharedRiver;
        sharedRiver.strength = MapGenerationFactStrength::Must;
        sharedRiver.label = "Shared great river";
        sharedRiver.value = "river";
        sharedRiver.entityRefs = {"Aster", "Bram", "Cyr"};
        sharedRiver.sourceText = "Three named nations must share one great river.";
        sharedRiver.confidence = 1.0f;
        sharedRiver.confirmed = true;
        request.facts.push_back(std::move(sharedRiver));
        addMustFact("qa_capital", MapGenerationFactType::Capital,
                    "QA Capital");
        addMustFact("qa_excluded_lava", MapGenerationFactType::ExcludedElement,
                    "lava");

        const MapDimensions dimensions = request.sizePreset == MapSizePreset::Custom
                                             ? request.dimensions
                                             : mapDimensionsForPreset(
                                                   request.intent, request.sizePreset);
        MapGenerationConstraint capital;
        capital.id = "qa_capital";
        capital.label = "QA Capital";
        capital.featureKind = MapGeneratedFeatureKind::Settlement;
        capital.x = static_cast<float>(dimensions.width) * 0.52f;
        capital.y = static_cast<float>(dimensions.height) * 0.48f;
        capital.required = true;
        capital.locked = true;
        request.constraints.push_back(std::move(capital));
    }

    MapGenerationOptions options = mapGenerationOptionsFromCreationRequest(request);
    options.qualityAttempts = 12;
    options.landscape = static_cast<MapLandscapeProfile>(localIndex % 5);
    options.waterLevel = 12 + static_cast<int>((seed / 11u) % 58u);
    options.mountainDensity = 8 + static_cast<int>((seed / 13u) % 58u);
    options.forestDensity = 10 + static_cast<int>((seed / 17u) % 60u);
    options.routeDensity = 18 + static_cast<int>((seed / 19u) % 58u);
    options.tacticalDensity = 12 + static_cast<int>((seed / 23u) % 50u);
    options.requestedSettlements = 2 + static_cast<int>(seed % 4u);
    options.requestedLandmarks = 2 + static_cast<int>((seed / 5u) % 5u);
    options.edgeConnectionCount = 2 + static_cast<int>(seed % 3u);
    options.roadNetwork = MapRoadNetworkMode::Connected;
    if (localIndex >= 80 && localIndex < 90)
    {
        options.waterLevel = std::max(options.waterLevel, 38);
        options.requestedSettlements = 4;
        options.edgeConnectionCount = 4;
    }
    mapGenApplyFactsToOptions(options);
    return options;
}

bool testV5ReleaseWorldRegressionSeedsMeetDraftBudget()
{
    bool ok = true;
    struct RegressionCase
    {
        int localIndex;
        std::uint32_t seed;
    };
    const RegressionCase cases[] = {
        {5, 969595u},
        {11, 1017109u},
        {83, 1587277u},
        {88, 1626872u},
        {89, 1634791u}};
    for (const RegressionCase &testCase : cases)
    {
        const MapGenerationOptions options = releaseQaWorldRegressionOptions(
            testCase.localIndex, testCase.seed);
        const MapGenerationBatchResult batch =
            generateSemanticMapCandidateBatch(options, 1);
        ok &= require(batch.succeeded() && batch.attemptsEvaluated == 12,
                      "Each minimized release-world regression must find a valid candidate within the unchanged 12-attempt Draft budget");
        if (!batch.candidates.empty())
            ok &= require(batch.candidates.front().validation.eligible &&
                              batch.candidates.front().quality.criticalIssueCount == 0 &&
                              mapGenQualityMeetsCandidateFloor(
                                  batch.candidates.front().map,
                                  batch.candidates.front().quality),
                          "A release-world regression may publish only a fully eligible candidate");
    }
    return ok;
}

bool testGenerationRequestContradictionsFailBeforeAttempts()
{
    bool ok = true;
    const std::string note =
        "Three nations named Alderreach, Veyra, and Kharad share a great river. "
        "There is a large capital named Crosswater.";
    const MapWorldBrief brief = MapWorldBriefInterpreter::interpret(note);

    MapGenerationOptions includeExclude = noteDrivenWorldOptions(brief, note, 48, 32, 901u);
    includeExclude.requiredElements.push_back("Ancient tower");
    includeExclude.excludedElements.push_back("Ancient tower");
    MapGenerationBatchResult result = generateSemanticMapCandidateBatch(includeExclude, 3);
    ok &= require(result.failure == MapGenerationBatchFailureCode::InvalidRequest &&
                      result.attemptsEvaluated == 0 && result.candidates.empty(),
                  "A must/include-exclude contradiction should fail before any generation attempt");
    ok &= require(!result.requestValidation.eligible && !result.requestValidation.blockers.empty(),
                  "An invalid request should expose structured request blockers");

    MapGenerationOptions nationConflict = noteDrivenWorldOptions(brief, note, 48, 32, 902u);
    for (MapGenerationFact &fact : nationConflict.facts)
        if (fact.type == MapGenerationFactType::NationCount)
            fact.label = fact.value = "2";
    result = generateSemanticMapCandidateBatch(nationConflict, 3);
    ok &= require(result.failure == MapGenerationBatchFailureCode::InvalidRequest && result.attemptsEvaluated == 0,
                  "A required nation count below the named nations should fail before generation");

    MapGenerationOptions untyped = noteDrivenWorldOptions(brief, note, 48, 32, 903u);
    MapGenerationFact mustNote;
    mustNote.id = "brief_untyped";
    mustNote.type = MapGenerationFactType::Note;
    mustNote.strength = MapGenerationFactStrength::Must;
    mustNote.sourceText = "Some relationship the deterministic interpreter did not understand.";
    untyped.facts.push_back(std::move(mustNote));
    result = generateSemanticMapCandidateBatch(untyped, 3);
    ok &= require(result.failure == MapGenerationBatchFailureCode::InvalidRequest && result.attemptsEvaluated == 0,
                  "An untyped Must note should fail closed before generation");
    return ok;
}

bool testGenerationCancellationNeverPublishesPartialCandidates()
{
    bool ok = true;
    bool cancelRequested = false;
    MapGenerationOptions options;
    options.generatorVersion = 5;
    options.creationIntent = MapCreationIntent::RegionExploration;
    options.scale = MapLevel::Region;
    options.scope = MapScope::Local;
    options.width = 64;
    options.height = 48;
    options.seed = 44017u;
    options.qualityAttempts = 12;
    options.progressCallback = [&](const MapGenerationProgress &progress) {
        if (progress.stage.find("Evaluating candidate 1/") != std::string::npos)
            cancelRequested = true;
    };
    options.cancellationRequested = [&]() { return cancelRequested; };

    const MapGenerationBatchResult cancelled = generateSemanticMapCandidateBatch(options, 3);
    ok &= require(cancelled.failure == MapGenerationBatchFailureCode::Cancelled && !cancelled.succeeded(),
                  "A cancelled deterministic search should report a distinct cancellation result");
    ok &= require(cancelled.candidates.empty() && cancelled.rejectedCandidates.empty(),
                  "Cancellation must not publish a partially evaluated candidate set");
    ok &= require(cancelled.recommendedIndex == -1,
                  "A cancelled search must never expose a recommended candidate index");
    ok &= require(cancelled.attemptsEvaluated == 1,
                  "Cancellation should report only the deterministic attempts that actually completed");

    MapGenerationOptions cancelledBeforeStart = options;
    cancelledBeforeStart.progressCallback = {};
    cancelledBeforeStart.cancellationRequested = []() { return true; };
    const MapGenerationBatchResult immediate = generateSemanticMapCandidateBatch(cancelledBeforeStart, 3);
    ok &= require(immediate.failure == MapGenerationBatchFailureCode::Cancelled &&
                      immediate.attemptsEvaluated == 0 && immediate.candidates.empty(),
                  "A request cancelled before generation must do no work and expose no design");
    ok &= require(immediate.recommendedIndex == -1,
                  "A request cancelled before generation must not recommend a design");
    return ok;
}

bool testCandidateSelectionPrefersDistinctCompositions()
{
    const auto candidate = [](const std::string &id, int score, int pattern) {
        MapGenerationResult result;
        result.map.metadata.id = id;
        result.map.width = 24;
        result.map.height = 16;
        resizeMapCells(result.map);
        for (int y = 0; y < result.map.height; ++y)
            for (int x = 0; x < result.map.width; ++x)
            {
                MapCellModel *cell = mapCellAt(result.map, x, y);
                if (pattern == 1 && x < result.map.width / 2)
                    applyMapCellTerrainDefaults(*cell, MapTileKind::Water);
                else if (pattern == 2)
                    applyMapCellTerrainDefaults(*cell, MapTileKind::Mountain);
                else
                    applyMapCellTerrainDefaults(*cell, MapTileKind::Empty);
            }
        result.quality.score = score;
        return result;
    };

    std::vector<MapGenerationResult> ranked;
    ranked.push_back(candidate("best", 100, 0));
    ranked.push_back(candidate("near_clone", 99, 0));
    ranked.push_back(candidate("river_world", 95, 1));
    ranked.push_back(candidate("mountain_world", 90, 2));
    std::vector<MapGenerationResult> selected = mapGenSelectDiverseCandidates(std::move(ranked), 3);
    bool ok = require(selected.size() == 3 && selected.front().map.metadata.id == "best",
                      "Diverse selection should keep the highest-quality design first");
    ok &= require(std::none_of(selected.begin(), selected.end(), [](const MapGenerationResult &result) {
                      return result.map.metadata.id == "near_clone";
                  }),
                  "A near-identical high-scoring clone should not displace visibly different designs");
    ok &= require(mapGenCompositionDistancePercent(selected[0].map, selected[1].map) > 20 &&
                      mapGenCompositionDistancePercent(selected[0].map, selected[2].map) > 20,
                  "Selected alternatives should have a perceptibly different coarse composition");
    return ok;
}

bool testV5NameCulturesAreDeterministicAndDistinct()
{
    bool ok = true;
    const std::array<std::string, 4> cultures = {
        "germanic_central_european", "nordic", "romance", "neutral_fantasy"};
    std::set<std::string> settlementNames;
    std::set<std::string> landmarkNames;
    for (const std::string &culture : cultures)
    {
        const std::string settlement = mapGenV4SettlementName(240529u, 3, culture);
        const std::string landmark = mapGenV4LandmarkName(240529u, 3, culture);
        ok &= require(!settlement.empty() && !landmark.empty(),
                      "Every V5 name culture must provide settlement and landmark names");
        ok &= require(settlement == mapGenV4SettlementName(240529u, 3, culture) &&
                          landmark == mapGenV4LandmarkName(240529u, 3, culture),
                      "Name-culture selection must be deterministic");
        settlementNames.insert(settlement);
        landmarkNames.insert(landmark);
    }
    ok &= require(settlementNames.size() == cultures.size() && landmarkNames.size() == cultures.size(),
                  "The four V5 name cultures must produce visibly distinct vocabularies");
    ok &= require(mapGenNormalizedNameCulture("de") == "germanic_central_european" &&
                      mapGenNormalizedNameCulture("Norse") == "nordic" &&
                      mapGenNormalizedNameCulture("romanisch") == "romance" &&
                      mapGenNormalizedNameCulture("unknown") == "neutral_fantasy",
                  "Name-culture aliases must normalize to stable persisted IDs");
    return ok;
}

bool testV4ScaleSpecificProductionPipeline()
{
    bool ok = true;
    const std::string snapshotId = mapStyleCurrentProductionSnapshotId();
    ok &= require(snapshotId.rfind("fantasy_neutral_v3_3_", 0) == 0, "V5 should pin the V3.3 corrective World-art snapshot");
    ok &= require(mapStyleProductionSnapshotAvailable() && mapStyleProductionSnapshotAvailable(snapshotId),
                  "The accepted V3 through V3.3 production asset chain should be available at runtime");

    int v3AssetCount = 0;
    int v31AssetCount = 0;
    int v32AssetCount = 0;
    int v33AssetCount = 0;
    for (const MapStylePackedRuntimeAsset &asset : mapStylePackedRuntimeAssets())
    {
        if (asset.assetClass != "production_v3" && asset.assetClass != "production_v3_1" &&
            asset.assetClass != "production_v3_2" && asset.assetClass != "production_v3_3")
            continue;
        v3AssetCount += asset.assetClass == "production_v3" ? 1 : 0;
        v31AssetCount += asset.assetClass == "production_v3_1" ? 1 : 0;
        v32AssetCount += asset.assetClass == "production_v3_2" ? 1 : 0;
        v33AssetCount += asset.assetClass == "production_v3_3" ? 1 : 0;
        ok &= require(asset.status == "accepted", "Only accepted V3 through V3.3 assets may enter the production runtime lookup");
        ok &= require(!asset.role.empty() && !asset.semanticRole.empty(), "Every production runtime asset should retain role and semantic metadata");
        ok &= require(asset.width > 0 && asset.height > 0 && asset.footprintWidth > 0 && asset.footprintHeight > 0,
                      "Every production runtime asset should retain valid image and footprint dimensions");
        ok &= require(asset.minimumZoom <= asset.maximumZoom, "Every production runtime asset should retain an ordered zoom range");
        ok &= require(std::filesystem::is_regular_file(asset.atlasPath), "Every accepted production runtime asset should resolve to a packaged image file");
        if (asset.assetClass == "production_v3_1")
        {
            ok &= require(mapStyleAssetSupportsScale(asset, MapLevel::World) && mapStyleAssetSupportsScale(asset, MapLevel::Region),
                          "The immutable V3.1 overview snapshot should retain its declared world/region scale contract");
            ok &= require(!mapStyleAssetSupportsScale(asset, MapLevel::Site) && !mapStyleAssetSupportsScale(asset, MapLevel::Tactical),
                          "V3.1 overview assets must not leak into site or tactical maps");
        }
        else if (asset.assetClass == "production_v3_2")
            ok &= require(mapStyleAssetSupportsScale(asset, MapLevel::World) && !mapStyleAssetSupportsScale(asset, MapLevel::Region) &&
                              !mapStyleAssetSupportsScale(asset, MapLevel::Site) && !mapStyleAssetSupportsScale(asset, MapLevel::Tactical),
                          "V3.2 corrective mountain assets should remain exclusive to world rendering");
        else if (asset.assetClass == "production_v3_3")
            ok &= require(mapStyleAssetSupportsScale(asset, MapLevel::World) && !mapStyleAssetSupportsScale(asset, MapLevel::Region) &&
                              !mapStyleAssetSupportsScale(asset, MapLevel::Site) && !mapStyleAssetSupportsScale(asset, MapLevel::Tactical),
                          "V3.3 corrective art must remain exclusive to world rendering");
    }
    ok &= require(v3AssetCount == 424, "The complete accepted 424-asset V3 detail snapshot should be available at runtime");
    ok &= require(v31AssetCount == 60, "The immutable accepted 60-asset V3.1 overview snapshot should remain available at runtime");
    ok &= require(v32AssetCount == 3, "The V3.2 corrective snapshot should contain exactly its three versioned world ridges");
    ok &= require(v33AssetCount == 4, "The V3.3 corrective snapshot should contain one global surface and three versioned world ridges");

    const std::string baseSnapshotId = mapStyleBaseProductionSnapshotId();
    const std::string v31SnapshotId = mapStyleV31ProductionSnapshotId();
    const std::string v32SnapshotId = mapStyleV32ProductionSnapshotId();
    ok &= require(!baseSnapshotId.empty() && baseSnapshotId != snapshotId,
                  "The immutable V3 base snapshot should remain independently addressable");
    ok &= require(!v31SnapshotId.empty() && v31SnapshotId != baseSnapshotId && v31SnapshotId != snapshotId &&
                      mapStyleProductionSnapshotAvailable(v31SnapshotId),
                  "The immutable V3.1 snapshot should remain independently addressable after the V3.3 overlay");
    ok &= require(!v32SnapshotId.empty() && v32SnapshotId != snapshotId &&
                      mapStyleProductionSnapshotAvailable(v32SnapshotId),
                  "The immutable V3.2 snapshot should remain independently addressable after the V3.3 overlay");
    ok &= require(mapStyleProductionSnapshotAvailable(baseSnapshotId),
                  "A campaign pinned to the V3 base snapshot should still resolve its original art");
    const MapStylePackedRuntimeAsset *baseWorldGrass = resolveMapStyleProductionAssetForSnapshot(
        baseSnapshotId, MapLevel::World, "terrain_material", "grass_temperate", 2, 3, 11);
    ok &= require(baseWorldGrass && baseWorldGrass->assetClass == "production_v3",
                  "A base-snapshot pin must never be silently upgraded to V3.1 art");
    const std::string unknownSnapshotId = "fantasy_neutral_unknown_snapshot";
    ok &= require(!mapStyleProductionSnapshotAvailable(unknownSnapshotId) &&
                      resolveMapStyleProductionAssetForSnapshot(unknownSnapshotId, MapLevel::World,
                          "terrain_material", "grass_temperate", 2, 3, 11) == nullptr,
                  "An unknown immutable snapshot pin must fail closed instead of substituting current art");

    const MapStylePackedRuntimeAsset *pinnedV31WorldRidge = resolveMapStyleProductionAssetForSnapshot(
        v31SnapshotId, MapLevel::World, "landscape_stamp", "connected_ridge", 7, 5, 19);
    ok &= require(pinnedV31WorldRidge && pinnedV31WorldRidge->assetClass == "production_v3_1",
                  "A V3.1 campaign pin must retain its original world-ridge art");
    const MapStylePackedRuntimeAsset *pinnedV31WorldVegetation = resolveMapStyleProductionAssetForSnapshot(
        v31SnapshotId, MapLevel::World, "vegetation_stamp", "mixed_tree_group", 7, 5, 19);
    ok &= require(pinnedV31WorldVegetation && pinnedV31WorldVegetation->assetClass == "production_v3_1",
                  "A V3.1 campaign pin must retain its original world-vegetation resolver contract");

    const MapStylePackedRuntimeAsset *worldGrass = resolveMapStyleProductionAsset(MapLevel::World, "terrain_material", "grass_temperate", 2, 3, 11);
    ok &= require(worldGrass && worldGrass->assetClass == "production_v3_1", "World materials should prefer the scale-specific V3.1 atlas");
    const MapStylePackedRuntimeAsset *repeatedWorldGrass = resolveMapStyleProductionAsset(MapLevel::World, "terrain_material", "grass_temperate", 2, 3, 11);
    ok &= require(worldGrass && repeatedWorldGrass && worldGrass->id == repeatedWorldGrass->id,
                  "Production asset selection should be deterministic for a stable scale, cell, and salt");
    const MapStylePackedRuntimeAsset *worldRidge = resolveMapStyleProductionAsset(MapLevel::World, "landscape_stamp", "connected_ridge", 7, 5, 19);
    ok &= require(worldRidge && worldRidge->assetClass == "production_v3_3" && worldRidge->footprintWidth > 1,
                  "Current World maps should use only the V3.3 corrective connected ridges");
    ok &= require(worldRidge && std::abs(worldRidge->sourceAxisRadians) > 0.01f &&
                      worldRidge->selectionWeight >= 1,
                  "V3.3 connected ridges must retain their measured source axis and deterministic selection weight");
    const MapStylePackedRuntimeAsset *pinnedV32WorldRidge = resolveMapStyleProductionAssetForSnapshot(
        v32SnapshotId, MapLevel::World, "landscape_stamp", "connected_ridge", 7, 5, 19);
    ok &= require(pinnedV32WorldRidge && pinnedV32WorldRidge->assetClass == "production_v3_2",
                  "A V3.2 campaign pin must retain its original corrective ridge art");
    const MapStylePackedRuntimeAsset *worldSurface = resolveMapStyleProductionAsset(
        MapLevel::World, "world_surface_modulation", "watercolor", 0, 0, 0, 0.6f);
    ok &= require(worldSurface && worldSurface->assetClass == "production_v3_3" &&
                      worldSurface->width > 1000 && worldSurface->height > 1000,
                  "Current World maps should resolve the immutable global watercolor surface");
    ok &= require(resolveMapStyleProductionAssetForSnapshot(
                      v32SnapshotId, MapLevel::World, "world_surface_modulation", "watercolor", 0, 0, 0, 0.6f) == nullptr,
                  "A V3.2 campaign pin must not silently receive the V3.3 watercolor surface");
    const MapStylePackedRuntimeAsset *regionRidge = resolveMapStyleProductionAsset(MapLevel::Region, "landscape_stamp", "connected_ridge", 7, 5, 19);
    ok &= require(regionRidge && regionRidge->assetClass == "production_v3_1",
                  "V3.3 Region maps should continue using the inherited V3.1 overview ridge contract");
    ok &= require(resolveMapStyleProductionAsset(MapLevel::World, "vegetation_stamp", "mixed_tree_group", 7, 5, 19) == nullptr,
                  "V3.2 World maps must suppress every per-cell vegetation-stamp lookup");
    ok &= require(resolveMapStyleProductionAsset(MapLevel::World, "terrain_material", "forest_floor", 7, 5, 21) == nullptr,
                  "V3.2 World maps must suppress the close-range fern floor material");
    ok &= require(resolveMapStyleProductionAsset(MapLevel::World, "map_symbol", "discovery", 7, 5, 23) == nullptr,
                  "V3.2 World maps must suppress the local planted discovery artwork");
    const MapStylePackedRuntimeAsset *tacticalGrass = resolveMapStyleProductionAsset(MapLevel::Tactical, "terrain_material", "grass_temperate", 2, 3, 11);
    ok &= require(tacticalGrass && tacticalGrass->assetClass == "production_v3", "Tactical maps should retain detailed V3 materials");
    const MapStylePackedRuntimeAsset *siteDungeon = resolveMapStyleProductionAsset(MapLevel::Site, "dungeon_stamp", "cave_natural", 4, 6, 3);
    ok &= require(siteDungeon && siteDungeon->assetClass == "production_v3", "Site maps should accept V3 assets tagged for dungeon rendering");
    ok &= require(resolveMapStyleProductionAsset(MapLevel::World, "tactical_prop", "barricade", 1, 1, 0) == nullptr,
                  "World maps must not resolve tactical-only production props");
    const MapStylePackedRuntimeAsset *overviewZoomGrass = resolveMapStyleProductionAsset(MapLevel::World, "terrain_material", "grass_temperate", 2, 3, 11, 0.3f);
    ok &= require(overviewZoomGrass && overviewZoomGrass->assetClass == "production_v3_1", "World overview zoom should select V3.1 materials");
    const MapStylePackedRuntimeAsset *detailZoomGrass = resolveMapStyleProductionAsset(MapLevel::World, "terrain_material", "grass_temperate", 2, 3, 11, 2.0f);
    ok &= require(detailZoomGrass && detailZoomGrass->assetClass == "production_v3", "World detail zoom should fall back to an in-range V3 material");
    ok &= require(resolveMapStyleProductionAsset(MapLevel::World, "terrain_material", "grass_temperate", 2, 3, 11, 0.1f) == nullptr,
                  "Production lookup should reject assets below their minimum zoom");
    ok &= require(resolveMapStyleProductionAsset(MapLevel::Tactical, "terrain_material", "grass_temperate", 2, 3, 11, 4.1f) == nullptr,
                  "Production lookup should reject assets above their maximum zoom");

    const MapRenderScaleProfile &worldProfile = mapRenderScaleProfile(MapLevel::World);
    const MapRenderScaleProfile &regionProfile = mapRenderScaleProfile(MapLevel::Region);
    const MapRenderScaleProfile &siteProfile = mapRenderScaleProfile(MapLevel::Site);
    const MapRenderScaleProfile &tacticalProfile = mapRenderScaleProfile(MapLevel::Tactical);
    ok &= require(worldProfile.materialCellsPerRepeat > regionProfile.materialCellsPerRepeat &&
                      regionProfile.materialCellsPerRepeat > siteProfile.materialCellsPerRepeat &&
                      siteProfile.materialCellsPerRepeat > tacticalProfile.materialCellsPerRepeat,
                  "Material repetition should become progressively finer toward tactical scale");
    ok &= require(worldProfile.symbolDensity < regionProfile.symbolDensity && regionProfile.symbolDensity < siteProfile.symbolDensity &&
                      siteProfile.symbolDensity < tacticalProfile.symbolDensity,
                  "Symbol density should become progressively richer toward tactical scale");
    ok &= require(worldProfile.riverWidthCells >= 0.18f && worldProfile.riverWidthCells <= 0.28f &&
                      worldProfile.riverWidthCells < regionProfile.riverWidthCells,
                   "World rivers should read as restrained cartographic lines rather than tactical water bands");
    ok &= require(worldProfile.roadWidthCells <= 0.18f &&
                      worldProfile.roadWidthCells < regionProfile.roadWidthCells,
                  "World roads should remain thinner than regional travel routes");
    ok &= require(gm::map_render::kOverviewRoadShoulder.a <
                          gm::map_render::kOverviewRoadSurface.a &&
                      gm::map_render::kOverviewRoadSurface.a <
                          gm::map_render::kOverviewSettlementPaper.a &&
                      gm::map_render::kOverviewRiverBank.a <
                          gm::map_render::kOverviewRiverWater.a &&
                      gm::map_render::kOverviewV33MountainTint.a <
                          gm::map_render::kOverviewSettlementPaper.a,
                  "World cartographic inks should keep roads, river banks and macro mountains subordinate to settlements and water");
    ok &= require(gm::map_render::kOverviewForestBase.r > 50 &&
                      gm::map_render::kOverviewForestBase.g >
                          gm::map_render::kOverviewForestBase.r &&
                      gm::map_render::kOverviewForestInk.g + 30 < gm::map_render::kOverviewForestBase.g &&
                      gm::map_render::kOverviewForestHighlight.g > gm::map_render::kOverviewForestBase.g &&
                      gm::map_render::kOverviewRoadWidthScale >= 1.0f &&
                      gm::map_render::kOverviewRoadWidthScale <= 1.2f,
                  "World forests and roads should use restrained readable cartographic styling");
    ok &= require(std::abs(gm::map_render::overviewRiverWidthFactor(0u, 9u) - 0.78f) < 0.001f &&
                      std::abs(gm::map_render::overviewRiverWidthFactor(8u, 9u) - 1.0f) < 0.001f &&
                      gm::map_render::overviewRiverWidthFactor(4u, 9u) >
                          gm::map_render::overviewRiverWidthFactor(2u, 9u),
                  "World river rendering should widen deterministically from source to mouth");
    ok &= require(worldProfile.settlementStampCells >= 2.9f &&
                      worldProfile.settlementStampCells > regionProfile.settlementStampCells,
                   "World settlement symbols should remain readable at fit-to-map scale");
    ok &= require(!worldProfile.allowIndividualBuildings && !regionProfile.allowIndividualBuildings && siteProfile.allowIndividualBuildings,
                  "Individual buildings should begin at site scale");
    ok &= require(!siteProfile.allowTacticalProps && tacticalProfile.allowTacticalProps && !siteProfile.showGridByDefault && tacticalProfile.showGridByDefault,
                  "Tactical props and the default grid should remain exclusive to tactical scale");
    ok &= require(!mapRenderUsesDetailedTerrainMaterials(MapLevel::Tactical, 16.0f) &&
                      mapRenderUsesDetailedTerrainMaterials(MapLevel::Tactical, 20.0f),
                  "Fit-to-window tactical maps should skip unreadable per-cell texture work and restore it when zoomed in");
    ok &= require(!mapRenderUsesDetailedTerrainMaterials(MapLevel::World, 10.0f) &&
                       !mapRenderUsesDetailedTerrainMaterials(MapLevel::World, 48.0f) &&
                       !mapRenderUsesDetailedTerrainMaterials(MapLevel::Region, 14.0f) &&
                       mapRenderUsesDetailedTerrainMaterials(MapLevel::Region, 18.0f),
                  "World must stay on the seamless continuous field while Region detail remains zoom-gated");

    MapModel overviewMountains;
    overviewMountains.width = 40;
    overviewMountains.height = 18;
    overviewMountains.atlasLevel = MapLevel::World;
    resizeMapCells(overviewMountains);
    for (int x = 3; x <= 14; ++x)
    {
        const int y = 4 + (x - 3) / 4;
        overviewMountains.cells[static_cast<std::size_t>(mapCellIndex(overviewMountains, x, y))].kind =
            MapTileKind::Mountain;
    }
    // An isolated cell is semantic relief, not permission to enlarge one
    // tactical peak into a macro painting.
    overviewMountains.cells[static_cast<std::size_t>(mapCellIndex(overviewMountains, 32, 14))].kind =
        MapTileKind::Mountain;
    const std::vector<gm::map_render::OverviewMountainStamp> mountainStamps =
        gm::map_render::buildOverviewMountainStamps(overviewMountains);
    const std::vector<gm::map_render::OverviewMountainStamp> repeatedMountainStamps =
        gm::map_render::buildOverviewMountainStamps(overviewMountains);
    ok &= require(mountainStamps.size() == 1u,
                  "A compact connected World range should produce one macro stamp, not repeated per-cell art");
    ok &= require(repeatedMountainStamps.size() == mountainStamps.size() &&
                      !mountainStamps.empty() &&
                      repeatedMountainStamps.front().centerX == mountainStamps.front().centerX &&
                      repeatedMountainStamps.front().centerY == mountainStamps.front().centerY &&
                      repeatedMountainStamps.front().angleRadians == mountainStamps.front().angleRadians,
                  "Overview mountain placement should be deterministic");

    MapModel longMountainRange;
    longMountainRange.width = 44;
    longMountainRange.height = 12;
    longMountainRange.atlasLevel = MapLevel::World;
    resizeMapCells(longMountainRange);
    for (int x = 3; x < 39; ++x)
        longMountainRange.cells[static_cast<std::size_t>(mapCellIndex(longMountainRange, x, 6))].kind =
            MapTileKind::Mountain;
    const std::vector<gm::map_render::OverviewMountainStamp> longRangeStamps =
        gm::map_render::buildOverviewMountainStamps(longMountainRange);
    ok &= require(longRangeStamps.size() == 2u,
                  "Only a very long connected World range should use two overlapping macro sections");
    longMountainRange.atlasLevel = MapLevel::Tactical;
    ok &= require(gm::map_render::buildOverviewMountainStamps(longMountainRange).empty(),
                  "Tactical maps must not use the overview mountain placement policy");

    MapModel semanticMountainRange;
    semanticMountainRange.width = 42;
    semanticMountainRange.height = 20;
    semanticMountainRange.atlasLevel = MapLevel::World;
    resizeMapCells(semanticMountainRange);
    MapGeneratedFeature rangeFeature;
    rangeFeature.id = "range_semantic_spine";
    rangeFeature.kind = MapGeneratedFeatureKind::MountainRange;
    rangeFeature.width = 2.4f;
    rangeFeature.points = {{4.0f, 12.0f}, {12.0f, 7.0f}, {23.0f, 8.0f}, {35.0f, 4.0f}};
    semanticMountainRange.generation.graph.features.push_back(rangeFeature);
    const std::vector<gm::map_render::OverviewMountainStamp> semanticStamps =
        gm::map_render::buildOverviewMountainStamps(semanticMountainRange);
    ok &= require(!semanticStamps.empty() && semanticStamps.size() <= 5u,
                  "A World MountainRange feature should drive a small coherent chain of macro stamps");
    ok &= require(!semanticStamps.empty() && semanticStamps.front().centerX > 4.0f &&
                      semanticStamps.back().centerX < 35.0f,
                  "Semantic ridge stamps should follow the range spine rather than unrelated raster cells");
    bool overlappingRangeSections = true;
    for (std::size_t index = 1; index < semanticStamps.size(); ++index)
    {
        const auto &previous = semanticStamps[index - 1];
        const auto &current = semanticStamps[index];
        const float dx = previous.centerX - current.centerX;
        const float dy = previous.centerY - current.centerY;
        const float paintedReach = (previous.lengthCells + current.lengthCells) *
            gm::map_render::kOverviewMountainLengthScale * 0.5f;
        overlappingRangeSections &= std::sqrt(dx * dx + dy * dy) < paintedReach;
    }
    ok &= require(overlappingRangeSections,
                  "Adjacent mountain illustrations must overlap along the semantic spine after render scaling");
    MapModel coastalRidge = semanticMountainRange;
    for (int y = 0; y < coastalRidge.height; ++y)
        for (int x = 30; x < coastalRidge.width; ++x)
            coastalRidge.cells[static_cast<std::size_t>(mapCellIndex(coastalRidge, x, y))].kind = MapTileKind::Water;
    coastalRidge.generation.graph.features.front().points = {{5.0f, 10.0f}, {28.5f, 10.0f}};
    const auto coastalStamps = gm::map_render::buildOverviewMountainStamps(coastalRidge);
    bool reliefStaysOnLand = !coastalStamps.empty();
    for (const auto &stamp : coastalStamps)
    {
        const float paintedLength = stamp.lengthCells * gm::map_render::kOverviewMountainLengthScale;
        const float paintedWidth = stamp.widthCells * gm::map_render::kOverviewV33MountainWidthScale;
        reliefStaysOnLand &= stamp.centerX + std::sqrt(paintedLength * paintedLength + paintedWidth * paintedWidth) * 0.5f < 30.0f;
    }
    ok &= require(reliefStaysOnLand,
                  "A coastal ridge must taper its entire rotated painting footprint before reaching the ocean");
    const std::vector<gm::map_render::OverviewRidgePath> semanticRidges =
        gm::map_render::buildOverviewRidgePaths(semanticMountainRange);
    const std::vector<gm::map_render::OverviewRidgePath> repeatedSemanticRidges =
        gm::map_render::buildOverviewRidgePaths(semanticMountainRange);
    ok &= require(semanticRidges.size() == 1u &&
                      !semanticRidges.front().spine.empty() &&
                      semanticRidges.front().marks.size() >= 8u,
                  "A World MountainRange should produce one coherent cartographic ridge with repeated hachures");
    ok &= require(!semanticRidges.empty() && !repeatedSemanticRidges.empty() &&
                      !semanticRidges.front().marks.empty() &&
                      !repeatedSemanticRidges.front().marks.empty() &&
                      repeatedSemanticRidges.size() == semanticRidges.size() &&
                      repeatedSemanticRidges.front().marks.size() ==
                          semanticRidges.front().marks.size() &&
                      repeatedSemanticRidges.front().marks.front().peak.x ==
                          semanticRidges.front().marks.front().peak.x &&
                      repeatedSemanticRidges.front().marks.front().peak.y ==
                          semanticRidges.front().marks.front().peak.y,
                  "World ridge hachures should be deterministic for a map seed and feature graph");
    semanticMountainRange.atlasLevel = MapLevel::Region;
    ok &= require(gm::map_render::buildOverviewRidgePaths(semanticMountainRange).empty(),
                  "Cartographic ridge glyphs are a World-only replacement for pasted overview assets");
    semanticMountainRange.atlasLevel = MapLevel::World;
    const auto townGlyph = gm::map_render::overviewSettlementGlyphStyle(
        gm::map_render::OverviewSettlementGlyphKind::Town);
    const auto cityGlyph = gm::map_render::overviewSettlementGlyphStyle(
        gm::map_render::OverviewSettlementGlyphKind::City);
    const auto capitalGlyph = gm::map_render::overviewSettlementGlyphStyle(
        gm::map_render::OverviewSettlementGlyphKind::Capital);
    ok &= require(townGlyph.outerRadiusCells < cityGlyph.outerRadiusCells &&
                      cityGlyph.outerRadiusCells < capitalGlyph.outerRadiusCells &&
                      gm::map_render::overviewSettlementGlyphKind("capital") ==
                          gm::map_render::OverviewSettlementGlyphKind::Capital &&
                      gm::map_render::overviewSettlementGlyphKind("village") ==
                          gm::map_render::OverviewSettlementGlyphKind::Town,
                  "World settlement glyphs should expose a deliberate town, city and capital hierarchy");

    MapModel forestPatternMap;
    forestPatternMap.width = 20;
    forestPatternMap.height = 12;
    forestPatternMap.atlasLevel = MapLevel::World;
    forestPatternMap.generation.recipe.seed = 930002u;
    resizeMapCells(forestPatternMap);
    for (int y = 2; y <= 9; ++y)
        for (int x = 3; x <= 16; ++x)
            forestPatternMap.cells[static_cast<std::size_t>(
                mapCellIndex(forestPatternMap, x, y))].kind = MapTileKind::Forest;
    MapGeneratedFeature forestRoad;
    forestRoad.id = "forest_clearance_road";
    forestRoad.kind = MapGeneratedFeatureKind::Road;
    forestRoad.points = {{2.0f, 6.5f}, {18.0f, 6.5f}};
    forestPatternMap.generation.graph.features.push_back(forestRoad);
    const std::vector<gm::map_render::OverviewForestGlyph> forestGlyphs =
        gm::map_render::buildOverviewForestGlyphs(forestPatternMap);
    const std::vector<gm::map_render::OverviewForestGlyph> repeatedForestGlyphs =
        gm::map_render::buildOverviewForestGlyphs(forestPatternMap);
    const auto forestTerrainHash = gm::map_render::overviewTerrainContentHash(forestPatternMap);
    MapModel changedTerrain = forestPatternMap;
    changedTerrain.cells.front().kind = MapTileKind::Water;
    ok &= require(gm::map_render::overviewTerrainContentHash(changedTerrain) != forestTerrainHash,
                  "Atlas terrain caches must invalidate on an unsaved cell edit without a revision bump");
    changedTerrain = forestPatternMap;
    changedTerrain.cells.front().wayKind = MapWayKind::River;
    ok &= require(gm::map_render::overviewTerrainContentHash(changedTerrain) != forestTerrainHash,
                  "Atlas terrain caches must invalidate when river occupancy changes");
    changedTerrain = forestPatternMap;
    ++changedTerrain.generation.recipe.seed;
    ok &= require(gm::map_render::overviewTerrainContentHash(changedTerrain) != forestTerrainHash,
                  "Atlas terrain caches must invalidate seed-dependent organic sampling");
    const std::vector<gm::map_render::OverviewForestHachure> forestHachures =
        gm::map_render::buildOverviewForestHachures(forestPatternMap);
    const std::vector<gm::map_render::OverviewForestHachure> repeatedForestHachures =
        gm::map_render::buildOverviewForestHachures(forestPatternMap);
    bool forestGlyphsRespectClearance = !forestGlyphs.empty();
    for (const gm::map_render::OverviewForestGlyph &glyph : forestGlyphs)
        forestGlyphsRespectClearance = forestGlyphsRespectClearance &&
                                      std::abs(glyph.center.y - 6.5f) > 0.47f;
    ok &= require(forestGlyphs.size() == repeatedForestGlyphs.size() &&
                      forestGlyphs.size() >= 18u && forestGlyphs.size() <= 48u &&
                      forestGlyphsRespectClearance,
                  "World forests should use grouped deterministic crowns while clearing roads");
    ok &= require(forestHachures.size() == forestGlyphs.size() * 2u &&
                      forestHachures.size() == repeatedForestHachures.size() &&
                      !forestHachures.empty() &&
                      forestHachures.front().start.x == repeatedForestHachures.front().start.x &&
                      forestHachures.front().crest.y == repeatedForestHachures.front().crest.y &&
                      forestHachures.front().end.x == repeatedForestHachures.front().end.x,
                  "World forest texture should use deterministic shared hachure geometry");

    MapModel cartographicRoadMap;
    cartographicRoadMap.width = 24;
    cartographicRoadMap.height = 12;
    cartographicRoadMap.atlasLevel = MapLevel::World;
    cartographicRoadMap.generation.recipe.seed = 930002u;
    resizeMapCells(cartographicRoadMap);
    MapGeneratedFeature straightRoad;
    straightRoad.id = "world_road_straight_regression";
    straightRoad.kind = MapGeneratedFeatureKind::Road;
    straightRoad.points = {{2.0f, 5.0f}, {21.0f, 5.0f}};
    const std::vector<MapFeaturePoint> cartographicRoad =
        gm::map_render::overviewCartographicCurvePoints(cartographicRoadMap, straightRoad);
    const std::vector<MapFeaturePoint> repeatedCartographicRoad =
        gm::map_render::overviewCartographicCurvePoints(cartographicRoadMap, straightRoad);
    bool roadDeterministic = cartographicRoad.size() == repeatedCartographicRoad.size();
    bool roadHasSubtleDrift = false;
    for (std::size_t index = 0; index < cartographicRoad.size() && roadDeterministic; ++index)
    {
        roadDeterministic = cartographicRoad[index].x == repeatedCartographicRoad[index].x &&
                            cartographicRoad[index].y == repeatedCartographicRoad[index].y;
        if (index > 0 && index + 1 < cartographicRoad.size() &&
            std::abs(cartographicRoad[index].y - 5.0f) > 0.01f)
            roadHasSubtleDrift = true;
    }
    ok &= require(cartographicRoad.size() > straightRoad.points.size() && roadHasSubtleDrift,
                  "World roads should gain restrained cartographic bends without changing their topology");
    ok &= require(roadDeterministic && !cartographicRoad.empty() &&
                      cartographicRoad.front().x == straightRoad.points.front().x &&
                      cartographicRoad.front().y == straightRoad.points.front().y &&
                      cartographicRoad.back().x == straightRoad.points.back().x &&
                      cartographicRoad.back().y == straightRoad.points.back().y,
                  "Cartographic road shaping must be deterministic and preserve graph endpoints");
    cartographicRoadMap.atlasLevel = MapLevel::Region;
    const std::vector<MapFeaturePoint> regionRoad =
        gm::map_render::overviewCartographicCurvePoints(cartographicRoadMap, straightRoad);
    ok &= require(regionRoad.size() > straightRoad.points.size() &&
                      regionRoad.front().x == straightRoad.points.front().x &&
                      regionRoad.back().x == straightRoad.points.back().x,
                  "Region routes should share endpoint-preserving atlas shaping");
    straightRoad.locked = true;
    const auto lockedRoad = gm::map_render::overviewCartographicCurvePoints(cartographicRoadMap, straightRoad);
    ok &= require(lockedRoad.size() == straightRoad.points.size() &&
                      lockedRoad.front().x == straightRoad.points.front().x &&
                      lockedRoad.back().y == straightRoad.points.back().y,
                  "A locked authored route must render its exact control geometry");
    straightRoad.locked = false;
    for (auto &cell : cartographicRoadMap.cells)
        cell.kind = MapTileKind::Water;
    const auto wetRoad = gm::map_render::overviewCartographicCurvePoints(cartographicRoadMap, straightRoad);
    ok &= require(std::all_of(wetRoad.begin(), wetRoad.end(), [](const MapFeaturePoint &point) {
                      return point.y == 5.0f;
                  }), "Cartographic shaping must not invent route bends into open water");
    for (auto &cell : cartographicRoadMap.cells)
        cell.kind = MapTileKind::Empty;
    MapGeneratedFeature bridgeAnchor;
    bridgeAnchor.id = "visual_bridge_anchor";
    bridgeAnchor.kind = MapGeneratedFeatureKind::Bridge;
    bridgeAnchor.points = {{10.0f, 4.0f}, {10.0f, 6.0f}};
    cartographicRoadMap.generation.graph.features.push_back(bridgeAnchor);
    const auto bridgeRoad = gm::map_render::overviewCartographicCurvePoints(cartographicRoadMap, straightRoad);
    ok &= require(std::all_of(bridgeRoad.begin(), bridgeRoad.end(), [](const MapFeaturePoint &point) {
                      return std::abs(point.x - 10.0f) >= 1.4f || point.y == 5.0f;
                  }), "A cartographic bend must keep a bridge approach aligned");

    MapModel coastMap;
    coastMap.width = 14;
    coastMap.height = 10;
    coastMap.atlasLevel = MapLevel::World;
    coastMap.generation.recipe.seed = 930002u;
    resizeMapCells(coastMap);
    for (MapCellModel &cell : coastMap.cells)
        cell.kind = MapTileKind::Water;
    for (int y = 2; y <= 7; ++y)
        for (int x = 3; x <= 10; ++x)
            coastMap.cells[static_cast<std::size_t>(mapCellIndex(coastMap, x, y))].kind = MapTileKind::Empty;
    MapCellModel &projectedRiverCell =
        coastMap.cells[static_cast<std::size_t>(mapCellIndex(coastMap, 6, 4))];
    projectedRiverCell.kind = MapTileKind::Water;
    projectedRiverCell.wayKind = MapWayKind::River;
    MapCellModel &projectedRiverShoulder =
        coastMap.cells[static_cast<std::size_t>(mapCellIndex(coastMap, 7, 4))];
    projectedRiverShoulder.kind = MapTileKind::Water;
    projectedRiverShoulder.wayKind = MapWayKind::None;
    MapCellModel &projectedDiagonalContinuation =
        coastMap.cells[static_cast<std::size_t>(mapCellIndex(coastMap, 7, 5))];
    projectedDiagonalContinuation.kind = MapTileKind::Water;
    projectedDiagonalContinuation.wayKind = MapWayKind::River;
    ok &= require(gm::map_render::overviewGroundKindForRender(coastMap, 6, 4) != MapTileKind::Water &&
                      gm::map_render::overviewGroundKindForRender(coastMap, 7, 4) != MapTileKind::Water &&
                      gm::map_render::overviewGroundKindForRender(coastMap, 0, 0) == MapTileKind::Water,
                  "World terrain should hide coarse river projection cells and their raster shoulders without turning actual ocean into land");
    const std::vector<MapTileKind> cachedGroundProjection =
        gm::map_render::buildOverviewGroundProjection(coastMap);
    ok &= require(gm::map_render::overviewGroundKindFromProjection(
                          coastMap, cachedGroundProjection, 7, 4) ==
                          gm::map_render::overviewGroundKindForRender(coastMap, 7, 4),
                  "The per-map overview ground cache should preserve the river-shoulder policy exactly");
    ok &= require(gm::map_render::kOverviewCoastBlendRadiusCells >
                          gm::map_render::kOverviewTerrainBlendRadiusCells &&
                      gm::map_render::kOverviewCoastBlendRadiusCells >= 1.5f &&
                      gm::map_render::kOverviewCoastBlendRadiusCells <= 2.5f &&
                      gm::map_render::kOverviewCoastWaterShadow.a > 0 &&
                      gm::map_render::kOverviewCoastWaterShadow.a <= 60 &&
                      gm::map_render::kOverviewCoastLine.a > 0 &&
                      gm::map_render::kOverviewCoastLandHighlight.a > 0 &&
                      gm::map_render::kOverviewCoastLandHighlight.a <= 100 &&
                      gm::map_render::overviewSharpenLandCoverage(0.46f) == 0.0f &&
                      gm::map_render::overviewSharpenLandCoverage(0.54f) == 1.0f,
                  "World coastline should have a crisp edge with a readable ink contour and subdued shoreline shading");
    const gm::map_render::OverviewSamplePoint coastSampleA =
        gm::map_render::overviewOrganicTerrainSample(coastMap, 4.25f, 3.75f);
    const gm::map_render::OverviewSamplePoint coastSampleB =
        gm::map_render::overviewOrganicTerrainSample(coastMap, 4.25f, 3.75f);
    ok &= require(std::abs(coastSampleA.x - coastSampleB.x) < 0.00001f &&
                      std::abs(coastSampleA.y - coastSampleB.y) < 0.00001f &&
                      gm::map_render::overviewTerrainGrain(coastMap, 4.25f, 3.75f) ==
                          gm::map_render::overviewTerrainGrain(coastMap, 4.25f, 3.75f) &&
                      gm::map_render::overviewSharpenLandCoverage(0.0f) == 0.0f &&
                      std::abs(gm::map_render::overviewSharpenLandCoverage(0.5f) - 0.5f) < 0.0001f &&
                      gm::map_render::overviewSharpenLandCoverage(1.0f) == 1.0f,
                  "World coastline reconstruction should be deterministic and sharpen its smooth sub-cell contour");
    const std::vector<gm::map_render::OverviewCoastSegment> coastSegments =
        gm::map_render::buildOverviewCoastSegments(coastMap);
    const std::vector<gm::map_render::OverviewCoastSegment> repeatedCoastSegments =
        gm::map_render::buildOverviewCoastSegments(coastMap);
    ok &= require(!coastSegments.empty() &&
                      repeatedCoastSegments.size() == coastSegments.size(),
                  "A World landmass should produce a deterministic cartographic coastline");
    bool deterministicCoast = coastSegments.size() == repeatedCoastSegments.size();
    bool normalsPointToLand = true;
    for (std::size_t index = 0; index < coastSegments.size() && deterministicCoast; ++index)
    {
        const gm::map_render::OverviewCoastSegment &segment = coastSegments[index];
        const gm::map_render::OverviewCoastSegment &repeated = repeatedCoastSegments[index];
        deterministicCoast = segment.startX == repeated.startX &&
                             segment.startY == repeated.startY &&
                             segment.endX == repeated.endX &&
                             segment.endY == repeated.endY &&
                             segment.landNormalX == repeated.landNormalX &&
                             segment.landNormalY == repeated.landNormalY;
        const float middleX = (segment.startX + segment.endX) * 0.5f;
        const float middleY = (segment.startY + segment.endY) * 0.5f;
        const float landSide = gm::map_render::overviewLandCoverage(
            coastMap, middleX + segment.landNormalX * 0.18f,
            middleY + segment.landNormalY * 0.18f);
        const float waterSide = gm::map_render::overviewLandCoverage(
            coastMap, middleX - segment.landNormalX * 0.18f,
            middleY - segment.landNormalY * 0.18f);
        normalsPointToLand = normalsPointToLand && landSide + 0.0001f >= waterSide;
    }
    ok &= require(deterministicCoast,
                  "Overview coastline geometry should be bit-stable for the same map and seed");
    ok &= require(normalsPointToLand,
                  "Overview coastline normals should point toward the land highlight side");
    for (MapCellModel &cell : coastMap.cells)
        cell.kind = MapTileKind::Water;
    ok &= require(gm::map_render::buildOverviewCoastSegments(coastMap).empty(),
                  "An all-water World should not invent a coastline");
    for (MapCellModel &cell : coastMap.cells)
        cell.kind = MapTileKind::Empty;
    ok &= require(gm::map_render::buildOverviewCoastSegments(coastMap).empty(),
                  "An all-land World should not draw a false internal coastline");

    // The authoritative World river is rendered from feature-graph geometry.
    // Its coarse Water raster (including diagonal helper cells) is gameplay
    // projection only and must therefore not cut a second coastline through an
    // otherwise continuous landmass.
    for (MapCellModel &cell : coastMap.cells)
    {
        cell.kind = MapTileKind::Empty;
        cell.wayKind = MapWayKind::None;
    }
    for (int x = 2; x <= 11; ++x)
    {
        MapCellModel *riverCell = mapCellAt(coastMap, x, 5);
        riverCell->kind = MapTileKind::Water;
        riverCell->wayKind = MapWayKind::River;
    }
    MapCellModel *diagonalContinuation = mapCellAt(coastMap, 6, 4);
    diagonalContinuation->kind = MapTileKind::Water;
    diagonalContinuation->wayKind = MapWayKind::River;
    MapCellModel *diagonalShoulder = mapCellAt(coastMap, 7, 4);
    diagonalShoulder->kind = MapTileKind::Water;
    diagonalShoulder->wayKind = MapWayKind::None;
    ok &= require(gm::map_render::overviewGroundKindForRender(coastMap, 6, 5) !=
                          MapTileKind::Water &&
                      gm::map_render::overviewGroundKindForRender(coastMap, 7, 4) !=
                          MapTileKind::Water &&
                      gm::map_render::buildOverviewCoastSegments(coastMap).empty(),
                  "World river raster cells and diagonal helpers must not create an internal coastline in the rendered ground domain");
    ok &= require(mapGenV5WorldDiagonalRiverHelperCell(coastMap, 7, 4) &&
                      !mapGenV5WorldOpenWaterAt(coastMap, 7, 4) &&
                      mapGenV5WorldOpenWaterDistance(coastMap, 7, 4, 1) > 1,
                  "Hydrology and road coast-distance gates must classify the exact diagonal river shoulder as projection water, not open sea");

    MapModel politicalMap;
    politicalMap.width = 12;
    politicalMap.height = 8;
    politicalMap.atlasLevel = MapLevel::World;
    resizeMapCells(politicalMap);
    for (MapCellModel &cell : politicalMap.cells)
        cell.kind = MapTileKind::Empty;
    MapGeneratedFeature westernTerritory;
    westernTerritory.id = "territory_west";
    westernTerritory.kind = MapGeneratedFeatureKind::TerrainRegion;
    westernTerritory.materialId = "fantasy_neutral.territory.0";
    westernTerritory.closed = true;
    westernTerritory.points = {{0.0f, 0.0f}, {6.0f, 0.0f},
                               {6.0f, 8.0f}, {0.0f, 8.0f}};
    MapGeneratedFeature easternTerritory = westernTerritory;
    easternTerritory.id = "territory_east";
    easternTerritory.materialId = "fantasy_neutral.territory.1";
    easternTerritory.points = {{6.0f, 0.0f}, {12.0f, 0.0f},
                               {12.0f, 8.0f}, {6.0f, 8.0f}};
    politicalMap.generation.graph.features = {westernTerritory, easternTerritory};
    const gm::map_render::OverviewPoliticalOwnerProjection politicalOwners =
        gm::map_render::buildOverviewPoliticalOwnerProjection(politicalMap);
    const std::vector<gm::map_render::OverviewPoliticalBorderRun> politicalBorders =
        gm::map_render::buildOverviewPoliticalBorderRuns(politicalMap);
    const std::vector<gm::map_render::OverviewPoliticalBorderRun> repeatedPoliticalBorders =
        gm::map_render::buildOverviewPoliticalBorderRuns(politicalMap);
    ok &= require(politicalOwners.territoryCount == 2 &&
                      politicalOwners.owners[static_cast<std::size_t>(mapCellIndex(politicalMap, 2, 3))] !=
                          politicalOwners.owners[static_cast<std::size_t>(mapCellIndex(politicalMap, 9, 3))],
                  "World political rendering should assign each land cell to one stable territory owner");
    ok &= require(politicalBorders.size() == 1u &&
                      repeatedPoliticalBorders.size() == politicalBorders.size() &&
                      politicalBorders.front().points.size() >= 2u,
                  "Two adjacent territories should produce one deterministic shared internal border, not two polygon outlines");
    bool internalBorderOnly = !politicalBorders.empty();
    if (!politicalBorders.empty())
        for (const MapFeaturePoint &point : politicalBorders.front().points)
            internalBorderOnly = internalBorderOnly && std::abs(point.x - 6.0f) < 0.01f;
    ok &= require(internalBorderOnly,
                  "Shared political borders should follow owner changes without tracing the outside map edge");
    const std::vector<gm::map_render::OverviewPoliticalStrokeSegment> politicalDashes =
        politicalBorders.empty()
            ? std::vector<gm::map_render::OverviewPoliticalStrokeSegment>{}
            : gm::map_render::buildOverviewPoliticalDashes(politicalBorders.front());
    ok &= require(politicalDashes.size() >= 4u,
                  "Continental political borders should render as a restrained dashed map convention");
    ok &= require(gm::map_render::overviewPoliticalBorderEndpointIsAnchored(
                      politicalOwners, {6.0f, 0.0f}) &&
                      !gm::map_render::overviewPoliticalBorderEndpointIsAnchored(
                          politicalOwners, {6.0f, 4.0f}),
                  "Open political borders should terminate only at a coast/map edge or a true multi-owner junction");

    MapModel politicalGapMap = politicalMap;
    politicalGapMap.generation.graph.features[0].points =
        {{0.0f, 0.0f}, {5.0f, 0.0f}, {5.0f, 8.0f}, {0.0f, 8.0f}};
    politicalGapMap.generation.graph.features[1].points =
        {{7.0f, 0.0f}, {12.0f, 0.0f}, {12.0f, 8.0f}, {7.0f, 8.0f}};
    const gm::map_render::OverviewPoliticalOwnerProjection gapOwners =
        gm::map_render::buildOverviewPoliticalOwnerProjection(politicalGapMap);
    ok &= require(std::all_of(gapOwners.owners.begin(), gapOwners.owners.end(),
                              [](int owner) { return owner >= 0; }),
                  "Small polygon cracks inside a claimed landmass should inherit the nearest owner instead of terminating a border in open land");

    coastMap.atlasLevel = MapLevel::Tactical;
    ok &= require(gm::map_render::buildOverviewCoastSegments(coastMap).empty(),
                  "Tactical maps must not use the World cartographic coastline treatment");

    MapModel inferredGround;
    inferredGround.width = 3;
    inferredGround.height = 3;
    resizeMapCells(inferredGround);
    for (MapCellModel &cell : inferredGround.cells)
        cell.kind = MapTileKind::Dirt;
    inferredGround.cells[static_cast<std::size_t>(mapCellIndex(inferredGround, 1, 1))].kind = MapTileKind::Building;
    ok &= require(mapVisualGroundKind(inferredGround, 1, 1) == MapTileKind::Dirt,
                  "Building symbols should inherit nearby ground instead of cutting a grass square into a district");
    inferredGround.cells[static_cast<std::size_t>(mapCellIndex(inferredGround, 1, 1))].kind = MapTileKind::Bridge;
    inferredGround.cells[static_cast<std::size_t>(mapCellIndex(inferredGround, 1, 0))].kind = MapTileKind::Water;
    ok &= require(mapVisualGroundKind(inferredGround, 1, 1) == MapTileKind::Water,
                  "Bridge symbols should preserve water below the crossing artwork");

    std::set<std::string> settlementLayouts;
    bool foundAlternativeRiverOutlet = false;
    for (std::uint32_t seed = 9100; seed < 9148; ++seed)
    {
        MapGenerationOptions worldOptions;
        worldOptions.generatorVersion = 4;
        worldOptions.scope = MapScope::World;
        worldOptions.scale = MapLevel::World;
        worldOptions.width = 42;
        worldOptions.height = 28;
        worldOptions.seed = seed;
        worldOptions.requestedSettlements = 4;
        worldOptions.requestedLandmarks = 3;
        worldOptions.qualityAttempts = 1;
        const MapGenerationResult world = generateSemanticMap(worldOptions);
        ok &= require(world.map.generation.recipe.styleSnapshotId == snapshotId, "Generated V4 maps should persist the exact production snapshot");
        ok &= require(world.map.generation.recipe.generatorVersion == 4 && world.map.generation.recipe.styleVersion == 3,
                      "Generated V4 maps should persist their generator and production style versions");
        ok &= require(world.map.atlasLevel == MapLevel::World && world.map.generation.recipe.scale == MapLevel::World,
                      "Generated V4 maps should preserve the requested atlas scale in the map and recipe");
        ok &= require(world.map.generation.continuousRendering && world.quality.assetCoveragePercent == 100 && world.quality.scaleCorrectnessPercent == 100,
                      "Generated V4 world maps should use complete production art with no wrong-scale semantic details");
        bool hasOcean = false;
        for (const MapCellModel &cell : world.map.cells)
        {
            hasOcean = hasOcean || (cell.kind == MapTileKind::Water && cell.wayKind != MapWayKind::River);
            ok &= require(cell.kind != MapTileKind::Building && cell.kind != MapTileKind::Cover && cell.kind != MapTileKind::Rubble &&
                              cell.kind != MapTileKind::Hazard && cell.kind != MapTileKind::Pit,
                          "World maps must not contain local building or tactical roles");
        }
        ok &= require(hasOcean, "Every V4 world-scale map should establish a non-river ocean or coastline water body");
        for (const MapGeneratedFeature &feature : world.map.generation.graph.features)
        {
            if (feature.kind != MapGeneratedFeatureKind::River || feature.points.empty())
                continue;
            const MapFeaturePoint &outlet = feature.points.back();
            const int outletX = std::clamp(static_cast<int>(std::floor(outlet.x)), 0, world.map.width - 1);
            const int outletY = std::clamp(static_cast<int>(std::floor(outlet.y)), 0, world.map.height - 1);
            const bool validEdge = outlet.x <= 1.5f || outlet.y <= 1.5f || outlet.x >= world.map.width - 1.5f || outlet.y >= world.map.height - 1.5f;
            bool reachesWaterBody = false;
            for (int offsetY = -1; offsetY <= 1 && !reachesWaterBody; ++offsetY)
            {
                for (int offsetX = -1; offsetX <= 1; ++offsetX)
                {
                    if (offsetX == 0 && offsetY == 0)
                        continue;
                    const MapCellModel *neighbor = mapCellAt(world.map, outletX + offsetX, outletY + offsetY);
                    reachesWaterBody = reachesWaterBody ||
                                       (neighbor && neighbor->kind == MapTileKind::Water && neighbor->wayKind != MapWayKind::River);
                }
            }
            ok &= require(validEdge || reachesWaterBody, "Every V4 river should terminate at a map edge or adjacent non-river coastline water");
            foundAlternativeRiverOutlet = foundAlternativeRiverOutlet || reachesWaterBody || outlet.y < world.map.height - 1.5f;
        }

        MapGenerationOptions cityOptions;
        cityOptions.generatorVersion = 4;
        cityOptions.scope = MapScope::Local;
        cityOptions.scale = MapLevel::Site;
        cityOptions.purpose = MapGenerationPurpose::SettlementEnvirons;
        cityOptions.width = 36;
        cityOptions.height = 26;
        // Settlement families advance every 17 generator seeds. Stride by 17
        // so this regression corpus deliberately exercises every family.
        cityOptions.seed = seed * 17u;
        cityOptions.qualityAttempts = 1;
        const MapGenerationResult city = generateSemanticMap(cityOptions);
        settlementLayouts.insert(city.map.generation.recipe.recipeId);
        std::vector<MapCellCoordinate> buildingCells;
        for (int y = 0; y < city.map.height; ++y)
            for (int x = 0; x < city.map.width; ++x)
                if (const MapCellModel *cell = mapCellAt(city.map, x, y); cell && cell->kind == MapTileKind::Building)
                    buildingCells.push_back({x, y});
        ok &= require(buildingCells.size() <= 30,
                      "V4 settlements should cap individual building stamps to preserve visual hierarchy");
        for (std::size_t left = 0; left < buildingCells.size(); ++left)
            for (std::size_t right = left + 1; right < buildingCells.size(); ++right)
                ok &= require(std::abs(buildingCells[left].x - buildingCells[right].x) > 1 ||
                                  std::abs(buildingCells[left].y - buildingCells[right].y) > 1,
                              "V4 settlement buildings should not overlap adjacent stamp footprints");
        for (const MapGeneratedFeature &feature : city.map.generation.graph.features)
            if (feature.kind == MapGeneratedFeatureKind::District)
                ok &= require(feature.points.size() == 8,
                              "V4 settlement districts should use softened eight-point footprints instead of preview-like rectangles");
        if (city.map.generation.recipe.recipeId == "settlement_river")
            ok &= require(std::any_of(city.map.generation.graph.features.begin(), city.map.generation.graph.features.end(),
                                      [](const MapGeneratedFeature &feature) {
                                          return feature.kind == MapGeneratedFeatureKind::Bridge && feature.points.size() >= 2;
                                      }),
                          "V4 river settlements should publish bridge geometry wherever a street crosses the river");
    }
    ok &= require(foundAlternativeRiverOutlet, "V4 rivers should reach coastlines or multiple outlet edges instead of always flowing south");
    ok &= require(settlementLayouts.size() == 6, "The six V4 settlement layout families should all be reachable and distinct");
    return ok;
}

bool testV4ExcludedElementsAreAppliedBeforeQualitySelection()
{
    MapGenerationOptions options;
    options.generatorVersion = 4;
    options.scope = MapScope::Local;
    options.scale = MapLevel::Region;
    options.purpose = MapGenerationPurpose::TravelArea;
    options.landscape = MapLandscapeProfile::RiverValley;
    options.width = 38;
    options.height = 28;
    options.seed = 884211u;
    options.waterLevel = 62;
    options.requestedLandmarks = 5;
    options.qualityAttempts = 2;
    options.excludedElements = {"river", "landmark"};
    const MapGenerationResult result = generateSemanticMap(options);
    bool ok = true;
    ok &= require(result.map.generation.recipe.excludedElements == options.excludedElements,
                  "V4 generation should persist every explicit must-not element");
    ok &= require(std::none_of(result.map.generation.graph.features.begin(), result.map.generation.graph.features.end(),
                               [](const MapGeneratedFeature &feature) {
                                   return feature.kind == MapGeneratedFeatureKind::River ||
                                          feature.kind == MapGeneratedFeatureKind::Landmark;
                               }),
                  "Excluded river and landmark features must be removed before candidate scoring");
    ok &= require(std::none_of(result.map.cells.begin(), result.map.cells.end(), [](const MapCellModel &cell) {
                      return cell.wayKind == MapWayKind::River || cell.kind == MapTileKind::Landmark;
                  }),
                  "Excluded semantic cells and ways must not survive in the returned raster projection");
    return ok;
}

bool testV4RiverElevationsRemainPersistableAndDownhillAfterCrossings()
{
    bool ok = true;
    // These seeds cover the former release-QA failures: high terrain samples
    // above the persisted editor range and roads turning river cells into
    // bridges after hydrology had assigned their elevations.
    const std::uint32_t regressionSeeds[] = {
        707919u, 715838u, 723757u, 1700003u, 1715841u, 1723760u};
    for (std::uint32_t seed : regressionSeeds)
    {
        MapGenerationOptions options;
        options.generatorVersion = 4;
        options.scope = seed < 1000000u ? MapScope::World : MapScope::Local;
        options.scale = seed < 1000000u ? MapLevel::World : MapLevel::Region;
        options.purpose = seed < 1000000u ? MapGenerationPurpose::Region : MapGenerationPurpose::TravelArea;
        options.width = 40 + static_cast<int>(seed % 41u);
        options.height = 30 + static_cast<int>((seed / 7u) % 31u);
        options.seed = seed;
        options.waterLevel = 12 + static_cast<int>((seed / 11u) % 58u);
        options.mountainDensity = 8 + static_cast<int>((seed / 13u) % 58u);
        options.forestDensity = 10 + static_cast<int>((seed / 17u) % 60u);
        options.routeDensity = 18 + static_cast<int>((seed / 19u) % 58u);
        options.requestedSettlements = 2 + static_cast<int>(seed % 4u);
        options.requestedLandmarks = 2 + static_cast<int>((seed / 5u) % 5u);
        options.edgeConnectionCount = 2 + static_cast<int>(seed % 3u);
        options.roadNetwork = MapRoadNetworkMode::Connected;
        options.qualityAttempts = 1;

        const MapGenerationResult generated = generateSemanticMap(options);
        for (const MapCellModel &cell : generated.map.cells)
            ok &= require(cell.elevation >= -20 && cell.elevation <= 20,
                          "V4 generated elevations must remain inside the persisted editor range");

        for (const MapGeneratedFeature &feature : generated.map.generation.graph.features)
        {
            if (feature.kind != MapGeneratedFeatureKind::River)
                continue;
            int previousElevation = 20;
            MapTileKind previousKind = MapTileKind::Empty;
            MapWayKind previousWay = MapWayKind::None;
            int previousX = -1;
            int previousY = -1;
            for (const MapFeaturePoint &point : feature.points)
            {
                const int pointX = std::clamp(static_cast<int>(std::floor(point.x)), 0, generated.map.width - 1);
                const int pointY = std::clamp(static_cast<int>(std::floor(point.y)), 0, generated.map.height - 1);
                const MapCellModel *cell = mapCellAt(generated.map, pointX, pointY);
                if (!cell)
                    continue;
                ok &= require(cell->elevation <= previousElevation,
                              "V4 river elevations must remain downhill after roads create bridge crossings: seed=" +
                                  std::to_string(seed) + " feature=" + feature.id + " cell=" +
                                  std::to_string(pointX) + "," + std::to_string(pointY) + " " +
                                  std::to_string(previousElevation) + "->" + std::to_string(cell->elevation) +
                                  " previous=" + std::to_string(previousX) + "," + std::to_string(previousY) +
                                  " kind=" + std::to_string(static_cast<int>(previousKind)) +
                                  " way=" + std::to_string(static_cast<int>(previousWay)));
                previousElevation = cell->elevation;
                previousKind = cell->kind;
                previousWay = cell->wayKind;
                previousX = pointX;
                previousY = pointY;
            }
        }
    }

    // Release case world_8071 placed a locked landmark directly over a river
    // sample after hydrology had been evaluated. Required facts must move to
    // the nearest safe cell instead of changing the persisted river height.
    {
        constexpr int localIndex = 8071;
        constexpr std::uint32_t seed = 64614249u;
        MapGenerationOptions options;
        options.generatorVersion = 4;
        options.styleId = "fantasy_neutral";
        options.seed = seed;
        options.landscape = static_cast<MapLandscapeProfile>(localIndex % 5);
        options.quality = MapGenerationQualityLevel::Polished;
        options.qualityAttempts = 8;
        options.width = 40 + static_cast<int>(seed % 41u);
        options.height = 30 + static_cast<int>((seed / 7u) % 31u);
        options.waterLevel = 12 + static_cast<int>((seed / 11u) % 58u);
        options.mountainDensity = 8 + static_cast<int>((seed / 13u) % 58u);
        options.forestDensity = 10 + static_cast<int>((seed / 17u) % 60u);
        options.routeDensity = 18 + static_cast<int>((seed / 19u) % 58u);
        options.tacticalDensity = 12 + static_cast<int>((seed / 23u) % 50u);
        options.requestedSettlements = 2 + static_cast<int>(seed % 4u);
        options.requestedLandmarks = 2 + static_cast<int>((seed / 5u) % 5u);
        options.edgeConnectionCount = 2 + static_cast<int>(seed % 3u);
        options.roadNetwork = MapRoadNetworkMode::Connected;
        options.scope = MapScope::World;
        options.scale = MapLevel::World;
        options.purpose = MapGenerationPurpose::Region;
        options.nationNames = {"Aster", "Bram", "Cyr"};
        MapGenerationConstraint constraint;
        constraint.id = "qa_required_anchor_8071";
        constraint.label = "QA Required Landmark";
        constraint.featureKind = MapGeneratedFeatureKind::Landmark;
        constraint.x = static_cast<float>(options.width) * 0.42f;
        constraint.y = static_cast<float>(options.height) * 0.57f;
        constraint.required = true;
        constraint.locked = true;
        options.constraints.push_back(constraint);
        options.requiredElements.push_back(constraint.label);
        options.excludedElements.push_back("modern highway");
        options.lockedFeatureIds.push_back(constraint.id);

        const MapGenerationResult generated = generateSemanticMap(options);
        ok &= require(std::any_of(generated.map.generation.graph.features.begin(),
                                  generated.map.generation.graph.features.end(),
                                  [&](const MapGeneratedFeature &feature) {
                                      return feature.id == constraint.id && feature.name == constraint.label;
                                  }),
                      "A constrained V4 world landmark must retain its stable ID and label after safe relocation");
        for (const MapGeneratedFeature &feature : generated.map.generation.graph.features)
        {
            if (feature.kind != MapGeneratedFeatureKind::River)
                continue;
            int previousElevation = std::numeric_limits<int>::max();
            int previousX = -1;
            int previousY = -1;
            MapTileKind previousKind = MapTileKind::Empty;
            MapWayKind previousWay = MapWayKind::None;
            for (const MapFeaturePoint &point : feature.points)
            {
                const int pointX = std::clamp(static_cast<int>(std::floor(point.x)), 0, generated.map.width - 1);
                const int pointY = std::clamp(static_cast<int>(std::floor(point.y)), 0, generated.map.height - 1);
                const MapCellModel *cell = mapCellAt(generated.map, pointX, pointY);
                if (!cell)
                    continue;
                ok &= require(cell->elevation <= previousElevation,
                              "Locked world facts must not make a V4 river climb after hydrology: feature=" +
                                  feature.id + " cell=" + std::to_string(pointX) + "," + std::to_string(pointY) +
                                  " " + std::to_string(previousElevation) + "->" +
                                  std::to_string(cell->elevation) + " previous=" +
                                  std::to_string(previousX) + "," + std::to_string(previousY) +
                                  " previous-kind=" + std::to_string(static_cast<int>(previousKind)) +
                                  " previous-way=" + std::to_string(static_cast<int>(previousWay)) +
                                  " kind=" + std::to_string(static_cast<int>(cell->kind)) +
                                  " way=" + std::to_string(static_cast<int>(cell->wayKind)));
                previousElevation = cell->elevation;
                previousX = pointX;
                previousY = pointY;
                previousKind = cell->kind;
                previousWay = cell->wayKind;
            }
        }
    }

    // Release case world_8163 left no traversable, unused land cell for its
    // required landmark. Extreme terrain may relocate the fact onto steep
    // land, but must never silently omit it.
    {
        constexpr int localIndex = 8163;
        constexpr std::uint32_t seed = 65613702u;
        MapGenerationOptions options;
        options.generatorVersion = 4;
        options.styleId = "fantasy_neutral";
        options.seed = seed;
        options.landscape = static_cast<MapLandscapeProfile>(localIndex % 5);
        options.quality = MapGenerationQualityLevel::Polished;
        options.qualityAttempts = 8;
        options.width = 40 + static_cast<int>(seed % 41u);
        options.height = 30 + static_cast<int>((seed / 7u) % 31u);
        options.waterLevel = 12 + static_cast<int>((seed / 11u) % 58u);
        options.mountainDensity = 8 + static_cast<int>((seed / 13u) % 58u);
        options.forestDensity = 10 + static_cast<int>((seed / 17u) % 60u);
        options.routeDensity = 18 + static_cast<int>((seed / 19u) % 58u);
        options.tacticalDensity = 12 + static_cast<int>((seed / 23u) % 50u);
        options.requestedSettlements = 2 + static_cast<int>(seed % 4u);
        options.requestedLandmarks = 2 + static_cast<int>((seed / 5u) % 5u);
        options.edgeConnectionCount = 2 + static_cast<int>(seed % 3u);
        options.roadNetwork = MapRoadNetworkMode::Connected;
        options.scope = MapScope::World;
        options.scale = MapLevel::World;
        options.purpose = MapGenerationPurpose::Region;
        options.nationNames = {"Aster", "Bram", "Cyr"};
        MapGenerationConstraint constraint;
        constraint.id = "qa_required_anchor_8163";
        constraint.label = "QA Required Landmark";
        constraint.featureKind = MapGeneratedFeatureKind::Landmark;
        constraint.x = static_cast<float>(options.width) * 0.42f;
        constraint.y = static_cast<float>(options.height) * 0.57f;
        constraint.required = true;
        constraint.locked = true;
        options.constraints.push_back(constraint);
        options.requiredElements.push_back(constraint.label);
        options.excludedElements.push_back("modern highway");
        options.lockedFeatureIds.push_back(constraint.id);

        const MapGenerationResult generated = generateSemanticMap(options);
        ok &= require(std::any_of(generated.map.generation.graph.features.begin(),
                                  generated.map.generation.graph.features.end(),
                                  [&](const MapGeneratedFeature &feature) {
                                      return feature.id == constraint.id && feature.name == constraint.label &&
                                             !feature.points.empty();
                                  }),
                      "Extreme V4 world terrain must retain a required locked landmark");
    }
    return ok;
}

bool testV10WorldTopologyAndRequestCoverage()
{
    bool ok=true;
    MapGenerationOptions options;
    options.generatorVersion=10;options.styleId="illustrated_atlas";options.scale=MapLevel::World;options.scope=MapScope::World;
    options.width=48;options.height=34;options.seed=516149336u;options.landscape=MapLandscapeProfile::LakeCountry;
    options.waterLevel=77;options.mountainDensity=28;options.forestDensity=39;options.requestedSettlements=7;
    options.requestedLandmarks=3;options.qualityAttempts=24;
    const auto lake=generateSemanticMap(options);
    const int towns=static_cast<int>(std::count_if(lake.map.generation.graph.features.begin(),lake.map.generation.graph.features.end(),
        [](const auto &f){return f.kind==MapGeneratedFeatureKind::Settlement;}));
    ok &= require(towns>=7 && lake.quality.criticalIssueCount==0,
        "V10 wet-world regression must retain the requested usable settlements");
    ok &= require(lake.map.generation.recipe.styleVersion==5 && lake.map.generation.recipe.styleSnapshotId=="illustrated_atlas_v5",
        "New V10 atlas maps must explicitly pin illustrated v5");
    auto missing=lake.map;
    missing.generation.graph.features.erase(std::remove_if(missing.generation.graph.features.begin(),missing.generation.graph.features.end(),
        [](const auto &f){return f.kind==MapGeneratedFeatureKind::Settlement;}),missing.generation.graph.features.end());
    const auto missingReport=analyzeMapGenerationQuality(missing);
    ok &= require(std::any_of(missingReport.issues.begin(),missingReport.issues.end(),[](const auto &issue){
        return issue.code=="settlement.requested_count_missing" && issue.severity==MapGenerationIssueSeverity::Critical;}),
        "A lost requested settlement count must never be reported eligible");
    options.seed=373657762u;options.landscape=MapLandscapeProfile::RiverValley;options.waterLevel=70;options.requestedSettlements=6;
    const auto maritime=generateSemanticMap(options);
    int land=0;for(int y=0;y<maritime.map.height;++y)for(int x=0;x<maritime.map.width;++x)
        land+=!mapGenV5WorldOpenWaterAt(maritime.map,x,y);
    const int largest=mapGenLargestComponentSize(maritime.map,[](const MapCellModel &cell){
        return cell.kind!=MapTileKind::Water || cell.wayKind==MapWayKind::River || cell.wayKind==MapWayKind::Canal;},false);
    ok &= require(land-largest>=12,"V10 high-water world must retain actual offshore land beyond its main continent");
    for(const auto &feature:maritime.map.generation.graph.features)
        if(feature.kind==MapGeneratedFeatureKind::Landmark && !feature.points.empty())
        {
            const int x=static_cast<int>(feature.points.front().x),y=static_cast<int>(feature.points.front().y);
            ok &= require(!mapGenNearKind(maritime.map,x,y,MapTileKind::Water,1),"V10 automatic landmarks require a dry coastal buffer");
        }
    auto frontierOptions=options;frontierOptions.seed=1683047740u;frontierOptions.landscape=MapLandscapeProfile::ForestFrontier;
    frontierOptions.waterLevel=38;frontierOptions.mountainDensity=29;frontierOptions.forestDensity=78;
    frontierOptions.requestedSettlements=7;
    const auto frontier=generateSemanticMap(frontierOptions);
    const int forestCells=static_cast<int>(std::count_if(frontier.map.cells.begin(),frontier.map.cells.end(),
        [](const auto &cell){return cell.kind==MapTileKind::Forest;}));
    ok &= require(forestCells>=30,"A high-forest frontier must place its primary woodland on usable land");
    MapModel roadMap;roadMap.width=44;roadMap.height=24;roadMap.atlasLevel=MapLevel::Region;
    roadMap.generation.recipe.generatorVersion=10;roadMap.generation.recipe.seed=741902u;resizeMapCells(roadMap);
    for(auto &cell:roadMap.cells)cell.kind=MapTileKind::Dirt;
    MapGeneratedFeature road;road.kind=MapGeneratedFeatureKind::Road;road.id="v10-road";road.points={{3.5f,12.5f},{39.5f,12.5f}};
    auto repeat=road;mapGenV7FitOverviewRoadPathFeature(roadMap,road);mapGenV7FitOverviewRoadPathFeature(roadMap,repeat);
    ok &= require(road.points.size()==repeat.points.size() && std::equal(road.points.begin(),road.points.end(),repeat.points.begin(),
        [](auto a,auto b){return a.x==b.x && a.y==b.y;}),"V10 curved road geometry must be deterministic");
    ok &= require(road.points.front().x==3.5f && road.points.back().x==39.5f,"V10 alignment must preserve destination anchors");
    for(std::size_t i=1;i<road.points.size();++i)ok &= require(mapGenV7RoadChordIsSafe(roadMap,road.points[i-1],road.points[i]),
        "V10 road relaxation must preserve the complete dry shoulder");
    repeat.locked=true;const auto locked=repeat.points;mapGenV7FitOverviewRoadPathFeature(roadMap,repeat);
    ok &= require(repeat.points.size()==locked.size() && std::equal(locked.begin(),locked.end(),repeat.points.begin(),
        [](auto a,auto b){return a.x==b.x && a.y==b.y;}),"V10 must not realign locked roads");
    std::vector<float> flatElevation(roadMap.cells.size(),.4f);
    const MapCellCoordinate spring{3,12},mouth{39,12};
    const auto drainage=mapGenV4FindDrainagePath(roadMap,flatElevation,spring,mouth,37191u,true);
    const auto drainageReplay=mapGenV4FindDrainagePath(roadMap,flatElevation,spring,mouth,37191u,true);
    ok &= require(!drainage.empty() && drainage.front()==spring && drainage.back()==mouth,
        "Composed drainage must preserve source and receiving endpoint");
    ok &= require(drainage==drainageReplay,"Composed drainage must remain deterministic");
    ok &= require(std::any_of(drainage.begin(),drainage.end(),[&](auto p){return std::abs(p.y-spring.y)>=2;}),
        "Long flat-basin rivers should follow a valley instead of one ruler row");
    return ok;
}

bool testV9NaturalRoadAlignment()
{
    MapModel map;
    map.width=44;map.height=24;map.atlasLevel=MapLevel::World;
    map.generation.recipe.generatorVersion=9;map.generation.recipe.seed=741902;
    resizeMapCells(map);
    for (auto &cell:map.cells) cell.kind=MapTileKind::Dirt;
    MapGeneratedFeature road;
    road.kind=MapGeneratedFeatureKind::Road;road.id="natural-road";
    for (int x=3;x<=39;++x) road.points.push_back({x+.5f,12.5f});
    const auto original=road.points;
    mapGenV7FitOverviewRoadPathFeature(map,road);
    bool ok=true;float maximumOffset=0.f,length=0.f;
    for (std::size_t i=1;i<road.points.size();++i)
    {
        const auto a=road.points[i-1],b=road.points[i];
        maximumOffset=std::max(maximumOffset,std::abs(b.y-12.5f));
        length+=std::hypot(b.x-a.x,b.y-a.y);
        ok &= require(mapGenV7RoadChordIsSafe(map,a,b),"V9 natural alignment must preserve the full dry road shoulder");
    }
    ok &= require(maximumOffset>.25f && length<36.f*1.10f && road.points.front().x==3.5f && road.points.back().x==39.5f,
                  "V9 roads should have restrained natural alignment without detours or moved destinations");
    road.points=original;road.locked=true;
    mapGenV7FitOverviewRoadPathFeature(map,road);
    ok &= require(road.points.size()==original.size() && std::equal(road.points.begin(),road.points.end(),original.begin(),
        [](auto a,auto b){return a.x==b.x && a.y==b.y;}),"V9 alignment must not change locked roads");
    road.locked=false;road.points=original;map.generation.recipe.generatorVersion=8;
    mapGenV7FitOverviewRoadPathFeature(map,road);
    // The historical quadratic rounding/resampling produces a few float ULPs
    // of error even along a mathematically horizontal line.
    float legacyOffset=0.f;
    for (const auto p:road.points) legacyOffset=std::max(legacyOffset,std::abs(p.y-12.5f));
    std::cout << "V8 horizontal road maximum numeric offset=" << legacyOffset << '\n';
    ok &= require(legacyOffset<0.00001f,
                  "Saved V8 paths retain their historical straight alignment");
    return ok;
}

bool testV8AtlasBasinsAndRegionalRelief()
{
    bool ok = true;
    for (const auto seed : {2801112u,2905841u,3010570u})
    {
        MapGenerationOptions options;
        options.generatorVersion=8; options.styleId="illustrated_atlas";
        options.scale=MapLevel::World; options.scope=MapScope::World;
        options.width=64; options.height=44; options.seed=seed;
        options.landscape=MapLandscapeProfile::LakeCountry; options.waterLevel=60;
        const auto generated=generateSemanticMap(options);
        const auto &map=generated.map;
        ok &= require(map.generation.recipe.styleVersion==3 && map.generation.recipe.styleSnapshotId=="illustrated_atlas_v3",
                      "V8 drafts must explicitly pin illustrated atlas V3");
        int lakes=0;
        for (const auto &feature:map.generation.graph.features)
        {
            if (feature.materialId!="fantasy_neutral.water.lake") continue;
            ++lakes;
            float cx=0.f,cy=0.f;
            for (auto p:feature.points) {cx+=p.x;cy+=p.y;}
            cx/=feature.points.size();cy/=feature.points.size();
            const int x=static_cast<int>(cx),y=static_cast<int>(cy);
            const auto *centre=mapCellAt(map,x,y);
            ok &= require(centre && centre->kind==MapTileKind::Water,
                          "A named lake must have a real wet basin, not a lake outline over land");
            // A broad-water component must be enclosed. River-tagged narrow
            // outlets are excluded: connected drainage is legitimate.
            std::queue<MapCellCoordinate> queue;
            std::vector<bool> visited(map.cells.size(),false);
            queue.push({x,y}); bool touchesEdge=false;
            while (!queue.empty())
            {
                const auto p=queue.front();queue.pop();
                if (!isMapCellInside(map,p.x,p.y)) continue;
                const auto index=p.y*map.width+p.x;
                const auto &cell=map.cells[index];
                if (visited[index] || cell.kind!=MapTileKind::Water || cell.wayKind==MapWayKind::River) continue;
                visited[index]=true;
                touchesEdge |= p.x==0 || p.y==0 || p.x==map.width-1 || p.y==map.height-1;
                for (const auto d : std::array<MapCellCoordinate,4>{{{1,0},{-1,0},{0,1},{0,-1}}})
                    queue.push({p.x+d.x,p.y+d.y});
            }
            ok &= require(!touchesEdge,"V8 inland lakes must not be painted into the ocean");
        }
        ok &= require(lakes==2,"The lake-country regression seeds must retain two inland basins");
    }
    MapGenerationOptions pass;
    pass.generatorVersion=8;pass.styleId="illustrated_atlas";
    pass.scale=MapLevel::Region;pass.scope=MapScope::Local;
    pass.width=48;pass.height=36;pass.seed=932571;
    pass.landscape=MapLandscapeProfile::MountainPass;pass.mountainDensity=65;pass.waterLevel=18;pass.forestDensity=15;
    const auto generated=generateSemanticMap(pass);
    ok &= require(std::any_of(generated.map.generation.graph.features.begin(),generated.map.generation.graph.features.end(),
        [](const auto &feature){return feature.kind==MapGeneratedFeatureKind::MountainRange && feature.points.size()>1;}),
        "Regional mountain terrain must have visible range geometry");
    return ok;
}

bool testV7TerrainFollowingRoadsAndVersionPins()
{
    bool ok = true;
    MapFeatureGraph junctions;
    MapGeneratedFeature trunk;
    trunk.id = "locked-main"; trunk.kind = MapGeneratedFeatureKind::Road; trunk.locked = true;
    trunk.points = {{1.5f, 8.5f}, {20.5f, 8.5f}};
    junctions.features.push_back(trunk);
    MapGeneratedFeature distantTrunk = trunk;
    distantTrunk.id = "far-main"; distantTrunk.points = {{1.5f, 12.5f}, {20.5f, 12.5f}};
    junctions.features.push_back(distantTrunk);
    MapGeneratedFeature branch;
    branch.id = "new-branch"; branch.kind = MapGeneratedFeatureKind::Road;
    branch.points = {{9.5f, 2.5f}, {9.5f, 16.5f}};
    mapGenV7JoinFirstExistingRoad(branch, junctions);
    ok &= require(branch.points.size() == 2 && branch.points.back().x == 9.5f && branch.points.back().y == 8.5f &&
                      junctions.features.front().points.size() == 2 && junctions.features.front().locked,
                  "A new road branch must stop at its first exact network junction without extending into redundant crossing triangles or changing locked roads");
    branch.points = {{0.5f, 8.5f}, {25.5f, 8.5f}};
    mapGenV7JoinFirstExistingRoad(branch, junctions);
    ok &= require(branch.points.back().x == 1.5f && branch.points.back().y == 8.5f,
                  "A new road that joins along an existing alignment must stop at first overlap instead of overpainting the trunk");
    branch.points = {{9.5f, 2.5f}, {9.5f, 16.5f}};
    const std::vector<std::string> connectedIds{"far-main"};
    mapGenV7JoinFirstExistingRoad(branch, junctions, &connectedIds);
    ok &= require(branch.points.back().y == 12.5f,
                  "An unrelated inherited road must not silently terminate a branch before it reaches the connected network");
    MapModel projection;
    projection.width = 22; projection.height = 20;
    resizeMapCells(projection);
    for (auto &cell : projection.cells) cell.kind = MapTileKind::Dirt;
    for (int x = 0; x < projection.width; ++x)
    {
        mapCellAt(projection, x, 8)->wayKind = MapWayKind::DirtRoad;
        mapCellAt(projection, x, 12)->kind = MapTileKind::Water;
        mapCellAt(projection, x, 12)->wayKind = MapWayKind::River;
    }
    mapCellAt(projection, 9, 5)->kind = MapTileKind::Mountain;
    const auto terrainBeforeBranch = projection.cells;
    std::vector<MapCellCoordinate> proposedPath;
    for (int y = 2; y <= 16; ++y) proposedPath.push_back({9, y});
    mapGenV2RasterizeRoad(projection, proposedPath, MapWayKind::DirtRoad);
    ok &= require(mapCellAt(projection, 9, 12)->kind == MapTileKind::Bridge,
                  "The projection regression must first create a speculative bridge on the discarded suffix");
    branch.points = {{9.5f, 2.5f}, {9.5f, 8.5f}};
    mapGenV7CommitFittedRoadProjection(projection, branch, terrainBeforeBranch, MapWayKind::DirtRoad);
    ok &= require(mapCellAt(projection, 9, 12)->kind == MapTileKind::Water &&
                      mapCellAt(projection, 9, 12)->wayKind == MapWayKind::River &&
                      mapCellAt(projection, 9, 14)->wayKind == MapWayKind::None &&
                      mapCellAt(projection, 9, 5)->kind == MapTileKind::Stone &&
                      mapCellAt(projection, 9, 5)->wayKind == MapWayKind::DirtRoad &&
                      mapCellAt(projection, 4, 8)->wayKind == MapWayKind::DirtRoad,
                  "Committing a trimmed branch must remove its speculative suffix/bridge while retaining the real pass and existing trunk");
    for (const auto level : {MapLevel::World, MapLevel::Region})
    {
        MapModel map;
        map.width = 33; map.height = 25; map.atlasLevel = level;
        map.generation.recipe.generatorVersion = 7;
        resizeMapCells(map);
        for (auto &cell : map.cells) { cell.kind = MapTileKind::Dirt; cell.elevation = 0; }
        auto path = mapGenV2FindRoadPath(map, {2, 3}, {29, 17});
        MapGeneratedFeature road;
        road.kind = MapGeneratedFeatureKind::Road;
        for (const auto point : path) road.points.push_back({point.x + 0.5f, point.y + 0.5f});
        mapGenV5SmoothWorldRoadPathFeature(map, road);
        float length = 0.0f;
        for (std::size_t index = 1; index < road.points.size(); ++index)
            length += std::hypot(road.points[index].x - road.points[index - 1].x, road.points[index].y - road.points[index - 1].y);
        ok &= require(!path.empty() && !road.points.empty() && length <= std::hypot(27.0f, 14.0f) * 1.02f,
                      "V7 roads across uniform ground must stay direct instead of adding decorative wiggles");
        for (int y = 6; y <= 18; ++y)
            for (int x = 12; x <= 19; ++x)
                mapCellAt(map, x, y)->elevation = 10;
        const auto valley = mapGenV2FindRoadPath(map, {2, 12}, {30, 12});
        bool staysLow = !valley.empty();
        for (const auto point : valley) staysLow &= mapCellAt(map, point.x, point.y)->elevation == 0;
        ok &= require(staysLow, "V7 roads must take the gentle valley instead of crossing two steep embankments");
        map.generation.recipe.generatorVersion = 6;
        const auto legacy = mapGenV2FindRoadPath(map, {2, 12}, {30, 12});
        MapModel flatLegacy = map;
        for (auto &cell : flatLegacy.cells) cell.elevation = 0;
        const auto legacyFlat = mapGenV2FindRoadPath(flatLegacy, {2, 12}, {30, 12});
        ok &= require(!legacy.empty() && legacy == legacyFlat &&
                          (level != MapLevel::Region || (legacy.size() == 29 &&
                           std::all_of(legacy.begin(), legacy.end(), [](auto point) { return point.y == 12; }))),
                      "V6's legacy level-blind route must remain pinned when V7 adds grade costs");
        map.generation.recipe.generatorVersion = 7;
        for (auto &cell : map.cells) cell.elevation = 0;
        for (int y = 8; y < 23; ++y)
            for (int x = 7; x < 24; ++x)
                mapCellAt(map, x, y)->kind = MapTileKind::Wall;
        road.points = {{2.5f, 18.5f}, {2.5f, 5.5f}, {27.5f, 5.5f}, {27.5f, 18.5f}};
        mapGenV5SmoothWorldRoadPathFeature(map, road);
        bool intermediateBend = false;
        bool safe = true;
        float worstTurn = 1.0f;
        for (std::size_t index = 1; index < road.points.size(); ++index)
        {
            const auto a = road.points[index - 1], b = road.points[index];
            safe &= mapGenV7RoadChordIsSafe(map, a, b);
            intermediateBend |= std::abs(a.x - b.x) > 0.01f && std::abs(a.y - b.y) > 0.01f;
            if (index + 1 < road.points.size())
            {
                const auto c = road.points[index + 1];
                const float divisor = std::hypot(b.x - a.x, b.y - a.y) * std::hypot(c.x - b.x, c.y - b.y);
                if (divisor > 0.0001f)
                    worstTurn = std::min(worstTurn, ((b.x - a.x) * (c.x - b.x) + (b.y - a.y) * (c.y - b.y)) / divisor);
            }
        }
        ok &= require(safe && intermediateBend && worstTurn > 0.85f,
                      "V7 road bends must round a real obstacle smoothly while keeping their full shoulder on safe ground");
        auto locked = road; locked.locked = true;
        const auto lockedPoints = locked.points;
        mapGenV5SmoothWorldRoadPathFeature(map, locked);
        const auto samePoint = [](auto a, auto b) { return a.x == b.x && a.y == b.y; };
        ok &= require(locked.points.size() == lockedPoints.size() &&
                          std::equal(locked.points.begin(), locked.points.end(), lockedPoints.begin(), samePoint),
                      "V7 fitting must not move locked road controls");

        for (auto &cell : map.cells) cell.kind = MapTileKind::Dirt;
        for (int y = 0; y < map.height; ++y)
        {
            auto *cell = mapCellAt(map, 16, y);
            cell->kind = MapTileKind::Water; cell->wayKind = MapWayKind::River;
        }
        mapCellAt(map, 16, 12)->kind = MapTileKind::Bridge;
        road.points = {{2.5f, 10.5f}, {14.5f, 12.5f}, {15.5f, 12.5f}, {16.5f, 12.5f}, {17.5f, 12.5f}, {18.5f, 12.5f}, {30.5f, 15.5f}};
        mapGenV5SmoothWorldRoadPathFeature(map, road);
        const std::vector<MapFeaturePoint> anchors{{15.5f, 12.5f}, {16.5f, 12.5f}, {17.5f, 12.5f}};
        ok &= require(std::search(road.points.begin(), road.points.end(), anchors.begin(), anchors.end(), samePoint) != road.points.end(),
                      "V7 fitting must preserve exact perpendicular bridge entry, crossing and exit controls");
    }
    for (const int version : {6, 7})
        for (const auto level : {MapLevel::World, MapLevel::Region})
        {
            MapGenerationOptions options;
            options.generatorVersion = version; options.styleId = "illustrated_atlas";
            options.styleVersion = version == 7 ? 1 : 2; // Deliberately mismatched incoming pin.
            options.styleSnapshotId = version == 7 ? "illustrated_atlas_v1" : "illustrated_atlas_v2";
            options.scale = level; options.scope = level == MapLevel::World ? MapScope::World : MapScope::Local;
            options.width = 40; options.height = 28; options.seed = 9127u; options.qualityAttempts = 1;
            const auto generated = generateSemanticMap(options);
            const auto &pin = generated.map.generation.recipe;
            ok &= require(pin.styleVersion == (version == 7 ? 2 : 1) &&
                              pin.styleSnapshotId == (version == 7 ? "illustrated_atlas_v2" : "illustrated_atlas_v1") &&
                              generated.quality.assetCoveragePercent == 100,
                          "Explicit generation must select the matching illustrated snapshot while legacy V6 retains v1");
        }
    return ok;
}

bool testV7CrossingApproachAndLocalExits()
{
    bool ok = true;
    MapModel anchorFixture;
    anchorFixture.width = 20; anchorFixture.height = 16;
    resizeMapCells(anchorFixture);
    for (auto &cell : anchorFixture.cells) cell.kind = MapTileKind::Dirt;
    MapGeneratedFeature angledTrunk;
    angledTrunk.id = "angled-main"; angledTrunk.kind = MapGeneratedFeatureKind::Road; angledTrunk.locked = true;
    angledTrunk.points = {{3.25f, 7.75f}, {16.25f, 9.75f}};
    anchorFixture.generation.graph.features.push_back(angledTrunk);
    MapFeaturePoint anchor;
    const float projectedAmount = ((9.5f - 3.25f) * 13.0f + (2.5f - 7.75f) * 2.0f) / (13.0f * 13.0f + 2.0f * 2.0f);
    ok &= require(mapGenV7NearestDryRoadAnchor(anchorFixture, {9.5f, 2.5f}, anchor) &&
                      gm::map_geometry::roadPointSegmentDistanceSquared(anchor, angledTrunk.points.front(), angledTrunk.points.back()) < 0.000001f &&
                      std::abs(anchor.x - (3.25f + 13.0f * projectedAmount)) < 0.00001f &&
                      std::abs(anchor.y - (7.75f + 2.0f * projectedAmount)) < 0.00001f &&
                      std::hypot(anchor.x - (std::floor(anchor.x) + 0.5f), anchor.y - (std::floor(anchor.y) + 0.5f)) > 0.001f,
                  "An automatic crossing approach must target the actual angled road, not merely the center of its occupied raster cell");
    MapModel banks;
    banks.width = 20; banks.height = 12; banks.atlasLevel = MapLevel::Region;
    banks.generation.recipe.generatorVersion = 7;
    resizeMapCells(banks);
    for (auto &cell : banks.cells) cell.kind = MapTileKind::Dirt;
    for (int x = 0; x < banks.width; ++x)
    {
        mapCellAt(banks, x, 5)->kind = MapTileKind::Water;
        mapCellAt(banks, x, 5)->wayKind = MapWayKind::River;
    }
    MapGeneratedFeature northRoad = angledTrunk;
    northRoad.id = "north-road"; northRoad.points = {{2.5f, 1.5f}, {17.5f, 1.5f}};
    MapGeneratedFeature southRoad = angledTrunk;
    southRoad.id = "south-road"; southRoad.points = {{2.5f, 8.5f}, {17.5f, 8.5f}};
    banks.generation.graph.features = {northRoad, southRoad};
    const std::vector<MapCellCoordinate> crossing{{10, 4}, {10, 5}, {10, 6}};
    const auto bothBanks = mapGenV7ChooseBridgeApproach(banks, crossing);
    ok &= require(!bothBanks.cells.empty() && bothBanks.cells.front() == crossing.back() &&
                      !mapGenV2FindRoadPath(banks, crossing.front(), {10, 1}, true).empty() &&
                      !mapGenV2FindRoadPath(banks, crossing.back(), {10, 8}, true).empty(),
                  "When both banks reach roads without new water crossings, choose the shorter approach instead of always the first bank");
    banks.generation.graph.features.pop_back();
    const auto oneBank = mapGenV7ChooseBridgeApproach(banks, crossing);
    ok &= require(!oneBank.cells.empty() && oneBank.cells.front() == crossing.front() &&
                      mapGenV2FindRoadPath(banks, crossing.back(), {10, 1}, true).empty(),
                  "A bank separated from the network by the river must not manufacture an adjacent second bridge");
    for (int x = 0; x < banks.width; ++x) mapCellAt(banks, x, 3)->kind = MapTileKind::Wall;
    ok &= require(mapGenV7ChooseBridgeApproach(banks, crossing).cells.empty(),
                  "An optional bridge with neither bank connected must report no approach rather than publish an isolated crossing");
    MapGenerationOptions options;
    options.generatorVersion = 7; options.styleId = "illustrated_atlas";
    options.scale = MapLevel::Region; options.scope = MapScope::Local;
    options.creationIntent = MapCreationIntent::RegionExploration;
    options.seed = 1859824975u; options.width = 56; options.height = 40;
    options.waterLevel = 24; options.mountainDensity = 38; options.forestDensity = 62; options.qualityAttempts = 1;
    const auto observedGap = generateSemanticMap(options);
    int approaches = 0;
    for (const auto &feature : observedGap.map.generation.graph.features)
    {
        if (feature.name == "Regional river crossing")
            ok &= require(feature.points.size() == 5 && feature.points.front().x == 49.5f &&
                              feature.points.back().x == 53.5f && std::all_of(feature.points.begin(), feature.points.end(), [](auto p) { return p.y == 3.5f; }),
                          "The observed Region crossing must retain its exact perpendicular dry-bank and bridge controls");
        if (feature.name != "Bridge approach" || feature.points.size() < 2) continue;
        ++approaches;
        float nearest = std::numeric_limits<float>::max();
        for (const auto &other : observedGap.map.generation.graph.features)
        {
            if (other.id == feature.id || other.name == "Regional river crossing" ||
                (other.kind != MapGeneratedFeatureKind::Road && other.kind != MapGeneratedFeatureKind::Trail)) continue;
            for (std::size_t index = 1; index < other.points.size(); ++index)
                nearest = std::min(nearest, gm::map_geometry::roadPointSegmentDistanceSquared(
                    feature.points.back(), other.points[index - 1], other.points[index]));
        }
        ok &= require(nearest < 0.000001f,
                      "The actual1859824975 Region bridge approach must end exactly on the visible road network, closing its sub-cell gap");
    }
    ok &= require(approaches > 0, "The Region gap regression must exercise an actual automatic bridge approach");

    options.seed = 3253455220u;
    const auto observedBankLoop = generateSemanticMap(options);
    ok &= require(mapCellAt(observedBankLoop.map, 14, 1)->kind == MapTileKind::Water &&
                      mapCellAt(observedBankLoop.map, 14, 1)->wayKind == MapWayKind::River &&
                      mapCellAt(observedBankLoop.map, 15, 1)->kind == MapTileKind::Bridge &&
                      mapCellAt(observedBankLoop.map, 15, 2)->kind == MapTileKind::Bridge,
                  "The actual3253455220 holdout must keep its intended bridge and remove the adjacent duplicate crossing that formed a U");
    bool foundBankApproach = false;
    for (const auto &feature : observedBankLoop.map.generation.graph.features)
    {
        if (feature.name == "Regional river crossing")
            ok &= require(feature.points.size() == 4 && feature.points.front().y == 0.5f &&
                              feature.points.back().y == 3.5f && std::all_of(feature.points.begin(), feature.points.end(), [](auto p) { return p.x == 15.5f; }),
                          "Choosing the correct approach bank must leave the intended bridge's exact original controls unchanged");
        if (feature.name == "Bridge approach" && !feature.points.empty())
        {
            foundBankApproach = true;
            ok &= require(feature.points.front().x == 15.5f && feature.points.front().y == 3.5f &&
                              std::all_of(feature.points.begin(), feature.points.end(), [](auto p) { return p.y >= 3.5f - 0.00001f; }),
                          "The holdout bridge approach must leave its southern bank toward the road, without wrapping around the river");
        }
    }
    ok &= require(foundBankApproach, "The wrong-bank regression must exercise the automatic approach rather than omit the crossing");

    options.seed = 4203552724u; options.width = 48; options.height = 32;
    options.waterLevel = 20; options.mountainDensity = 18; options.forestDensity = 25;
    const auto observedSpur = generateSemanticMap(options);
    const float maximumSpur = std::max(5.0f, std::min(options.width, options.height) * 0.45f) * 1.35f;
    for (const auto &feature : observedSpur.map.generation.graph.features)
    {
        if (feature.name != "Road connection" || feature.points.size() < 2) continue;
        const auto endpoint = feature.points.front(); // V7 branches begin at their new destination.
        if (endpoint.x > 1.0f && endpoint.y > 1.0f && endpoint.x < options.width - 1.0f && endpoint.y < options.height - 1.0f) continue;
        float length = 0.0f;
        for (std::size_t index = 1; index < feature.points.size(); ++index)
            length += std::hypot(feature.points[index].x - feature.points[index - 1].x, feature.points[index].y - feature.points[index - 1].y);
        ok &= require(length <= maximumSpur,
                      "Optional V7 border exits must connect locally rather than draw a dominant border-to-center traverse");
    }
    MapGeneratedFeature boundaryFact;
    boundaryFact.id = "fixed-border-road"; boundaryFact.kind = MapGeneratedFeatureKind::Road; boundaryFact.locked = true;
    boundaryFact.points = {{0.5f, 20.5f}, {35.5f, 20.5f}};
    options.preservedFeatures = {boundaryFact};
    const auto preserved = generateSemanticMap(options);
    const auto actual = std::find_if(preserved.map.generation.graph.features.begin(), preserved.map.generation.graph.features.end(),
                                     [&](const auto &feature) { return feature.id == boundaryFact.id; });
    ok &= require(actual != preserved.map.generation.graph.features.end() && actual->locked && actual->points.size() == 2 &&
                      actual->points.front().x == 0.5f && actual->points.back().x == 35.5f,
                  "The optional-spur bound must preserve explicit locked boundary-road facts even when they are long");
    return ok;
}

bool testV6BoundedMountainPassesAndRoadClearance()
{
    bool ok = true;
    for (const auto size : {MapCellCoordinate{7, 6}, MapCellCoordinate{1, 7}, MapCellCoordinate{7, 1}})
        for (const auto corner : {MapCellCoordinate{0, 0}, MapCellCoordinate{size.x - 1, 0},
                                  MapCellCoordinate{0, size.y - 1}, MapCellCoordinate{size.x - 1, size.y - 1}})
        {
            MapModel edge;
            edge.width = size.x; edge.height = size.y;
            resizeMapCells(edge);
            for (auto &cell : edge.cells) cell.kind = MapTileKind::Dirt;
            mapCellAt(edge, corner.x, corner.y)->kind = MapTileKind::Mountain;
            const auto field = gm::map_routing::buildMountainPassRoutingField(edge);
            for (int y = 0; y < edge.height; ++y)
                for (int x = 0; x < edge.width; ++x)
                    ok &= require(field.distance[mapCellIndex(edge, x, y)] ==
                                      std::min(5, std::max(std::abs(x - corner.x), std::abs(y - corner.y))),
                                  "Mountain clearance at every map edge must remain bounded and cannot wrap into another row");
        }
    for (const MapLevel level : {MapLevel::World, MapLevel::Region})
    {
        MapModel directional;
        directional.width = 11; directional.height = 11;
        directional.atlasLevel = level;
        directional.generation.recipe.generatorVersion = 6;
        resizeMapCells(directional);
        for (auto &cell : directional.cells) cell.kind = MapTileKind::Wall;
        for (const auto point : {MapCellCoordinate{3, 5}, {4, 5}, {6, 5}, {7, 5},
                                 {5, 3}, {5, 4}, {5, 6}, {5, 7}, {4, 7}, {3, 7}, {3, 6}})
            mapCellAt(directional, point.x, point.y)->kind = MapTileKind::Dirt;
        mapCellAt(directional, 5, 5)->kind = MapTileKind::Mountain;
        const auto alternateEntry = mapGenV2FindRoadPath(directional, {5, 7}, {7, 5});
        const std::vector<MapCellCoordinate> validCrossing{{4, 5}, {5, 5}, {6, 5}};
        ok &= require(!alternateEntry.empty() &&
                          std::search(alternateEntry.begin(), alternateEntry.end(), validCrossing.begin(), validCrossing.end()) != alternateEntry.end() &&
                          gm::map_routing::mountainPassPathIsBounded(directional, alternateEntry,
                              gm::map_routing::buildMountainPassRoutingField(directional)),
                      "A cheaper mountain arrival from the wrong direction must not discard the valid longer perpendicular crossing");
        ok &= require(mapGenV2FindRoadPath(directional, {-1, 1}, {7, 5}).empty() &&
                          mapGenV2FindRoadPath(directional, {5, 7}, {11, 1}).empty(),
                      "Road search must reject out-of-map endpoints rather than treating them as wrapped cell indices");
        MapModel map;
        map.width = 25;
        map.height = 21;
        map.atlasLevel = level;
        map.generation.recipe.generatorVersion = 6;
        map.generation.recipe.seed = 63091;
        resizeMapCells(map);
        for (auto &cell : map.cells) { cell.kind = MapTileKind::Dirt; cell.elevation = 0; }
        for (int x = 0; x < map.width; ++x)
        {
            auto *cell = mapCellAt(map, x, 10);
            cell->kind = MapTileKind::Mountain;
            cell->elevation = x == 17 ? 0 : 20;
        }
        const auto field = gm::map_routing::buildMountainPassRoutingField(map);
        const auto pass = mapGenV2FindRoadPath(map, {12, 3}, {12, 17});
        ok &= require(!pass.empty() && pass == mapGenV2FindRoadPath(map, {12, 3}, {12, 17}) &&
                          gm::map_routing::mountainPassPathIsBounded(map, pass, field),
                      "V6 World and Region must choose a deterministic, bounded perpendicular mountain pass");
        ok &= require(std::find(pass.begin(), pass.end(), MapCellCoordinate{17, 10}) != pass.end(),
                      "V6 must prefer the reachable low pass over a direct high-ridge crossing");
        const auto parallel = mapGenV2FindRoadPath(map, {3, 8}, {21, 8});
        ok &= require(!parallel.empty() && std::none_of(parallel.begin(), parallel.end(), [&](const auto &point) {
                          return point.x >= 8 && point.x <= 16 && field.distance[mapCellIndex(map, point.x, point.y)] < 3;
                      }),
                      "A road parallel to mountain mass must take the clear inland corridor instead of hugging the relief");
        MapModel thick = map;
        for (int y = 7; y <= 12; ++y)
            for (int x = 0; x < thick.width; ++x)
                mapCellAt(thick, x, y)->kind = MapTileKind::Mountain;
        ok &= require(mapGenV2FindRoadPath(thick, {12, 3}, {12, 17}).empty(),
                      "V6 must report an unmet road across an unbroken mountain wall wider than its bounded passes");
        thick.generation.recipe.generatorVersion = 5;
        ok &= require(!mapGenV2FindRoadPath(thick, {12, 3}, {12, 17}).empty(),
                      "Explicit V5 recipes must retain their historical mountain traversal behavior");
        thick.generation.recipe.generatorVersion = 6;
        for (int y = 7; y <= 12; ++y)
            mapCellAt(thick, 17, y)->kind = MapTileKind::Dirt;
        const auto gap = mapGenV2FindRoadPath(thick, {12, 3}, {12, 17});
        ok &= require(!gap.empty() && std::none_of(gap.begin(), gap.end(), [&](const auto &point) {
                          return mapCellAt(thick, point.x, point.y)->kind == MapTileKind::Mountain;
                      }),
                      "A bounded natural low gap must remain routable without carving a new mountain crossing");

        const int oldPeak = mapCellAt(map, 17, 10)->elevation;
        mapGenV2RasterizeRoad(map, pass, MapWayKind::DirtRoad);
        ok &= require(mapCellAt(map, 17, 10)->kind == MapTileKind::Stone &&
                          mapCellAt(map, 17, 10)->elevation == oldPeak,
                      "The chosen pass must become traversable Stone while preserving elevation");
        MapGeneratedFeature road;
        road.id = "bounded_pass_road";
        road.kind = MapGeneratedFeatureKind::Road;
        road.points = {{17.5f, 5.5f}, {17.5f, 7.5f}, {17.5f, 9.5f}, {17.5f, 10.5f}, {17.5f, 11.5f}, {17.5f, 15.5f}};
        MapGeneratedFeature bridge;
        bridge.id = "fixed_bridge";
        bridge.kind = MapGeneratedFeatureKind::Bridge;
        bridge.points = {{17.5f, 7.5f}, {17.5f, 8.5f}};
        map.generation.graph.features = {road, bridge};
        mapCellAt(map, 17, 7)->kind = MapTileKind::Bridge;
        mapCellAt(map, 17, 8)->kind = MapTileKind::Bridge;
        const auto rendered = gm::map_render::overviewFinalRoadCurvePoints(map, road);
        ok &= require(!rendered.empty() && rendered.front().x == road.points.front().x && rendered.front().y == road.points.front().y &&
                          rendered.back().x == road.points.back().x && rendered.back().y == road.points.back().y &&
                          std::any_of(rendered.begin(), rendered.end(), [&](const auto &point) { return point.x == 17.5f && point.y == 7.5f; }),
                      "Final road geometry must retain endpoints and the exact bridge approach anchor");
        for (std::size_t index = 1; index < rendered.size(); ++index)
            ok &= require(mapGenV5WorldRoadSegmentIsSafe(map, rendered[index - 1], rendered[index]),
                          "The complete rendered road must not smooth back into uncarved mountain cells");
        const auto samePoints = [](const auto &a, const auto &b) {
            return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](const auto &left, const auto &right) {
                return left.x == right.x && left.y == right.y;
            });
        };
        road.locked = true;
        ok &= require(samePoints(gm::map_render::overviewFinalRoadCurvePoints(map, road), road.points),
                      "Locked road controls must not be moved by the final cartographic curve");
        road.locked = false;
        road.id = "manual_way_17_10";
        ok &= require(samePoints(gm::map_render::overviewFinalRoadCurvePoints(map, road), road.points),
                      "Manual road controls must remain exact without requiring a separate lock");

        MapGenerationOptions options;
        options.generatorVersion = 6;
        options.scale = level;
        options.scope = MapScope::World;
        options.creationIntent = level == MapLevel::World ? MapCreationIntent::WorldOverview : MapCreationIntent::RegionExploration;
        options.width = 32; options.height = 24;
        options.seed = 9127;
        options.styleId = "illustrated_atlas";
        options.styleVersion = 3; // stale UI value must not clobber the chosen style's version.
        options.styleSnapshotId = "old_snapshot";
        options.qualityAttempts = 1;
        const auto generated = generateSemanticMap(options);
        ok &= require(generated.map.generation.recipe.generatorVersion == 6 &&
                          generated.map.generation.recipe.styleId == "illustrated_atlas" &&
                          generated.map.generation.recipe.styleVersion == 1 &&
                          generated.map.generation.recipe.styleSnapshotId == "illustrated_atlas_v1",
                      "New illustrated World/Region generation must persist explicit V6 routing and immutable illustrated v1 style pins");
        ok &= require(generated.quality.assetCoveragePercent == 100,
                      "The exact procedural illustrated atlas style must report complete renderer coverage independent of its version number");
        MapModel unknownStyle = generated.map;
        unknownStyle.generation.recipe.styleSnapshotId = "illustrated_atlas_unknown";
        ok &= require(analyzeMapGenerationQuality(unknownStyle).assetCoveragePercent == 75,
                      "Unknown illustrated snapshots must not inherit the registered procedural style's coverage exemption");
        options.qualityAttempts = 24;
        const auto batch = generateSemanticMapCandidateBatch(options, 1);
        ok &= require(!batch.candidates.empty() && mapGenQualityMeetsCandidateFloor(batch.candidates.front().map, batch.candidates.front().quality),
                      "Normal illustrated World/Region requests must produce a candidate through the unchanged production quality floor");
    }
    const gm::map_geometry::MapRoadCorridor corridor{"road", {{0, 0}, {10, 0}}, 0.60f};
    ok &= require(gm::map_geometry::roadCorridorIntersectsSegment(corridor, {5, -2}, {5, 2}) &&
                      gm::map_geometry::roadCorridorIntersectsSegment(corridor, {2, 0.55f}, {8, 0.55f}) &&
                      gm::map_geometry::roadCorridorIntersectsSegment(corridor, {5, 0.55f}, {5.01f, 0.55f}) &&
                      !gm::map_geometry::roadCorridorIntersectsSegment(corridor, {2, 2}, {8, 2}),
                  "Relief clearance must catch perpendicular, parallel and arbitrarily short shoulder overlaps while retaining a clear nearby range");
    ok &= require(gm::map_geometry::roadCorridorIntersectsSegment(corridor, {2, 0.8f}, {8, 0.8f}, 0.25f) &&
                      gm::map_geometry::roadClearanceRadiusCells(3.0f / 0.5f) > gm::map_geometry::roadClearanceRadiusCells(3.0f / 8.0f),
                  "Corridor collision must include mountain ink width and the actual minimum-pixel road shoulder at low zoom");
    return ok;
}

bool testV6RegionRiversJoinOnceAndEdgeRoadsRemainLeaves()
{
    bool ok = true;
    MapModel confluence;
    confluence.width = 5; confluence.height = 5;
    confluence.atlasLevel = MapLevel::Region;
    confluence.generation.recipe.generatorVersion = 6;
    resizeMapCells(confluence);
    MapGeneratedFeature existingRiver;
    existingRiver.id = "locked_river"; existingRiver.kind = MapGeneratedFeatureKind::River; existingRiver.locked = true;
    existingRiver.points = {{1.5f, 1.5f}, {2.5f, 2.5f}, {2.5f, 4.5f}};
    confluence.generation.graph.features.push_back(existingRiver);
    const std::vector<MapCellCoordinate> originalBranch{{2, 1}, {1, 2}, {1, 3}};
    auto branch = originalBranch;
    MapFeaturePoint join;
    ok &= require(mapGenV6RegionJoinFirstRiverConfluence(confluence, branch, join) && branch.size() == 2 &&
                      branch.back() == MapCellCoordinate{2, 2} && join.x == 2.5f && join.y == 2.5f &&
                      confluence.generation.graph.features.front().locked &&
                      confluence.generation.graph.features.front().points.size() == existingRiver.points.size(),
                  "A diagonal X between different cells must become one confluence without editing the inherited locked river");
    for (const auto level : {MapLevel::Region, MapLevel::World})
    {
        confluence.atlasLevel = level;
        confluence.generation.recipe.generatorVersion = level == MapLevel::World ? 6 : 5;
        branch = originalBranch;
        ok &= require(!mapGenV6RegionJoinFirstRiverConfluence(confluence, branch, join) && branch == originalBranch,
                      "Region confluence changes must leave World and V1-5 generation paths unchanged");
    }
    MapModel bridgeMap;
    bridgeMap.width = 9; bridgeMap.height = 7;
    bridgeMap.atlasLevel = MapLevel::Region; bridgeMap.generation.recipe.generatorVersion = 6;
    resizeMapCells(bridgeMap);
    for (auto &cell : bridgeMap.cells) cell.kind = MapTileKind::Dirt;
    std::vector<MapCellCoordinate> primary;
    for (int y = 0; y < bridgeMap.height; ++y)
    {
        primary.push_back({4, y});
        mapGenSetRiverCell(bridgeMap, 4, y);
    }
    mapGenV2RasterizeRoad(bridgeMap, {{2, 3}, {3, 3}, {4, 3}, {5, 3}, {6, 3}}, MapWayKind::DirtRoad);
    ok &= require(mapGenV6RegionHasCompleteRiverCrossing(bridgeMap, primary),
                  "An existing bridge with roads on both dry banks must suppress a duplicate automatic crossing");
    mapGenSetRiverCell(bridgeMap, 3, 3);
    ok &= require(!mapGenV6RegionHasCompleteRiverCrossing(bridgeMap, primary),
                  "A partial bridge ending in water must still allow the automatic complete crossing repair");
    mapGenSetCell(bridgeMap, 3, 3, MapTileKind::Mountain);
    mapCellAt(bridgeMap, 3, 3)->wayKind = MapWayKind::DirtRoad;
    ok &= require(!mapGenV6RegionHasCompleteRiverCrossing(bridgeMap, primary),
                  "A road tag on blocked mountain terrain must not count as a traversable bridge bank");
    struct Case { std::uint32_t seed; int width; int height; int water; int mountains; int forests; };
    // Actual selected candidates from the fixed/blind pre-registered gallery,
    // not another best-of-24 search that could silently replace the offender.
    for (const auto &sample : {Case{1161839998u, 48, 32, 20, 18, 25},
                               Case{905422476u, 56, 40, 24, 38, 62}})
    {
        MapGenerationOptions options;
        options.generatorVersion = 6; options.scale = MapLevel::Region; options.scope = MapScope::Local;
        options.styleId = "illustrated_atlas"; options.creationIntent = MapCreationIntent::RegionExploration;
        options.seed = sample.seed; options.width = sample.width; options.height = sample.height;
        options.waterLevel = sample.water; options.mountainDensity = sample.mountains; options.forestDensity = sample.forests;
        options.qualityAttempts = 1;
        const auto result = generateSemanticMap(options);
        const auto &map = result.map;
        std::vector<const MapGeneratedFeature *> rivers;
        std::vector<MapFeaturePoint> roadExits;
        for (const auto &feature : map.generation.graph.features)
        {
            if (feature.kind == MapGeneratedFeatureKind::River)
            {
                for (const auto *existing : rivers)
                    for (std::size_t next = 1; next < feature.points.size(); ++next)
                        for (std::size_t prior = 1; prior < existing->points.size(); ++prior)
                        {
                            const auto a = feature.points[next - 1], b = feature.points[next];
                            const auto c = existing->points[prior - 1], d = existing->points[prior];
                            if (gm::map_geometry::roadSegmentDistanceSquared(a, b, c, d) > 0.00001f)
                                continue;
                            const bool terminalConfluence = next + 1 == feature.points.size() &&
                                gm::map_geometry::roadPointSegmentDistanceSquared(b, c, d) <= 0.00001f;
                            ok &= require(terminalConfluence,
                                "V6 Region river seed " + std::to_string(sample.seed) +
                                " must terminate at its first geometric confluence instead of crossing and rejoining an existing river");
                        }
                rivers.push_back(&feature);
            }
            if ((feature.kind != MapGeneratedFeatureKind::Road && feature.kind != MapGeneratedFeatureKind::Trail) || feature.points.size() < 2)
                continue;
            ok &= require(feature.name != "Regional river crossing" && feature.name != "Bridge approach",
                          "The reproduced Region maps already have bank-to-bank bridges and must not add a redundant rectangular bridge approach");
            for (std::size_t point = 1; point + 1 < feature.points.size(); ++point)
            {
                const auto a = feature.points[point - 1], b = feature.points[point], c = feature.points[point + 1];
                const float ix = b.x - a.x, iy = b.y - a.y, ox = c.x - b.x, oy = c.y - b.y;
                const float length = std::sqrt((ix * ix + iy * iy) * (ox * ox + oy * oy));
                const bool avoidableHairpin = length > .0001f && (ix * ox + iy * oy) / length < -.25f &&
                    mapGenV5WorldRoadSegmentIsSafe(map, a, c) && mapGenV5WorldRoadSegmentIsDry(map, a, c);
                ok &= require(!avoidableHairpin, "V6 Region generated roads must remove a sharp reversal when its direct segment is terrain-safe");
            }
            for (std::size_t point = 1; point < feature.points.size(); ++point)
                ok &= require(mapGenV5WorldRoadSegmentIsSafe(map, feature.points[point - 1], feature.points[point]),
                              "V6 Region road cleanup must retain a terrain-safe path through its existing bridges and passes");
            for (const auto point : {feature.points.front(), feature.points.back()})
                if (point.x <= .51f || point.y <= .51f || point.x >= map.width - .51f || point.y >= map.height - .51f)
                {
                    const bool repeated = std::any_of(roadExits.begin(), roadExits.end(), [&](const auto &exit) {
                        return std::abs(point.x - exit.x) < .01f && std::abs(point.y - exit.y) < .01f;
                    });
                    ok &= require(!repeated, "V6 Region optional edge exits must be leaves, not road hubs that create an artificial edge hairpin (seed " + std::to_string(sample.seed) + ")");
                    roadExits.push_back(point);
                }
        }
        ok &= require(!rivers.empty() && map.generation.recipe.seed == sample.seed,
                      "Region geometry regression must inspect the actual reproduced candidate and retain drainage");
        if (sample.seed == 905422476u)
        {
            options.routeDensity = 72;
            const auto dense = generateSemanticMap(options);
            std::vector<MapFeaturePoint> denseExits;
            for (const auto &feature : dense.map.generation.graph.features)
            {
                if ((feature.kind != MapGeneratedFeatureKind::Road && feature.kind != MapGeneratedFeatureKind::Trail) || feature.points.size() < 2)
                    continue;
                for (const auto point : {feature.points.front(), feature.points.back()})
                    if (point.x <= .51f || point.y <= .51f || point.x >= dense.map.width - .51f || point.y >= dense.map.height - .51f)
                    {
                        ok &= require(std::none_of(denseExits.begin(), denseExits.end(), [&](const auto &exit) {
                                          return std::abs(point.x - exit.x) < .01f && std::abs(point.y - exit.y) < .01f;
                                      }), "High-density optional Region road loops must also keep synthetic map exits as leaves");
                        denseExits.push_back(point);
                    }
            }
        }
    }
    return ok;
}

bool testCanonicalV5DefaultsUseCurrentProductionSnapshot()
{
    const std::string currentSnapshot = mapStyleCurrentProductionSnapshotId();
    const MapCreationRequest request;
    const MapGenerationRecipe recipe;
    return require(!currentSnapshot.empty(), "The current production map snapshot must be available") &&
           require(request.generatorVersion == 5 && recipe.generatorVersion == 5,
                   "Canonical creation defaults must use generator V5") &&
           require(request.styleSnapshotId == currentSnapshot && recipe.styleSnapshotId == currentSnapshot,
                    "Canonical V5 creation defaults must pin the current production snapshot");
}

bool testV5SettlementDensityControlsReadableBuiltFootprint()
{
    MapGenerationOptions sparse;
    sparse.generatorVersion = 5;
    sparse.recipeVersion = 3;
    sparse.scope = MapScope::Local;
    sparse.scale = MapLevel::Site;
    sparse.creationIntent = MapCreationIntent::Settlement;
    sparse.purpose = MapGenerationPurpose::SettlementEnvirons;
    sparse.width = 48;
    sparse.height = 34;
    sparse.seed = 42967u;
    sparse.qualityAttempts = 1;
    sparse.waterLevel = 12;
    sparse.forestDensity = 10;
    sparse.settlementDensity = 8;
    sparse.routeDensity = 52;
    sparse.roadNetwork = MapRoadNetworkMode::Grid;

    MapGenerationOptions dense = sparse;
    dense.settlementDensity = 54;
    const MapGenerationResult sparseResult = generateSemanticMap(sparse);
    const MapGenerationResult denseResult = generateSemanticMap(dense);
    const int sparseBuildings = countTerrain(sparseResult.map, MapTileKind::Building);
    const int denseBuildings = countTerrain(denseResult.map, MapTileKind::Building);
    const int denseGround = countTerrain(denseResult.map, MapTileKind::Dirt);
    const int largestGround = mapGenLargestComponentSize(
        denseResult.map,
        [](const MapCellModel &cell) { return cell.kind == MapTileKind::Dirt; },
        true);

    bool ok = true;
    ok &= require(denseBuildings >= sparseBuildings + 6,
                  "Increasing city density should visibly increase the built footprint");
    ok &= require(denseBuildings <= 30,
                  "Dense settlements should retain the established non-overlap hierarchy cap");
    ok &= require(denseGround > 0 && largestGround * 100 / denseGround >= 70,
                  "Settlement ground should form one readable connected town footprint");
    return ok;
}
}

int main()
{
    enableHeadlessDebugDiagnostics();
    bool ok = true;
    const auto run = [&](const char *name, bool (*test)()) {
        std::cout << "RUN " << name << std::endl;
        const bool passed = test();
        std::cout << (passed ? "PASS " : "FAIL ") << name << std::endl;
        ok &= passed;
    };
    run("testV10WorldTopologyAndRequestCoverage", testV10WorldTopologyAndRequestCoverage);
    run("testV9NaturalRoadAlignment", testV9NaturalRoadAlignment);
    run("testV8AtlasBasinsAndRegionalRelief", testV8AtlasBasinsAndRegionalRelief);
    run("testV7TerrainFollowingRoadsAndVersionPins", testV7TerrainFollowingRoadsAndVersionPins);
    run("testV7CrossingApproachAndLocalExits", testV7CrossingApproachAndLocalExits);
    run("testV6BoundedMountainPassesAndRoadClearance", testV6BoundedMountainPassesAndRoadClearance);
    run("testV6RegionRiversJoinOnceAndEdgeRoadsRemainLeaves", testV6RegionRiversJoinOnceAndEdgeRoadsRemainLeaves);
    run("testDeterministicSeed", testDeterministicSeed);
    run("testDensityControls", testDensityControls);
    run("testMountainPatterns", testMountainPatterns);
    run("testWayGeneration", testWayGeneration);
    run("testAppDefaultCityGenerationUsesStoneRoadCandidateGround", testAppDefaultCityGenerationUsesStoneRoadCandidateGround);
    run("testRoadGenerationPreservesGroundContext", testRoadGenerationPreservesGroundContext);
    run("testRiverGenerationInfluencesDiagonalCorners", testRiverGenerationInfluencesDiagonalCorners);
    run("testCandidatePreviewPolicy", testCandidatePreviewPolicy);
    run("testBridgeSequenceRotation", testBridgeSequenceRotation);
    run("testCaveAndTacticalGeneration", testCaveAndTacticalGeneration);
    run("testStructureWaterBuffer", testStructureWaterBuffer);
    run("testGeneratedArtContractWayNormalization", testGeneratedArtContractWayNormalization);
    run("testPostTacticalStructureApproaches", testPostTacticalStructureApproaches);
    run("testGenerationProgressCallback", testGenerationProgressCallback);
    run("testCaveGenerationProgressCallback", testCaveGenerationProgressCallback);
    run("testDeterministicQualitySearch", testDeterministicQualitySearch);
    run("testQualityAnalysisDetectsDisconnectedDungeon", testQualityAnalysisDetectsDisconnectedDungeon);
    run("testV2FeatureGraphAndRankedDesigns", testV2FeatureGraphAndRankedDesigns);
    run("testWorldBriefAndNamedTerritories", testWorldBriefAndNamedTerritories);
    run("testNoteDrivenWorldV4QaContract", testNoteDrivenWorldV4QaContract);
    run("testSharedGreatRiverAtEditorAndPresetSizes", testSharedGreatRiverAtEditorAndPresetSizes);
    run("testV5WorldCompositionRejectsScreenshotRegressions", testV5WorldCompositionRejectsScreenshotRegressions);
    run("testV5ConstraintWorldAvoidsCoastalRoadAndKeepsPrincipalPlacesConnected", testV5ConstraintWorldAvoidsCoastalRoadAndKeepsPrincipalPlacesConnected);
    run("testV5ReleaseWorldRegressionSeedsMeetDraftBudget", testV5ReleaseWorldRegressionSeedsMeetDraftBudget);
    run("testGenerationRequestContradictionsFailBeforeAttempts", testGenerationRequestContradictionsFailBeforeAttempts);
    run("testGenerationCancellationNeverPublishesPartialCandidates", testGenerationCancellationNeverPublishesPartialCandidates);
    run("testCandidateSelectionPrefersDistinctCompositions", testCandidateSelectionPrefersDistinctCompositions);
    run("testV5NameCulturesAreDeterministicAndDistinct", testV5NameCulturesAreDeterministicAndDistinct);
    run("testV2OutdoorGenerationProperties", testV2OutdoorGenerationProperties);
    run("testV4ScaleSpecificProductionPipeline", testV4ScaleSpecificProductionPipeline);
    run("testV4ExcludedElementsAreAppliedBeforeQualitySelection", testV4ExcludedElementsAreAppliedBeforeQualitySelection);
    run("testV4RiverElevationsRemainPersistableAndDownhillAfterCrossings", testV4RiverElevationsRemainPersistableAndDownhillAfterCrossings);
    run("testV5SettlementDensityControlsReadableBuiltFootprint", testV5SettlementDensityControlsReadableBuiltFootprint);
    run("testCanonicalV5DefaultsUseCurrentProductionSnapshot", testCanonicalV5DefaultsUseCurrentProductionSnapshot);
    return ok ? 0 : 1;
}
