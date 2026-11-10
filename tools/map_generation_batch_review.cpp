#include "Models/Map/MapRuntimeVisualAssets.hpp"
#include "Models/Map/MapTileSemantics.hpp"
#include "Models/Map/MapGeneration.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
constexpr size_t kWayKindCount = static_cast<size_t>(MapWayKind::CaveTunnel) + 1;
constexpr size_t kTileKindCount = static_cast<size_t>(MapTileKind::Bridge) + 1;
constexpr int kMaxDiagnosticSamples = 8;

struct Color
{
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
};

struct Image
{
    int width = 1;
    int height = 1;
    std::vector<Color> pixels;

    Image(int imageWidth, int imageHeight, Color fill)
        : width(imageWidth), height(imageHeight), pixels(static_cast<size_t>(imageWidth * imageHeight), fill)
    {
    }
};

struct Diagnostic
{
    std::string severity;
    std::string label;
    std::string message;
    std::string semanticContext;
    std::string selectedAssetId;
    std::string fallbackReason;
    std::vector<MapCellCoordinate> samples;
};

struct ContextualCoverageSample
{
    std::string domain;
    std::string semanticContext;
    std::string workOrderId;
    std::string selectedAssetId;
    std::string candidatePreviewAssetId;
    std::string fallbackReason;
    std::string qaLabel;
    int targetAssetCount = 0;
    int acceptedAssetCount = 0;
    int candidateAssetCount = 0;
};

struct TileRunIssue
{
    MapTileKind kind = MapTileKind::Empty;
    int size = 0;
    int distinctSignatures = 0;
    int dominantPercent = 0;
    MapCellCoordinate sample;
};

enum class ReviewFixture
{
    None,
    GrassIntegratedWays
};

struct QualityMetrics
{
    int roadCells = 0;
    int roadBorderCells = 0;
    int roadAxisOnlyCells = 0;
    int roadWaterCrossings = 0;
    int gridWaterRoadCells = 0;
    int roadWayCells = 0;
    int roadIntegratedGroundCells = 0;
    int roadBridgeWayCells = 0;
    int roadOverlayOnlyCells = 0;
    int roadOverlayTerrainKinds = 0;
    int roadOverlayDominantTerrainPercent = 0;
    MapTileKind roadOverlayDominantTerrain = MapTileKind::Empty;
    int distinctRoadWayKinds = 0;
    std::array<int, kWayKindCount> roadWayKindCounts{};
    std::array<int, kTileKindCount> roadOverlayTerrainCounts{};
    std::vector<MapCellCoordinate> roadWaySamples;
    std::vector<MapCellCoordinate> roadOverlayOnlySamples;
    int bridgeCells = 0;
    int bridgeComponents = 0;
    int bridgeDistinctContexts = 0;
    int bridgeComponentDistinctContexts = 0;
    int bridgeApproachlessCells = 0;
    int bridgeOneSidedApproachCells = 0;
    int bridgeWithoutCrossingContextCells = 0;
    int bridgeWithoutBankContextCells = 0;
    int bridgeDominantContextPercent = 0;
    int bridgeComponentDominantContextPercent = 0;
    std::vector<MapCellCoordinate> bridgeApproachlessSamples;
    std::vector<MapCellCoordinate> bridgeOneSidedApproachSamples;
    std::vector<MapCellCoordinate> bridgeWithoutCrossingContextSamples;
    std::vector<MapCellCoordinate> bridgeWithoutBankContextSamples;
    std::vector<MapCellCoordinate> bridgeSamples;
    std::vector<MapCellCoordinate> bridgeSequenceSamples;
    std::vector<std::string> bridgeSequenceSampleSpanPositions;
    int structureCells = 0;
    int contextlessStructureCells = 0;
    int structureContextKinds = 0;
    int structureDistinctContextSignatures = 0;
    int structureDominantContextPercent = 0;
    int structureWithoutApproachCells = 0;
    int structureHostileContextCells = 0;
    int structureBridgeAdjacentCells = 0;
    int structureWaterfrontCells = 0;
    std::vector<MapCellCoordinate> structureWithoutApproachSamples;
    std::vector<MapCellCoordinate> structureHostileContextSamples;
    std::vector<MapCellCoordinate> structureBridgeAdjacentSamples;
    std::vector<MapCellCoordinate> structureWaterfrontSamples;
    std::vector<MapCellCoordinate> structureSamples;
    int waterAdjacentSettlementCells = 0;
    int mountainComponents = 0;
    int mountainRangeComponents = 0;
    int mountainRangeCells = 0;
    int mountainRangeComponentsNotContinuous = 0;
    int mountainRangeCellsNotContinuous = 0;
    int slabbyMountainComponents = 0;
    int largestMountainComponent = 0;
    int individualPeakComponents = 0;
    std::vector<MapCellCoordinate> mountainRangeNotContinuousSamples;
    std::vector<MapCellCoordinate> mountainRangeSamples;
    std::vector<std::string> mountainRangeSampleRoles;
    int forestCells = 0;
    int forestComponents = 0;
    int largestForestComponent = 0;
    int lowVarianceTileRuns = 0;
    int largestLowVarianceTileRun = 0;
    std::vector<TileRunIssue> lowVarianceTileRunSamples;
};

struct ReviewCase
{
    std::string name;
    std::string intent;
    MapGenerationOptions options;
    ReviewFixture fixture = ReviewFixture::None;
};

struct GenerationStageTiming
{
    std::string stage;
    int step = 0;
    int totalSteps = 0;
    long long elapsedMs = 0;
};

struct ReviewResult
{
    ReviewCase reviewCase;
    MapGenerationResult generated;
    QualityMetrics metrics;
    std::vector<Diagnostic> diagnostics;
    std::vector<ContextualCoverageSample> contextualCoverageSamples;
    std::vector<GenerationStageTiming> generationStageTimings;
    long long generationElapsedMs = 0;
    long long metricsElapsedMs = 0;
    long long validationElapsedMs = 0;
    long long elapsedMs = 0;
};

struct ReviewTimingSummary
{
    long long totalElapsedMs = 0;
    double averageElapsedMs = 0.0;
    long long maxElapsedMs = 0;
    std::string slowestCase;
};

struct IsolatedWayKindSummary
{
    MapWayKind kind = MapWayKind::None;
    int count = 0;
    std::vector<MapCellCoordinate> samples;
};

struct IsolatedWaySummary
{
    int total = 0;
    std::vector<IsolatedWayKindSummary> byKind;
};

void putPixel(Image &image, int x, int y, Color color)
{
    if (x < 0 || y < 0 || x >= image.width || y >= image.height)
        return;
    image.pixels[static_cast<size_t>(y * image.width + x)] = color;
}

void fillRect(Image &image, int x0, int y0, int width, int height, Color color)
{
    for (int y = y0; y < y0 + height; ++y)
    {
        for (int x = x0; x < x0 + width; ++x)
            putPixel(image, x, y, color);
    }
}

void drawRect(Image &image, int x0, int y0, int width, int height, Color color)
{
    for (int x = x0; x < x0 + width; ++x)
    {
        putPixel(image, x, y0, color);
        putPixel(image, x, y0 + height - 1, color);
    }
    for (int y = y0; y < y0 + height; ++y)
    {
        putPixel(image, x0, y, color);
        putPixel(image, x0 + width - 1, y, color);
    }
}

void drawLine(Image &image, int x0, int y0, int x1, int y1, Color color, int thickness = 1)
{
    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;

    for (;;)
    {
        fillRect(image, x0 - thickness / 2, y0 - thickness / 2, thickness, thickness, color);
        if (x0 == x1 && y0 == y1)
            break;
        const int doubledError = 2 * error;
        if (doubledError >= dy)
        {
            error += dy;
            x0 += sx;
        }
        if (doubledError <= dx)
        {
            error += dx;
            y0 += sy;
        }
    }
}

void writeU16(std::ofstream &out, std::uint16_t value)
{
    out.put(static_cast<char>(value & 0xff));
    out.put(static_cast<char>((value >> 8) & 0xff));
}

void writeU32(std::ofstream &out, std::uint32_t value)
{
    writeU16(out, static_cast<std::uint16_t>(value & 0xffff));
    writeU16(out, static_cast<std::uint16_t>((value >> 16) & 0xffff));
}

bool writeBmp(const std::filesystem::path &path, const Image &image)
{
    std::ofstream out(path, std::ios::binary);
    if (!out)
        return false;

    const int rowBytes = image.width * 3;
    const int padding = (4 - (rowBytes % 4)) % 4;
    const int pixelBytes = (rowBytes + padding) * image.height;

    out.put('B');
    out.put('M');
    writeU32(out, static_cast<std::uint32_t>(54 + pixelBytes));
    writeU16(out, 0);
    writeU16(out, 0);
    writeU32(out, 54);
    writeU32(out, 40);
    writeU32(out, static_cast<std::uint32_t>(image.width));
    writeU32(out, static_cast<std::uint32_t>(image.height));
    writeU16(out, 1);
    writeU16(out, 24);
    writeU32(out, 0);
    writeU32(out, static_cast<std::uint32_t>(pixelBytes));
    writeU32(out, 2835);
    writeU32(out, 2835);
    writeU32(out, 0);
    writeU32(out, 0);

    const std::array<char, 3> pad = {0, 0, 0};
    for (int y = image.height - 1; y >= 0; --y)
    {
        for (int x = 0; x < image.width; ++x)
        {
            const Color color = image.pixels[static_cast<size_t>(y * image.width + x)];
            out.put(static_cast<char>(color.b));
            out.put(static_cast<char>(color.g));
            out.put(static_cast<char>(color.r));
        }
        out.write(pad.data(), padding);
    }
    return true;
}

Color terrainColor(MapTileKind kind)
{
    switch (kind)
    {
    case MapTileKind::Empty:
        return {116, 171, 87};
    case MapTileKind::Water:
        return {59, 125, 190};
    case MapTileKind::Forest:
        return {35, 104, 53};
    case MapTileKind::Mountain:
        return {118, 121, 118};
    case MapTileKind::Hill:
        return {115, 143, 70};
    case MapTileKind::Road:
        return {148, 107, 66};
    case MapTileKind::Settlement:
        return {213, 148, 61};
    case MapTileKind::Building:
        return {150, 87, 62};
    case MapTileKind::Room:
        return {112, 105, 116};
    case MapTileKind::Wall:
        return {42, 45, 50};
    case MapTileKind::Door:
        return {176, 132, 54};
    case MapTileKind::Cave:
        return {93, 83, 66};
    case MapTileKind::Stairs:
        return {218, 194, 94};
    case MapTileKind::Hazard:
        return {180, 70, 130};
    case MapTileKind::Landmark:
        return {232, 203, 84};
    case MapTileKind::Dirt:
        return {159, 124, 75};
    case MapTileKind::Stone:
        return {133, 137, 132};
    case MapTileKind::Snow:
        return {224, 231, 229};
    case MapTileKind::Desert:
        return {210, 177, 104};
    case MapTileKind::Swamp:
        return {83, 110, 72};
    case MapTileKind::Lava:
        return {212, 66, 37};
    case MapTileKind::Ice:
        return {172, 219, 228};
    case MapTileKind::Pit:
        return {30, 25, 24};
    case MapTileKind::Rubble:
        return {120, 105, 88};
    case MapTileKind::Cover:
        return {65, 134, 124};
    case MapTileKind::Chasm:
        return {15, 18, 24};
    case MapTileKind::Bridge:
        return {145, 98, 53};
    }
    return {255, 0, 255};
}

Color wayColor(MapWayKind kind)
{
    switch (kind)
    {
    case MapWayKind::Trail:
        return {224, 198, 126};
    case MapWayKind::DirtRoad:
        return {111, 77, 42};
    case MapWayKind::StoneRoad:
        return {198, 195, 184};
    case MapWayKind::River:
        return {53, 151, 217};
    case MapWayKind::Canal:
        return {90, 184, 204};
    case MapWayKind::DungeonCorridor:
        return {226, 189, 91};
    case MapWayKind::CaveTunnel:
        return {197, 151, 78};
    case MapWayKind::None:
        return {0, 0, 0};
    }
    return {255, 0, 255};
}

int wayCount(const MapModel &map, MapWayKind kind)
{
    int count = 0;
    for (const MapCellModel &cell : map.cells)
    {
        if (cell.wayKind == kind)
            ++count;
    }
    return count;
}

int terrainCount(const MapModel &map, MapTileKind kind)
{
    int count = 0;
    for (const MapCellModel &cell : map.cells)
    {
        if (cell.kind == kind)
            ++count;
    }
    return count;
}

bool isRoadWay(MapWayKind kind)
{
    return kind == MapWayKind::Trail || kind == MapWayKind::DirtRoad || kind == MapWayKind::StoneRoad;
}

bool isRoadCell(const MapCellModel &cell)
{
    return isRoadWay(cell.wayKind) || cell.kind == MapTileKind::Road || cell.kind == MapTileKind::Bridge;
}

double percentOf(int value, int total);

void addCoordinateSample(std::vector<MapCellCoordinate> &samples, int x, int y)
{
    if (static_cast<int>(samples.size()) < kMaxDiagnosticSamples)
        samples.push_back({x, y});
}

bool coordinateEquals(const MapCellCoordinate &left, const MapCellCoordinate &right)
{
    return left.x == right.x && left.y == right.y;
}

void addUniqueCoordinate(std::vector<MapCellCoordinate> &coordinates, MapCellCoordinate coordinate)
{
    const auto found = std::find_if(coordinates.begin(), coordinates.end(), [&](const MapCellCoordinate &existing) {
        return coordinateEquals(existing, coordinate);
    });
    if (found == coordinates.end())
        coordinates.push_back(coordinate);
}

void addUniqueText(std::vector<std::string> &items, const std::string &item)
{
    if (!item.empty() && std::find(items.begin(), items.end(), item) == items.end())
        items.push_back(item);
}

bool isExplicitRoadbedTerrain(MapTileKind kind)
{
    return kind == MapTileKind::Road || kind == MapTileKind::Bridge;
}

bool isBridgeApproachCell(const MapCellModel &cell)
{
    return isRoadWay(cell.wayKind) || cell.kind == MapTileKind::Road || cell.kind == MapTileKind::Dirt || cell.kind == MapTileKind::Stone || cell.kind == MapTileKind::Settlement;
}

bool isStructureApproachCell(const MapCellModel &cell)
{
    return isBridgeApproachCell(cell) || mapWayKindIsInteriorLike(cell.wayKind);
}

bool isStructureLikeTile(MapTileKind kind)
{
    return kind == MapTileKind::Settlement || kind == MapTileKind::Building || kind == MapTileKind::Landmark;
}

bool isHostileStructureContext(MapTileKind kind)
{
    return kind == MapTileKind::Water ||
           kind == MapTileKind::Swamp ||
           kind == MapTileKind::Lava ||
           kind == MapTileKind::Chasm ||
           kind == MapTileKind::Pit ||
           kind == MapTileKind::Cave ||
           kind == MapTileKind::Room ||
           kind == MapTileKind::Wall;
}

bool isBridgeCrossingContext(const MapCellModel &cell)
{
    return cell.kind == MapTileKind::Water ||
           cell.kind == MapTileKind::Swamp ||
           cell.kind == MapTileKind::Lava ||
           cell.kind == MapTileKind::Chasm ||
           cell.kind == MapTileKind::Pit ||
           mapWayKindIsWaterLike(cell.wayKind);
}

bool isBridgeBankContext(const MapCellModel &cell)
{
    if (cell.kind == MapTileKind::Bridge || isBridgeCrossingContext(cell))
        return false;
    return cell.kind != MapTileKind::Wall && cell.kind != MapTileKind::Building;
}

bool hasRoadApproach(const MapModel &map, int x, int y)
{
    const MapCellCoordinate neighbors[] = {
        {x, y - 1},
        {x + 1, y},
        {x, y + 1},
        {x - 1, y}};
    for (const MapCellCoordinate &neighbor : neighbors)
    {
        const MapCellModel *cell = mapCellAt(map, neighbor.x, neighbor.y);
        if (cell && isStructureApproachCell(*cell))
            return true;
    }
    return false;
}

int bridgeLocalContextSignature(const MapModel &map, int x, int y)
{
    int bridgeMask = 0;
    int approachMask = 0;
    int waterMask = 0;
    const MapCellModel *owner = mapCellAt(map, x, y);
    const std::array<std::pair<int, MapCellCoordinate>, 4> directions = {
        std::make_pair(1, MapCellCoordinate{x, y - 1}),
        std::make_pair(2, MapCellCoordinate{x + 1, y}),
        std::make_pair(4, MapCellCoordinate{x, y + 1}),
        std::make_pair(8, MapCellCoordinate{x - 1, y})};
    for (const auto &[bit, coordinate] : directions)
    {
        const MapCellModel *neighbor = mapCellAt(map, coordinate.x, coordinate.y);
        if (!neighbor)
            continue;
        if (neighbor->kind == MapTileKind::Bridge)
            bridgeMask |= bit;
        else if (isBridgeApproachCell(*neighbor))
            approachMask |= bit;
        if (neighbor->kind == MapTileKind::Water || neighbor->kind == MapTileKind::Swamp || mapWayKindIsWaterLike(neighbor->wayKind))
            waterMask |= bit;
    }
    const int wayKind = owner ? static_cast<int>(owner->wayKind) : 0;
    return bridgeMask | (approachMask << 4) | (waterMask << 8) | (wayKind << 12);
}

MapTileRenderContext tileContextAt(const MapModel &map, int x, int y)
{
    return makeMapTileRenderContext(
        mapCellAt(map, x, y),
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
}

void addMountainComponentRoleSamples(
    const MapModel &map,
    const std::vector<MapCellCoordinate> &componentCells,
    std::vector<MapCellCoordinate> &samples,
    std::vector<std::string> &sampleRoles)
{
    std::vector<std::string> sampledRoles;
    for (const MapCellCoordinate &coordinate : componentCells)
    {
        const std::string role = mapStyleComponentRoleKey(tileContextAt(map, coordinate.x, coordinate.y));
        if (role.empty() || std::find(sampledRoles.begin(), sampledRoles.end(), role) != sampledRoles.end())
            continue;

        addCoordinateSample(samples, coordinate.x, coordinate.y);
        sampledRoles.push_back(role);
        addUniqueText(sampleRoles, role);
    }

    if (sampledRoles.empty() && !componentCells.empty())
    {
        addCoordinateSample(samples, componentCells.front().x, componentCells.front().y);
        addUniqueText(sampleRoles, mapStyleComponentRoleKey(tileContextAt(map, componentCells.front().x, componentCells.front().y)));
    }
}

int artReadinessTileRunSignature(const MapModel &map, int x, int y, const MapCellModel &cell)
{
    const MapTileRenderContext context = tileContextAt(map, x, y);
    const MapTileSemanticResolution resolution = resolveMapTileSemantics(context);
    int visualVariant = resolution.sameFamilyVariant;
    if (resolution.contextualSymbolVariant.enabled)
        visualVariant = resolution.contextualSymbolVariant.shapeVariant;

    std::uint32_t signature = 2166136261u;
    auto mix = [&](int value) {
        signature ^= static_cast<std::uint32_t>(value + 0x9e3779b9);
        signature *= 16777619u;
    };

    mix(static_cast<int>(cell.kind));
    mix(resolution.sameFamilyMask);
    mix(visualVariant);
    mix(static_cast<int>(cell.symbolVariantMode));
    mix(static_cast<int>(cell.wayKind));
    mix(normalizeMapRotation(cell.rotationQuarterTurns));
    mix(cell.rotationLocked ? 1 : 0);
    mix(static_cast<int>(resolution.neighborRelationships.size()));
    for (const MapTileResolvedTransitionOverlay &overlay : resolution.transitionOverlays)
    {
        mix(static_cast<int>(overlay.neighborGroup));
        mix(overlay.mask);
        mix(overlay.variant);
    }
    return static_cast<int>(signature & 0x7fffffffu);
}

int bridgeComponentSignature(int size, int minX, int minY, int maxX, int maxY, int approachEndpointCount, bool hasCrossingContext, bool hasBankContext)
{
    const int width = maxX - minX + 1;
    const int height = maxY - minY + 1;
    return std::min(size, 15) |
           (std::min(width, 15) << 4) |
           (std::min(height, 15) << 8) |
           (std::min(approachEndpointCount, 3) << 12) |
           (hasCrossingContext ? (1 << 14) : 0) |
           (hasBankContext ? (1 << 15) : 0);
}

int coarseStructureContextKind(MapTileKind kind)
{
    switch (kind)
    {
    case MapTileKind::Empty:
    case MapTileKind::Dirt:
    case MapTileKind::Stone:
    case MapTileKind::Snow:
    case MapTileKind::Desert:
    case MapTileKind::Ice:
        return 1;
    case MapTileKind::Forest:
    case MapTileKind::Hill:
    case MapTileKind::Swamp:
        return 2;
    case MapTileKind::Water:
    case MapTileKind::Lava:
    case MapTileKind::Pit:
    case MapTileKind::Chasm:
        return 3;
    case MapTileKind::Road:
    case MapTileKind::Bridge:
        return 4;
    case MapTileKind::Settlement:
    case MapTileKind::Building:
    case MapTileKind::Landmark:
        return 5;
    case MapTileKind::Room:
    case MapTileKind::Wall:
    case MapTileKind::Door:
    case MapTileKind::Cave:
    case MapTileKind::Stairs:
        return 6;
    case MapTileKind::Hazard:
    case MapTileKind::Rubble:
    case MapTileKind::Cover:
        return 7;
    case MapTileKind::Mountain:
        return 8;
    }
    return 0;
}

int structureLocalContextSignature(const MapModel &map, int x, int y)
{
    const MapCellModel *owner = mapCellAt(map, x, y);
    int signature = owner ? static_cast<int>(owner->kind) : 0;
    const std::array<std::pair<int, MapCellCoordinate>, 8> directions = {
        std::make_pair(0, MapCellCoordinate{x, y - 1}),
        std::make_pair(1, MapCellCoordinate{x + 1, y}),
        std::make_pair(2, MapCellCoordinate{x, y + 1}),
        std::make_pair(3, MapCellCoordinate{x - 1, y}),
        std::make_pair(4, MapCellCoordinate{x + 1, y - 1}),
        std::make_pair(5, MapCellCoordinate{x + 1, y + 1}),
        std::make_pair(6, MapCellCoordinate{x - 1, y + 1}),
        std::make_pair(7, MapCellCoordinate{x - 1, y - 1})};

    for (const auto &[slot, coordinate] : directions)
    {
        const MapCellModel *neighbor = mapCellAt(map, coordinate.x, coordinate.y);
        if (!neighbor || isStructureLikeTile(neighbor->kind))
            continue;
        signature ^= (coarseStructureContextKind(neighbor->kind) + static_cast<int>(neighbor->wayKind) * 11) << (slot % 4 * 4);
    }
    return signature;
}

int distinctCountAndDominantPercent(std::vector<int> values, int total)
{
    if (values.empty() || total <= 0)
        return 0;
    std::sort(values.begin(), values.end());
    int dominant = 1;
    int run = 1;
    for (size_t i = 1; i < values.size(); ++i)
    {
        if (values[i] == values[i - 1])
            ++run;
        else
        {
            dominant = std::max(dominant, run);
            run = 1;
        }
    }
    dominant = std::max(dominant, run);
    return static_cast<int>(percentOf(dominant, total) + 0.5);
}

int distinctCount(std::vector<int> values)
{
    if (values.empty())
        return 0;
    std::sort(values.begin(), values.end());
    return static_cast<int>(std::unique(values.begin(), values.end()) - values.begin());
}

bool shouldCheckTileRunVariance(MapTileKind kind)
{
    switch (kind)
    {
    case MapTileKind::Empty:
    case MapTileKind::Water:
    case MapTileKind::Forest:
    case MapTileKind::Mountain:
    case MapTileKind::Hill:
    case MapTileKind::Road:
    case MapTileKind::Settlement:
    case MapTileKind::Building:
    case MapTileKind::Room:
    case MapTileKind::Wall:
    case MapTileKind::Cave:
    case MapTileKind::Dirt:
    case MapTileKind::Stone:
    case MapTileKind::Snow:
    case MapTileKind::Desert:
    case MapTileKind::Swamp:
    case MapTileKind::Ice:
    case MapTileKind::Bridge:
        return true;
    case MapTileKind::Door:
    case MapTileKind::Stairs:
    case MapTileKind::Hazard:
    case MapTileKind::Landmark:
    case MapTileKind::Lava:
    case MapTileKind::Pit:
    case MapTileKind::Rubble:
    case MapTileKind::Cover:
    case MapTileKind::Chasm:
        return false;
    }
    return false;
}

int minimumTileRunSizeForVariance(MapTileKind kind)
{
    switch (kind)
    {
    case MapTileKind::Empty:
        return 80;
    case MapTileKind::Water:
        return 35;
    case MapTileKind::Settlement:
    case MapTileKind::Building:
    case MapTileKind::Bridge:
        return 6;
    case MapTileKind::Mountain:
    case MapTileKind::Forest:
    case MapTileKind::Room:
    case MapTileKind::Cave:
    case MapTileKind::Dirt:
    case MapTileKind::Stone:
        return 24;
    default:
        return 30;
    }
}

bool isBorderCell(const MapModel &map, int x, int y)
{
    return x == 0 || y == 0 || x == map.width - 1 || y == map.height - 1;
}

double percentOf(int value, int total)
{
    if (total <= 0)
        return 0.0;
    return static_cast<double>(value) * 100.0 / static_cast<double>(total);
}

MapWayRenderContext wayContextAt(const MapModel &map, int x, int y);

int orthogonalNeighborCount(const MapModel &map, int x, int y, MapTileKind kind)
{
    int count = 0;
    const MapCellCoordinate neighbors[] = {
        {x, y - 1},
        {x + 1, y},
        {x, y + 1},
        {x - 1, y}};
    for (const MapCellCoordinate &neighbor : neighbors)
    {
        const MapCellModel *cell = mapCellAt(map, neighbor.x, neighbor.y);
        if (cell && cell->kind == kind)
            ++count;
    }
    return count;
}

bool componentLooksSlabby(int size, int minX, int minY, int maxX, int maxY)
{
    const int width = maxX - minX + 1;
    const int height = maxY - minY + 1;
    const int boxArea = std::max(1, width * height);
    return size >= 18 && width >= 5 && height >= 4 && percentOf(size, boxArea) >= 72.0;
}

void measureBridgeComponents(const MapModel &map, QualityMetrics &metrics)
{
    std::vector<bool> visited(map.cells.size(), false);
    std::vector<int> componentSignatures;
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const int startIndex = y * map.width + x;
            const MapCellModel *start = mapCellAt(map, x, y);
            if (!start || start->kind != MapTileKind::Bridge || visited[static_cast<size_t>(startIndex)])
                continue;

            int componentSize = 0;
            int minX = x;
            int minY = y;
            int maxX = x;
            int maxY = y;
            bool hasCrossingContext = false;
            bool hasBankContext = false;
            std::vector<MapCellCoordinate> approachEndpoints;
            std::queue<MapCellCoordinate> pending;
            pending.push({x, y});
            visited[static_cast<size_t>(startIndex)] = true;

            while (!pending.empty())
            {
                const MapCellCoordinate current = pending.front();
                pending.pop();
                ++componentSize;
                minX = std::min(minX, current.x);
                minY = std::min(minY, current.y);
                maxX = std::max(maxX, current.x);
                maxY = std::max(maxY, current.y);

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
                    const MapCellModel *neighborCell = mapCellAt(map, neighbor.x, neighbor.y);
                    if (!neighborCell)
                        continue;

                    if (neighborCell->kind == MapTileKind::Bridge)
                    {
                        if (!visited[static_cast<size_t>(neighborIndex)])
                        {
                            visited[static_cast<size_t>(neighborIndex)] = true;
                            pending.push(neighbor);
                        }
                        continue;
                    }

                    if (isBridgeApproachCell(*neighborCell))
                        addUniqueCoordinate(approachEndpoints, neighbor);
                    if (isBridgeCrossingContext(*neighborCell))
                        hasCrossingContext = true;
                    if (isBridgeBankContext(*neighborCell))
                        hasBankContext = true;
                }
            }

            ++metrics.bridgeComponents;
            componentSignatures.push_back(bridgeComponentSignature(componentSize, minX, minY, maxX, maxY, static_cast<int>(approachEndpoints.size()), hasCrossingContext, hasBankContext));

            if (approachEndpoints.empty())
            {
                metrics.bridgeApproachlessCells += componentSize;
                addCoordinateSample(metrics.bridgeApproachlessSamples, x, y);
            }
            else if (approachEndpoints.size() == 1)
            {
                metrics.bridgeOneSidedApproachCells += componentSize;
                addCoordinateSample(metrics.bridgeOneSidedApproachSamples, x, y);
            }

            if (!hasCrossingContext)
            {
                metrics.bridgeWithoutCrossingContextCells += componentSize;
                addCoordinateSample(metrics.bridgeWithoutCrossingContextSamples, x, y);
            }
            if (!hasBankContext)
            {
                metrics.bridgeWithoutBankContextCells += componentSize;
                addCoordinateSample(metrics.bridgeWithoutBankContextSamples, x, y);
            }
        }
    }

    metrics.bridgeComponentDistinctContexts = distinctCount(componentSignatures);
    metrics.bridgeComponentDominantContextPercent = distinctCountAndDominantPercent(componentSignatures, metrics.bridgeComponents);
}

void measureLowVarianceTileRuns(const MapModel &map, QualityMetrics &metrics)
{
    std::vector<bool> visited(map.cells.size(), false);
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const int startIndex = y * map.width + x;
            const MapCellModel *start = mapCellAt(map, x, y);
            if (!start || visited[static_cast<size_t>(startIndex)] || !shouldCheckTileRunVariance(start->kind))
                continue;

            const MapTileKind kind = start->kind;
            const int minimumSize = minimumTileRunSizeForVariance(kind);
            int size = 0;
            std::vector<int> signatures;
            std::queue<MapCellCoordinate> pending;
            pending.push({x, y});
            visited[static_cast<size_t>(startIndex)] = true;

            while (!pending.empty())
            {
                const MapCellCoordinate current = pending.front();
                pending.pop();
                const MapCellModel *cell = mapCellAt(map, current.x, current.y);
                if (!cell)
                    continue;

                ++size;
                signatures.push_back(artReadinessTileRunSignature(map, current.x, current.y, *cell));

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
                    if (neighborCell && neighborCell->kind == kind)
                    {
                        visited[static_cast<size_t>(neighborIndex)] = true;
                        pending.push(neighbor);
                    }
                }
            }

            if (size < minimumSize)
                continue;

            const int distinctSignatures = distinctCount(signatures);
            const int dominantPercent = distinctCountAndDominantPercent(signatures, size);
            if (distinctSignatures <= 2 || dominantPercent >= 88)
            {
                ++metrics.lowVarianceTileRuns;
                metrics.largestLowVarianceTileRun = std::max(metrics.largestLowVarianceTileRun, size);
                if (static_cast<int>(metrics.lowVarianceTileRunSamples.size()) < kMaxDiagnosticSamples)
                    metrics.lowVarianceTileRunSamples.push_back({kind, size, distinctSignatures, dominantPercent, {x, y}});
            }
        }
    }
}

QualityMetrics measureQuality(const MapModel &map, const MapGenerationOptions &options, bool artReadiness)
{
    QualityMetrics metrics;
    metrics.forestCells = terrainCount(map, MapTileKind::Forest);
    std::vector<int> bridgeContextSignatures;
    std::vector<int> structureContextSignatures;
    std::array<int, kTileKindCount> structureContextCounts{};

    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell)
                continue;

            if (isRoadCell(*cell))
            {
                ++metrics.roadCells;
                if (isBorderCell(map, x, y))
                    ++metrics.roadBorderCells;
                const int mask = cell->wayKind == MapWayKind::None ? 0 : mapWayEightNeighborMask(wayContextAt(map, x, y));
                if ((mask & (2 | 8 | 32 | 128)) == 0)
                    ++metrics.roadAxisOnlyCells;
                if (cell->kind == MapTileKind::Water)
                {
                    ++metrics.roadWaterCrossings;
                    if (options.roadNetwork == MapRoadNetworkMode::Grid)
                        ++metrics.gridWaterRoadCells;
                }
            }

            if (isRoadWay(cell->wayKind))
            {
                ++metrics.roadWayCells;
                ++metrics.roadWayKindCounts[static_cast<size_t>(cell->wayKind)];
                addCoordinateSample(metrics.roadWaySamples, x, y);
                bool hasAcceptedContextualWayAsset = false;
                if (artReadiness && !isExplicitRoadbedTerrain(cell->kind))
                {
                    const MapStyleContextualAssetRequest wayRequest = mapStyleContextualAssetRequestForWayVisual(wayContextAt(map, x, y), options.styleId);
                    if (!wayRequest.owner.empty() && wayRequest.assetClass == MapStyleContextualAssetClass::IntegratedWayGround)
                        hasAcceptedContextualWayAsset = resolveMapStyleContextualAsset(wayRequest).hasExactAcceptedAsset;
                }
                if (cell->kind == MapTileKind::Road || hasAcceptedContextualWayAsset)
                    ++metrics.roadIntegratedGroundCells;
                if (cell->kind == MapTileKind::Bridge)
                    ++metrics.roadBridgeWayCells;
                if (!isExplicitRoadbedTerrain(cell->kind) && !hasAcceptedContextualWayAsset)
                {
                    ++metrics.roadOverlayOnlyCells;
                    ++metrics.roadOverlayTerrainCounts[static_cast<size_t>(cell->kind)];
                    addCoordinateSample(metrics.roadOverlayOnlySamples, x, y);
                }
                const MapStyleContextualAssetRequest bridgeRequest = mapStyleContextualAssetRequestForWayVisual(wayContextAt(map, x, y), options.styleId);
                if (bridgeRequest.assetClass == MapStyleContextualAssetClass::BridgeSequence)
                {
                    addCoordinateSample(metrics.bridgeSequenceSamples, x, y);
                    addUniqueText(metrics.bridgeSequenceSampleSpanPositions, mapStyleSemanticContextValue(bridgeRequest.semanticContext, "span_position"));
                }
            }

            if (cell->kind == MapTileKind::Bridge)
            {
                ++metrics.bridgeCells;
                bridgeContextSignatures.push_back(bridgeLocalContextSignature(map, x, y));
                addCoordinateSample(metrics.bridgeSamples, x, y);
            }

            if (isStructureLikeTile(cell->kind))
            {
                ++metrics.structureCells;
                ++metrics.contextlessStructureCells;
                addCoordinateSample(metrics.structureSamples, x, y);
                structureContextSignatures.push_back(structureLocalContextSignature(map, x, y));
                if (!hasRoadApproach(map, x, y))
                {
                    ++metrics.structureWithoutApproachCells;
                    addCoordinateSample(metrics.structureWithoutApproachSamples, x, y);
                }

                bool hasHostileContext = false;
                bool hasBridgeContext = false;
                bool hasWaterfrontContext = false;
                for (int dy = -1; dy <= 1; ++dy)
                {
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        if (dx == 0 && dy == 0)
                            continue;
                        const MapCellModel *neighbor = mapCellAt(map, x + dx, y + dy);
                        if (!neighbor || isStructureLikeTile(neighbor->kind))
                            continue;
                        ++structureContextCounts[static_cast<size_t>(neighbor->kind)];
                        if (neighbor->kind == MapTileKind::Bridge && (dx == 0 || dy == 0))
                            hasBridgeContext = true;
                        if (isBridgeCrossingContext(*neighbor))
                            hasWaterfrontContext = true;
                        if (isHostileStructureContext(neighbor->kind))
                            hasHostileContext = true;
                    }
                }

                if (hasHostileContext)
                {
                    ++metrics.structureHostileContextCells;
                    addCoordinateSample(metrics.structureHostileContextSamples, x, y);
                }
                if (hasBridgeContext)
                {
                    ++metrics.structureBridgeAdjacentCells;
                    addCoordinateSample(metrics.structureBridgeAdjacentSamples, x, y);
                }
                if (hasWaterfrontContext)
                {
                    ++metrics.structureWaterfrontCells;
                    addCoordinateSample(metrics.structureWaterfrontSamples, x, y);
                }
            }

            if ((cell->kind == MapTileKind::Settlement || cell->kind == MapTileKind::Building) && orthogonalNeighborCount(map, x, y, MapTileKind::Water) >= 3)
                ++metrics.waterAdjacentSettlementCells;
        }
    }

    for (int count : metrics.roadWayKindCounts)
    {
        if (count > 0)
            ++metrics.distinctRoadWayKinds;
    }
    int dominantRoadOverlayTerrainCount = 0;
    for (size_t i = 0; i < metrics.roadOverlayTerrainCounts.size(); ++i)
    {
        const int count = metrics.roadOverlayTerrainCounts[i];
        if (count <= 0)
            continue;
        ++metrics.roadOverlayTerrainKinds;
        if (count > dominantRoadOverlayTerrainCount)
        {
            dominantRoadOverlayTerrainCount = count;
            metrics.roadOverlayDominantTerrain = static_cast<MapTileKind>(i);
        }
    }
    metrics.roadOverlayDominantTerrainPercent = static_cast<int>(percentOf(dominantRoadOverlayTerrainCount, metrics.roadOverlayOnlyCells) + 0.5);
    metrics.bridgeDistinctContexts = distinctCount(bridgeContextSignatures);
    metrics.bridgeDominantContextPercent = distinctCountAndDominantPercent(bridgeContextSignatures, metrics.bridgeCells);
    metrics.structureDistinctContextSignatures = distinctCount(structureContextSignatures);
    metrics.structureDominantContextPercent = distinctCountAndDominantPercent(structureContextSignatures, metrics.structureCells);
    for (int count : structureContextCounts)
    {
        if (count > 0)
            ++metrics.structureContextKinds;
    }
    measureBridgeComponents(map, metrics);
    measureLowVarianceTileRuns(map, metrics);

    std::vector<bool> visited(map.cells.size(), false);
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const int startIndex = y * map.width + x;
            const MapCellModel *start = mapCellAt(map, x, y);
            if (!start || visited[static_cast<size_t>(startIndex)] || (start->kind != MapTileKind::Mountain && start->kind != MapTileKind::Forest))
                continue;

            const MapTileKind kind = start->kind;
            int size = 0;
            int individualCells = 0;
            int continuousCells = 0;
            int minX = x;
            int minY = y;
            int maxX = x;
            int maxY = y;
            std::vector<MapCellCoordinate> componentCells;
            std::queue<MapCellCoordinate> pending;
            pending.push({x, y});
            visited[static_cast<size_t>(startIndex)] = true;

            while (!pending.empty())
            {
                const MapCellCoordinate current = pending.front();
                pending.pop();
                const MapCellModel *cell = mapCellAt(map, current.x, current.y);
                if (!cell)
                    continue;

                ++size;
                minX = std::min(minX, current.x);
                minY = std::min(minY, current.y);
                maxX = std::max(maxX, current.x);
                maxY = std::max(maxY, current.y);
                if (cell->symbolVariantMode == MapSymbolVariantMode::Individual)
                    ++individualCells;
                if (cell->symbolVariantMode == MapSymbolVariantMode::Continuous)
                    ++continuousCells;
                componentCells.push_back(current);

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
                    if (neighborCell && neighborCell->kind == kind)
                    {
                        visited[static_cast<size_t>(neighborIndex)] = true;
                        pending.push(neighbor);
                    }
                }
            }

            if (kind == MapTileKind::Mountain)
            {
                ++metrics.mountainComponents;
                metrics.largestMountainComponent = std::max(metrics.largestMountainComponent, size);
                if (size >= 3)
                {
                    ++metrics.mountainRangeComponents;
                    metrics.mountainRangeCells += size;
                    addMountainComponentRoleSamples(map, componentCells, metrics.mountainRangeSamples, metrics.mountainRangeSampleRoles);
                    if (continuousCells != size)
                    {
                        ++metrics.mountainRangeComponentsNotContinuous;
                        metrics.mountainRangeCellsNotContinuous += size - continuousCells;
                        addCoordinateSample(metrics.mountainRangeNotContinuousSamples, x, y);
                    }
                }
                if (componentLooksSlabby(size, minX, minY, maxX, maxY))
                    ++metrics.slabbyMountainComponents;
                if (individualCells == size && size <= 2)
                    ++metrics.individualPeakComponents;
            }
            else if (kind == MapTileKind::Forest)
            {
                ++metrics.forestComponents;
                metrics.largestForestComponent = std::max(metrics.largestForestComponent, size);
            }
        }
    }

    return metrics;
}

int tacticalCount(const MapModel &map)
{
    int count = 0;
    for (const MapCellModel &cell : map.cells)
    {
        if (cell.kind == MapTileKind::Cover || cell.kind == MapTileKind::Rubble || cell.kind == MapTileKind::Hazard || cell.kind == MapTileKind::Pit || cell.kind == MapTileKind::Landmark)
            ++count;
    }
    return count;
}

MapWayRenderContext wayContextAt(const MapModel &map, int x, int y)
{
    return makeMapWayRenderContext(
        mapCellAt(map, x, y),
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

IsolatedWaySummary isolatedWayCells(const MapModel &map)
{
    constexpr int maxSamplesPerKind = 8;
    constexpr int wayKindCount = static_cast<int>(MapWayKind::CaveTunnel) + 1;
    std::array<IsolatedWayKindSummary, wayKindCount> byKind;
    for (int i = 0; i < wayKindCount; ++i)
        byKind[static_cast<size_t>(i)].kind = static_cast<MapWayKind>(i);

    IsolatedWaySummary summary;
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell || cell->wayKind == MapWayKind::None)
                continue;
            if (mapWayEightNeighborMask(wayContextAt(map, x, y)) == 0)
            {
                ++summary.total;
                IsolatedWayKindSummary &kindSummary = byKind[static_cast<size_t>(cell->wayKind)];
                ++kindSummary.count;
                if (static_cast<int>(kindSummary.samples.size()) < maxSamplesPerKind)
                    kindSummary.samples.push_back({x, y});
            }
        }
    }

    for (const IsolatedWayKindSummary &kindSummary : byKind)
    {
        if (kindSummary.count > 0)
            summary.byKind.push_back(kindSummary);
    }
    return summary;
}

std::string coordinateList(const std::vector<MapCellCoordinate> &coordinates)
{
    std::ostringstream stream;
    for (size_t i = 0; i < coordinates.size(); ++i)
    {
        if (i > 0)
            stream << ", ";
        stream << "(" << coordinates[i].x << "," << coordinates[i].y << ")";
    }
    return stream.str();
}

std::string isolatedWaySummaryMessage(const IsolatedWaySummary &summary)
{
    std::ostringstream stream;
    stream << summary.total << " way cells are isolated by kind: ";
    for (size_t i = 0; i < summary.byKind.size(); ++i)
    {
        if (i > 0)
            stream << "; ";
        const IsolatedWayKindSummary &kindSummary = summary.byKind[i];
        stream << mapWayKindLabel(kindSummary.kind) << "=" << kindSummary.count;
        if (!kindSummary.samples.empty())
        {
            stream << " at " << coordinateList(kindSummary.samples);
            if (static_cast<int>(kindSummary.samples.size()) < kindSummary.count)
                stream << " ...";
        }
    }
    return stream.str();
}

std::string coordinateSampleSuffix(const std::vector<MapCellCoordinate> &samples)
{
    if (samples.empty())
        return "";
    return " at " + coordinateList(samples);
}

std::string roadWayKindCountsMessage(const QualityMetrics &metrics)
{
    std::ostringstream stream;
    bool wroteAny = false;
    for (size_t i = 0; i < metrics.roadWayKindCounts.size(); ++i)
    {
        const MapWayKind kind = static_cast<MapWayKind>(i);
        if (!isRoadWay(kind) || metrics.roadWayKindCounts[i] == 0)
            continue;
        if (wroteAny)
            stream << ", ";
        stream << mapWayKindLabel(kind) << "=" << metrics.roadWayKindCounts[i];
        wroteAny = true;
    }
    return wroteAny ? stream.str() : "none";
}

std::string roadOverlayTerrainCountsMessage(const QualityMetrics &metrics)
{
    std::ostringstream stream;
    bool wroteAny = false;
    for (size_t i = 0; i < metrics.roadOverlayTerrainCounts.size(); ++i)
    {
        const int count = metrics.roadOverlayTerrainCounts[i];
        if (count == 0)
            continue;
        if (wroteAny)
            stream << ", ";
        stream << mapTileKindLabel(static_cast<MapTileKind>(i)) << "=" << count;
        wroteAny = true;
    }
    return wroteAny ? stream.str() : "none";
}

std::string textListMessage(const std::vector<std::string> &items)
{
    if (items.empty())
        return "none";

    std::ostringstream stream;
    for (size_t i = 0; i < items.size(); ++i)
    {
        if (i > 0)
            stream << ",";
        stream << items[i];
    }
    return stream.str();
}

std::string lowVarianceTileRunMessage(const QualityMetrics &metrics)
{
    std::ostringstream stream;
    stream << metrics.lowVarianceTileRuns << " large same-family components have low visual/context variance";
    if (!metrics.lowVarianceTileRunSamples.empty())
    {
        stream << ": ";
        for (size_t i = 0; i < metrics.lowVarianceTileRunSamples.size(); ++i)
        {
            if (i > 0)
                stream << "; ";
            const TileRunIssue &issue = metrics.lowVarianceTileRunSamples[i];
            stream << mapTileKindLabel(issue.kind)
                   << " size " << issue.size
                   << ", contexts " << issue.distinctSignatures
                   << ", dominant " << issue.dominantPercent << "%"
                   << " at (" << issue.sample.x << "," << issue.sample.y << ")";
        }
    }
    return stream.str();
}

int incompatibleWayTerrainCells(const MapModel &map)
{
    int bad = 0;
    for (const MapCellModel &cell : map.cells)
    {
        if (cell.wayKind == MapWayKind::None)
            continue;
        if (!mapGenTerrainSupportsWay(cell.kind, cell.wayKind))
            ++bad;
    }
    return bad;
}

bool hasDiagonalWay(const MapModel &map, MapWayKind kind)
{
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell || cell->wayKind != kind)
                continue;
            if ((mapWayEightNeighborMask(wayContextAt(map, x, y)) & (2 | 8 | 32 | 128)) != 0)
                return true;
        }
    }
    return false;
}

int roadLikeWayCount(const MapModel &map)
{
    int count = 0;
    for (const MapCellModel &cell : map.cells)
    {
        if (isRoadWay(cell.wayKind))
            ++count;
    }
    return count;
}

bool hasDiagonalRoadWay(const MapModel &map)
{
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell || !isRoadWay(cell->wayKind))
                continue;
            if ((mapWayEightNeighborMask(wayContextAt(map, x, y)) & (2 | 8 | 32 | 128)) != 0)
                return true;
        }
    }
    return false;
}

std::string scopeLabel(MapScope scope)
{
    switch (scope)
    {
    case MapScope::World:
        return "world";
    case MapScope::Local:
        return "local";
    case MapScope::Dungeon:
        return "dungeon";
    }
    return "unknown";
}

std::string mountainPatternLabel(MapMountainPattern pattern)
{
    switch (pattern)
    {
    case MapMountainPattern::Mixed:
        return "mixed";
    case MapMountainPattern::Ranges:
        return "ranges";
    case MapMountainPattern::IndividualPeaks:
        return "individual";
    }
    return "unknown";
}

std::string roadNetworkLabel(MapRoadNetworkMode mode)
{
    switch (mode)
    {
    case MapRoadNetworkMode::None:
        return "none";
    case MapRoadNetworkMode::Sparse:
        return "sparse";
    case MapRoadNetworkMode::Connected:
        return "connected";
    case MapRoadNetworkMode::Grid:
        return "grid";
    }
    return "unknown";
}

std::string reviewFixtureLabel(ReviewFixture fixture)
{
    switch (fixture)
    {
    case ReviewFixture::None:
        return "";
    case ReviewFixture::GrassIntegratedWays:
        return "grass_integrated_ways";
    }
    return "";
}

std::string optionsSummary(const MapGenerationOptions &options);

std::string reviewCaseOptionsSummary(const ReviewCase &reviewCase)
{
    std::string summary = optionsSummary(reviewCase.options);
    const std::string fixture = reviewFixtureLabel(reviewCase.fixture);
    if (!fixture.empty())
        summary += ", fixture=" + fixture;
    return summary;
}

const std::array<const char *, 7> &coreArtReadinessLabels()
{
    static const std::array<const char *, 7> labels = {
        "road_overlay_shortcut",
        "bridge_middle_repetition",
        "tower_cave_grass_halo",
        "range_not_multitile",
        "component_biome_drift",
        "single_road_family_collapse",
        "style_family_fallback"};
    return labels;
}

std::string baseDiagnosticContext(const MapGenerationOptions &options, const std::string &domain)
{
    std::ostringstream stream;
    stream << "style=" << options.styleId
           << "; scope=" << scopeLabel(options.scope)
           << "; domain=" << domain
           << "; seed=" << options.seed;
    return stream.str();
}

std::string roadArtDiagnosticContext(const MapGenerationOptions &options, const QualityMetrics &metrics)
{
    std::ostringstream stream;
    stream << baseDiagnosticContext(options, "way")
           << "; road_network=" << roadNetworkLabel(options.roadNetwork)
           << "; road_way_cells=" << metrics.roadWayCells
           << "; integrated_ground_cells=" << metrics.roadIntegratedGroundCells
           << "; bridge_way_cells=" << metrics.roadBridgeWayCells
           << "; overlay_only_cells=" << metrics.roadOverlayOnlyCells
           << "; road_families=" << roadWayKindCountsMessage(metrics);
    return stream.str();
}

std::string bridgeArtDiagnosticContext(const MapGenerationOptions &options, const QualityMetrics &metrics)
{
    std::ostringstream stream;
    stream << baseDiagnosticContext(options, "bridge")
           << "; bridge_cells=" << metrics.bridgeCells
           << "; bridge_components=" << metrics.bridgeComponents
           << "; local_contexts=" << metrics.bridgeDistinctContexts
           << "; component_contexts=" << metrics.bridgeComponentDistinctContexts
           << "; no_crossing_context_cells=" << metrics.bridgeWithoutCrossingContextCells
           << "; no_bank_context_cells=" << metrics.bridgeWithoutBankContextCells
           << "; sample_span_position_count=" << metrics.bridgeSequenceSampleSpanPositions.size()
           << "; sample_span_positions=" << textListMessage(metrics.bridgeSequenceSampleSpanPositions);
    return stream.str();
}

std::string structureArtDiagnosticContext(const MapGenerationOptions &options, const QualityMetrics &metrics)
{
    std::ostringstream stream;
    stream << baseDiagnosticContext(options, "structure")
           << "; structure_cells=" << metrics.structureCells
           << "; context_signatures=" << metrics.structureDistinctContextSignatures
           << "; neighboring_terrain_kinds=" << metrics.structureContextKinds
           << "; without_approach_cells=" << metrics.structureWithoutApproachCells
           << "; hostile_context_cells=" << metrics.structureHostileContextCells
           << "; bridge_adjacent_cells=" << metrics.structureBridgeAdjacentCells
           << "; waterfront_cells=" << metrics.structureWaterfrontCells;
    return stream.str();
}

std::string mountainArtDiagnosticContext(const MapGenerationOptions &options, const QualityMetrics &metrics)
{
    std::ostringstream stream;
    stream << baseDiagnosticContext(options, "mountain_component")
           << "; mountain_pattern=" << mountainPatternLabel(options.mountainPattern)
           << "; mountain_components=" << metrics.mountainComponents
           << "; range_components=" << metrics.mountainRangeComponents
           << "; range_cells=" << metrics.mountainRangeCells
           << "; largest_component=" << metrics.largestMountainComponent
           << "; contextual_shapes=" << mapTileContextualSymbolVariantCount(MapTileKind::Mountain)
           << "; sample_component_role_count=" << metrics.mountainRangeSampleRoles.size()
           << "; sample_component_roles=" << textListMessage(metrics.mountainRangeSampleRoles);
    return stream.str();
}

MapStyleContextualAssetResolution reviewerContextualCoverageResolution(
    const MapGenerationOptions &options,
    MapStyleContextualAssetClass assetClass,
    const std::string &owner,
    const std::string &semanticContext)
{
    MapStyleContextualAssetRequest request;
    request.styleId = options.styleId;
    request.assetClass = assetClass;
    request.owner = owner;
    request.semanticContext = semanticContext;
    return resolveMapStyleContextualAsset(request);
}

MapStyleContextualAssetResolution reviewerWayVisualCoverageResolution(
    const MapGenerationOptions &options,
    const MapModel &map,
    const std::vector<MapCellCoordinate> &samples,
    MapStyleContextualAssetClass fallbackAssetClass,
    const std::string &fallbackOwner,
    const std::string &fallbackSemanticContext)
{
    MapStyleContextualAssetResolution firstResolution;
    MapStyleContextualAssetResolution firstCandidateResolution;
    bool hasFirstResolution = false;
    bool hasCandidateResolution = false;
    for (const MapCellCoordinate &sample : samples)
    {
        if (!isMapCellInside(map, sample.x, sample.y))
            continue;

        MapStyleContextualAssetRequest request = mapStyleContextualAssetRequestForWayVisual(wayContextAt(map, sample.x, sample.y), options.styleId);
        if (!request.owner.empty() && request.assetClass == fallbackAssetClass)
        {
            const MapStyleContextualAssetResolution resolution = resolveMapStyleContextualAsset(request);
            if (!hasFirstResolution)
            {
                firstResolution = resolution;
                hasFirstResolution = true;
            }
            if (resolution.hasExactAcceptedAsset)
                return resolution;
            if (!hasCandidateResolution && resolution.hasCandidateAsset)
            {
                firstCandidateResolution = resolution;
                hasCandidateResolution = true;
            }
        }
    }

    if (hasCandidateResolution)
        return firstCandidateResolution;
    if (hasFirstResolution)
        return firstResolution;

    return reviewerContextualCoverageResolution(options, fallbackAssetClass, fallbackOwner, fallbackSemanticContext);
}

MapStyleContextualAssetResolution reviewerTileVisualCoverageResolution(
    const MapGenerationOptions &options,
    const MapModel &map,
    const std::vector<MapCellCoordinate> &samples,
    MapStyleContextualAssetClass fallbackAssetClass,
    const std::string &fallbackOwner,
    const std::string &fallbackSemanticContext,
    bool preferMissingContextSlot = false)
{
    MapStyleContextualAssetResolution firstResolution;
    MapStyleContextualAssetResolution firstCandidateResolution;
    MapStyleContextualAssetResolution firstMissingSlotResolution;
    bool hasFirstResolution = false;
    bool hasCandidateResolution = false;
    bool hasMissingSlotResolution = false;

    for (const MapCellCoordinate &sample : samples)
    {
        if (!isMapCellInside(map, sample.x, sample.y))
            continue;

        MapStyleContextualAssetRequest request = mapStyleContextualAssetRequestForTileVisual(tileContextAt(map, sample.x, sample.y), options.styleId);
        if (!request.owner.empty() && request.assetClass == fallbackAssetClass)
        {
            const MapStyleContextualAssetResolution resolution = resolveMapStyleContextualAsset(request);
            if (!hasFirstResolution)
            {
                firstResolution = resolution;
                hasFirstResolution = true;
            }
            if (resolution.hasExactAcceptedAsset)
                return resolution;
            if (!hasMissingSlotResolution && resolution.fallbackReason == "missing_contextual_asset_slot")
            {
                firstMissingSlotResolution = resolution;
                hasMissingSlotResolution = true;
            }
            if (!hasCandidateResolution && resolution.hasCandidateAsset)
            {
                firstCandidateResolution = resolution;
                hasCandidateResolution = true;
            }
        }
    }

    if (preferMissingContextSlot && hasMissingSlotResolution)
        return firstMissingSlotResolution;
    if (hasCandidateResolution)
        return firstCandidateResolution;
    if (hasFirstResolution)
        return firstResolution;

    return reviewerContextualCoverageResolution(options, fallbackAssetClass, fallbackOwner, fallbackSemanticContext);
}

void appendDiagnosticWorkOrder(std::string &semanticContext, const MapStyleContextualAssetResolution &resolution)
{
    if (!resolution.requestOwner.empty())
        semanticContext += "; request_owner=" + resolution.requestOwner;
    if (!resolution.requestSemanticContext.empty())
        semanticContext += "; " + resolution.requestSemanticContext;
    if (!resolution.workOrderId.empty())
        semanticContext += "; work_order=" + resolution.workOrderId;
    if (!resolution.candidatePreviewAssetId.empty())
        semanticContext += "; candidate_preview_asset=" + resolution.candidatePreviewAssetId;
}

struct ContextualDiagnosticTrace
{
    std::string semanticContext;
    MapStyleContextualAssetResolution resolution;
};

struct WayCoverageSampleGroup
{
    std::string owner;
    MapStyleContextualAssetClass assetClass = MapStyleContextualAssetClass::IntegratedWayGround;
    std::vector<MapCellCoordinate> samples;
};

std::vector<WayCoverageSampleGroup> collectWayCoverageSampleGroups(const MapModel &map, const MapGenerationOptions &options)
{
    std::vector<WayCoverageSampleGroup> groups;
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell || !isRoadWay(cell->wayKind))
                continue;

            const MapStyleContextualAssetRequest request = mapStyleContextualAssetRequestForWayVisual(wayContextAt(map, x, y), options.styleId);
            if (request.owner.empty() || request.assetClass != MapStyleContextualAssetClass::IntegratedWayGround)
                continue;

            auto existing = std::find_if(groups.begin(), groups.end(), [&](const WayCoverageSampleGroup &group) {
                return group.owner == request.owner && group.assetClass == request.assetClass;
            });
            if (existing == groups.end())
            {
                WayCoverageSampleGroup group;
                group.owner = request.owner;
                group.assetClass = request.assetClass;
                addCoordinateSample(group.samples, x, y);
                groups.push_back(std::move(group));
            }
            else
            {
                addCoordinateSample(existing->samples, x, y);
            }
        }
    }
    return groups;
}

void applyReviewFixture(MapModel &map, ReviewFixture fixture)
{
    if (fixture != ReviewFixture::GrassIntegratedWays)
        return;

    // This authored fixture exists to exercise both accepted grass-ground way
    // families. Keep it independent of whatever terrain the world generator
    // happened to place under the route, and give each family one contiguous
    // side of the sample instead of producing an alternating visual pattern.
    const int familySplitX = std::max(1, map.width / 2);
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell || !isRoadWay(cell->wayKind) || cell->kind == MapTileKind::Bridge)
                continue;
            cell->kind = MapTileKind::Empty;
            cell->movementCost = 1;
            cell->difficultTerrain = false;
            cell->blocksLineOfSight = false;
            cell->wayKind = x < familySplitX ? MapWayKind::DirtRoad : MapWayKind::Trail;
        }
    }
}

ContextualDiagnosticTrace bridgeDiagnosticTrace(
    const MapGenerationOptions &options,
    const MapModel &map,
    const QualityMetrics &metrics,
    const std::vector<MapCellCoordinate> &samples)
{
    ContextualDiagnosticTrace trace;
    trace.semanticContext = bridgeArtDiagnosticContext(options, metrics);
    trace.resolution = reviewerWayVisualCoverageResolution(
        options,
        map,
        samples,
        MapStyleContextualAssetClass::BridgeSequence,
        mapStyleBridgeSequenceOwnerKey(MapWayKind::DirtRoad, MapTileKind::Water),
        trace.semanticContext);
    appendDiagnosticWorkOrder(trace.semanticContext, trace.resolution);
    return trace;
}

ContextualDiagnosticTrace structureDiagnosticTrace(
    const MapGenerationOptions &options,
    const MapModel &map,
    const QualityMetrics &metrics,
    const std::vector<MapCellCoordinate> &samples)
{
    ContextualDiagnosticTrace trace;
    trace.semanticContext = structureArtDiagnosticContext(options, metrics);
    trace.resolution = reviewerTileVisualCoverageResolution(
        options,
        map,
        samples,
        MapStyleContextualAssetClass::StructureContextMatrix,
        mapStyleStructureContextOwnerKey(MapTileKind::Building),
        trace.semanticContext,
        true);
    appendDiagnosticWorkOrder(trace.semanticContext, trace.resolution);
    return trace;
}

void addDiagnostic(
    std::vector<Diagnostic> &diagnostics,
    std::string severity,
    std::string message,
    std::string label = {},
    std::string selectedAssetId = {},
    std::string fallbackReason = {},
    std::string semanticContext = {},
    std::vector<MapCellCoordinate> samples = {})
{
    diagnostics.push_back({
        std::move(severity),
        std::move(label),
        std::move(message),
        std::move(semanticContext),
        std::move(selectedAssetId),
        std::move(fallbackReason),
        std::move(samples)});
}

void addContextualCoverageSample(
    std::vector<ContextualCoverageSample> &samples,
    std::string domain,
    std::string semanticContext,
    const MapStyleContextualAssetResolution &resolution)
{
    appendDiagnosticWorkOrder(semanticContext, resolution);
    if (resolution.workOrderId.empty() &&
        resolution.selectedAssetId.empty() &&
        resolution.candidatePreviewAssetId.empty() &&
        resolution.qaLabel.empty())
    {
        return;
    }

    samples.push_back(ContextualCoverageSample{
        std::move(domain),
        std::move(semanticContext),
        resolution.workOrderId,
        resolution.selectedAssetId,
        resolution.candidatePreviewAssetId,
        resolution.fallbackReason,
        resolution.qaLabel,
        resolution.targetAssetCount,
        resolution.acceptedAssetCount,
        resolution.candidateAssetCount});
}

std::vector<ContextualCoverageSample> contextualCoverageSamplesForGeneratedMap(const ReviewCase &reviewCase, const MapGenerationResult &generated, const QualityMetrics &metrics)
{
    std::vector<ContextualCoverageSample> samples;
    const MapModel &map = generated.map;
    const MapGenerationOptions &options = reviewCase.options;
    const std::string roadContext = roadArtDiagnosticContext(options, metrics);
    const std::string bridgeContext = bridgeArtDiagnosticContext(options, metrics);
    const std::string structureContext = structureArtDiagnosticContext(options, metrics);
    const std::string mountainContext = mountainArtDiagnosticContext(options, metrics);

    for (const WayCoverageSampleGroup &group : collectWayCoverageSampleGroups(map, options))
    {
        std::string sampleContext = roadContext + "; sample_request_owner=" + group.owner;
        addContextualCoverageSample(
            samples,
            "way",
            sampleContext,
            reviewerWayVisualCoverageResolution(
                options,
                map,
                group.samples,
                group.assetClass,
                group.owner,
                sampleContext));
    }

    if (!metrics.bridgeSamples.empty())
    {
        addContextualCoverageSample(
            samples,
            "bridge",
            bridgeContext,
            reviewerWayVisualCoverageResolution(
                options,
                map,
                metrics.bridgeSamples,
                MapStyleContextualAssetClass::BridgeSequence,
                mapStyleBridgeSequenceOwnerKey(MapWayKind::DirtRoad, MapTileKind::Water),
                bridgeContext));
    }

    if (!metrics.structureSamples.empty())
    {
        addContextualCoverageSample(
            samples,
            "structure",
            structureContext,
            reviewerTileVisualCoverageResolution(
                options,
                map,
                metrics.structureSamples,
                MapStyleContextualAssetClass::StructureContextMatrix,
                mapStyleStructureContextOwnerKey(MapTileKind::Building),
                structureContext,
                true));
    }

    if (!metrics.mountainRangeSamples.empty())
    {
        addContextualCoverageSample(
            samples,
            "mountain_component",
            mountainContext,
            reviewerTileVisualCoverageResolution(
                options,
                map,
                metrics.mountainRangeSamples,
                MapStyleContextualAssetClass::ComponentRoleSet,
                mapStyleComponentRoleOwnerKey(MapTileKind::Mountain, MapSymbolVariantMode::Continuous),
                mountainContext));
    }

    return samples;
}

std::string diagnosticSummary(const Diagnostic &diagnostic)
{
    std::ostringstream stream;
    stream << diagnostic.severity;
    if (!diagnostic.label.empty())
        stream << " [" << diagnostic.label << "]";
    stream << ": " << diagnostic.message;
    if (!diagnostic.selectedAssetId.empty() || !diagnostic.fallbackReason.empty())
    {
        stream << " (";
        bool wrote = false;
        if (!diagnostic.selectedAssetId.empty())
        {
            stream << "selected_asset=" << diagnostic.selectedAssetId;
            wrote = true;
        }
        if (!diagnostic.fallbackReason.empty())
        {
            if (wrote)
                stream << ", ";
            stream << "fallback_reason=" << diagnostic.fallbackReason;
        }
        stream << ")";
    }
    return stream.str();
}

int diagnosticLabelCount(const std::vector<ReviewResult> &results, const std::string &label)
{
    int count = 0;
    for (const ReviewResult &result : results)
    {
        for (const Diagnostic &diagnostic : result.diagnostics)
        {
            if (diagnostic.label == label)
                ++count;
        }
    }
    return count;
}

std::pair<int, int> diagnosticSeverityCounts(const std::vector<ReviewResult> &results)
{
    int errors = 0;
    int warnings = 0;
    for (const ReviewResult &result : results)
    {
        for (const Diagnostic &diagnostic : result.diagnostics)
        {
            if (diagnostic.severity == "ERROR")
                ++errors;
            else if (diagnostic.severity == "WARN")
                ++warnings;
        }
    }
    return {errors, warnings};
}

bool reportIncludesContextFixtures(const std::vector<ReviewResult> &results)
{
    return std::any_of(results.begin(), results.end(), [](const ReviewResult &result) {
        return result.reviewCase.fixture != ReviewFixture::None;
    });
}

std::vector<Diagnostic> validateGeneratedMap(const ReviewCase &reviewCase, const MapGenerationResult &generated, const QualityMetrics &metrics, bool artReadiness)
{
    std::vector<Diagnostic> diagnostics;
    const MapModel &map = generated.map;
    const MapGenerationOptions &options = reviewCase.options;
    const int area = std::max(1, map.width * map.height);
    std::string roadContext = roadArtDiagnosticContext(options, metrics);
    std::string bridgeContext = bridgeArtDiagnosticContext(options, metrics);
    std::string structureContext = structureArtDiagnosticContext(options, metrics);
    std::string mountainContext = mountainArtDiagnosticContext(options, metrics);
    MapStyleContextualAssetResolution roadOverlayCoverage;
    MapStyleContextualAssetResolution bridgeCoverage;
    MapStyleContextualAssetResolution structureCoverage;
    MapStyleContextualAssetResolution mountainCoverage;
    if (artReadiness)
    {
        const std::vector<MapCellCoordinate> &roadCoverageSamples =
            metrics.roadOverlayOnlySamples.empty() ? metrics.roadWaySamples : metrics.roadOverlayOnlySamples;
        roadOverlayCoverage = reviewerWayVisualCoverageResolution(
            options,
            map,
            roadCoverageSamples,
            MapStyleContextualAssetClass::IntegratedWayGround,
            mapStyleIntegratedWayOwnerKey(MapWayKind::DirtRoad, MapTileKind::Empty),
            roadContext);
        bridgeCoverage = reviewerWayVisualCoverageResolution(
            options,
            map,
            metrics.bridgeSamples,
            MapStyleContextualAssetClass::BridgeSequence,
            mapStyleBridgeSequenceOwnerKey(MapWayKind::DirtRoad, MapTileKind::Water),
            bridgeContext);
        structureCoverage = reviewerTileVisualCoverageResolution(
            options,
            map,
            metrics.structureSamples,
            MapStyleContextualAssetClass::StructureContextMatrix,
            mapStyleStructureContextOwnerKey(MapTileKind::Building),
            structureContext,
            true);
        mountainCoverage = reviewerTileVisualCoverageResolution(
            options,
            map,
            metrics.mountainRangeSamples,
            MapStyleContextualAssetClass::ComponentRoleSet,
            mapStyleComponentRoleOwnerKey(MapTileKind::Mountain, MapSymbolVariantMode::Continuous),
            mountainContext);
        appendDiagnosticWorkOrder(roadContext, roadOverlayCoverage);
        appendDiagnosticWorkOrder(bridgeContext, bridgeCoverage);
        appendDiagnosticWorkOrder(structureContext, structureCoverage);
        appendDiagnosticWorkOrder(mountainContext, mountainCoverage);
    }

    if (map.width != std::clamp(options.width, 4, 120) || map.height != std::clamp(options.height, 4, 120))
        addDiagnostic(diagnostics, "ERROR", "Generated dimensions do not match clamped options");

    if (!mountainComponentsHaveSingleVariantMode(map))
        addDiagnostic(
            diagnostics,
            "ERROR",
            "A connected mountain component mixes Continuous and Individual variant modes",
            "component_biome_drift",
            mountainCoverage.selectedAssetId,
            "component_variant_mode_or_modifier_drift",
            mountainContext);

    const int badWayTerrain = incompatibleWayTerrainCells(map);
    if (badWayTerrain > 0)
        addDiagnostic(diagnostics, "ERROR", std::to_string(badWayTerrain) + " way cells sit on incompatible terrain");

    const IsolatedWaySummary isolatedWays = isolatedWayCells(map);
    if (isolatedWays.total > 0)
        addDiagnostic(diagnostics, "WARN", isolatedWaySummaryMessage(isolatedWays));

    if (options.scope != MapScope::Dungeon && metrics.roadCells >= 8)
    {
        const double borderPercent = percentOf(metrics.roadBorderCells, metrics.roadCells);
        const double axisOnlyPercent = percentOf(metrics.roadAxisOnlyCells, metrics.roadCells);
        if (options.roadNetwork != MapRoadNetworkMode::Grid && borderPercent >= 35.0)
            addDiagnostic(diagnostics, "WARN", "Road network is border-biased (" + std::to_string(static_cast<int>(borderPercent + 0.5)) + "% of road cells touch the map edge)");
        if (options.roadNetwork == MapRoadNetworkMode::Connected && axisOnlyPercent >= 85.0)
            addDiagnostic(diagnostics, "WARN", "Connected road network is strongly axis-biased (" + std::to_string(static_cast<int>(axisOnlyPercent + 0.5)) + "% of road cells lack diagonal neighbors)");
        if (artReadiness)
        {
            const int roadGroundWayCells = std::max(0, metrics.roadWayCells - metrics.roadBridgeWayCells);
            if (metrics.roadOverlayOnlyCells > 0)
                addDiagnostic(
                    diagnostics,
                    "WARN",
                    std::to_string(metrics.roadOverlayOnlyCells) + " road-like way cells render as overlays on base terrain instead of integrated road-on-ground assets (" + roadOverlayTerrainCountsMessage(metrics) + ")" + coordinateSampleSuffix(metrics.roadOverlayOnlySamples),
                    roadOverlayCoverage.qaLabel,
                    roadOverlayCoverage.selectedAssetId,
                    roadOverlayCoverage.fallbackReason,
                    roadContext,
                    metrics.roadOverlayOnlySamples);
            if (roadGroundWayCells >= 8 && metrics.roadIntegratedGroundCells == 0)
                addDiagnostic(
                    diagnostics,
                    "WARN",
                    "Road network has no integrated ground-road terrain cells; all " + std::to_string(roadGroundWayCells) + " non-bridge road way cells depend on overlay strokes",
                    roadOverlayCoverage.qaLabel,
                    roadOverlayCoverage.selectedAssetId,
                    roadOverlayCoverage.fallbackReason,
                    roadContext,
                    metrics.roadOverlayOnlySamples);
            else if (roadGroundWayCells >= 12 && percentOf(metrics.roadOverlayOnlyCells, roadGroundWayCells) >= 70.0)
                addDiagnostic(
                    diagnostics,
                    "WARN",
                    "Road network is overlay-dominant (" + std::to_string(static_cast<int>(percentOf(metrics.roadOverlayOnlyCells, roadGroundWayCells) + 0.5)) + "% of non-bridge road way cells lack integrated ground-road art)",
                    roadOverlayCoverage.qaLabel,
                    roadOverlayCoverage.selectedAssetId,
                    roadOverlayCoverage.fallbackReason,
                    roadContext,
                    metrics.roadOverlayOnlySamples);
            if (metrics.roadOverlayOnlyCells >= 12 && metrics.roadOverlayDominantTerrainPercent >= 75)
                addDiagnostic(
                    diagnostics,
                    "WARN",
                    "Overlay roads mostly sit on " + std::string(mapTileKindLabel(metrics.roadOverlayDominantTerrain)) + " terrain (" + std::to_string(metrics.roadOverlayDominantTerrainPercent) + "%), which can make stamped roads look detached from local ground context",
                    roadOverlayCoverage.qaLabel,
                    roadOverlayCoverage.selectedAssetId,
                    roadOverlayCoverage.fallbackReason,
                    roadContext,
                    metrics.roadOverlayOnlySamples);
        }
    }

    if (metrics.roadWaterCrossings > 0)
        addDiagnostic(diagnostics, "WARN", std::to_string(metrics.roadWaterCrossings) + " road cells cross water without bridge/ford terrain semantics");

    if (metrics.gridWaterRoadCells > 0)
        addDiagnostic(diagnostics, "WARN", "Grid road network places " + std::to_string(metrics.gridWaterRoadCells) + " street cells over water");

    if (metrics.waterAdjacentSettlementCells > 0)
        addDiagnostic(diagnostics, "WARN", std::to_string(metrics.waterAdjacentSettlementCells) + " settlement/building cells are surrounded by water on three or more sides");

    if (artReadiness)
    {
        if (metrics.bridgeCells > 0)
            addDiagnostic(
                diagnostics,
                "WARN",
                "Bridge cells use a single bridge terrain shortcut; production art needs approach, threshold, middle, exit, and bank-context asset IDs",
                bridgeCoverage.qaLabel,
                bridgeCoverage.selectedAssetId,
                bridgeCoverage.fallbackReason,
                bridgeContext,
                metrics.bridgeSamples);
        if (metrics.bridgeCells >= 4 && (metrics.bridgeDistinctContexts <= 2 || metrics.bridgeDominantContextPercent >= 75))
            addDiagnostic(
                diagnostics,
                "WARN",
                "Bridge tiles have weak endpoint/approach variety (" + std::to_string(metrics.bridgeCells) + " bridge cells, " + std::to_string(metrics.bridgeDistinctContexts) + " local contexts, " + std::to_string(metrics.bridgeDominantContextPercent) + "% dominant context)",
                bridgeCoverage.qaLabel,
                bridgeCoverage.selectedAssetId,
                bridgeCoverage.fallbackReason,
                bridgeContext,
                metrics.bridgeSamples);
        const bool lowBridgeSequenceRoleExercise = metrics.bridgeSequenceSampleSpanPositions.size() < 4;
        if (metrics.bridgeComponents >= 3 && lowBridgeSequenceRoleExercise && (metrics.bridgeComponentDistinctContexts <= 1 || metrics.bridgeComponentDominantContextPercent >= 80))
            addDiagnostic(
                diagnostics,
                "WARN",
                "Bridge components repeat the same shape/context without enough sequence-role exercise (" + std::to_string(metrics.bridgeComponents) + " components, " + std::to_string(metrics.bridgeComponentDistinctContexts) + " component contexts, " + std::to_string(metrics.bridgeComponentDominantContextPercent) + "% dominant, " + std::to_string(metrics.bridgeSequenceSampleSpanPositions.size()) + " sampled span roles)",
                bridgeCoverage.qaLabel,
                bridgeCoverage.selectedAssetId,
                "repeated_bridge_component_context",
                bridgeContext,
                metrics.bridgeSamples);
        if (metrics.bridgeApproachlessCells > 0)
        {
            const ContextualDiagnosticTrace bridgeTrace = bridgeDiagnosticTrace(options, map, metrics, metrics.bridgeApproachlessSamples);
            addDiagnostic(
                diagnostics,
                "WARN",
                std::to_string(metrics.bridgeApproachlessCells) + " bridge cells belong to components with no explicit roadbed/approach endpoint" + coordinateSampleSuffix(metrics.bridgeApproachlessSamples),
                bridgeTrace.resolution.qaLabel,
                bridgeTrace.resolution.selectedAssetId,
                "missing_bridge_approach_asset",
                bridgeTrace.semanticContext,
                metrics.bridgeApproachlessSamples);
        }
        if (metrics.bridgeOneSidedApproachCells > 0)
        {
            const ContextualDiagnosticTrace bridgeTrace = bridgeDiagnosticTrace(options, map, metrics, metrics.bridgeOneSidedApproachSamples);
            addDiagnostic(
                diagnostics,
                "WARN",
                std::to_string(metrics.bridgeOneSidedApproachCells) + " bridge cells belong to components with only one approach endpoint" + coordinateSampleSuffix(metrics.bridgeOneSidedApproachSamples),
                bridgeTrace.resolution.qaLabel,
                bridgeTrace.resolution.selectedAssetId,
                "missing_bridge_exit_approach_asset",
                bridgeTrace.semanticContext,
                metrics.bridgeOneSidedApproachSamples);
        }
        if (metrics.bridgeWithoutCrossingContextCells > 0)
        {
            const ContextualDiagnosticTrace bridgeTrace = bridgeDiagnosticTrace(options, map, metrics, metrics.bridgeWithoutCrossingContextSamples);
            addDiagnostic(
                diagnostics,
                "WARN",
                std::to_string(metrics.bridgeWithoutCrossingContextCells) + " bridge cells have no adjacent water/chasm crossing context for bank integration" + coordinateSampleSuffix(metrics.bridgeWithoutCrossingContextSamples),
                bridgeTrace.resolution.qaLabel,
                bridgeTrace.resolution.selectedAssetId,
                "missing_bridge_crossing_context",
                bridgeTrace.semanticContext,
                metrics.bridgeWithoutCrossingContextSamples);
        }
        if (metrics.bridgeWithoutBankContextCells > 0)
        {
            const ContextualDiagnosticTrace bridgeTrace = bridgeDiagnosticTrace(options, map, metrics, metrics.bridgeWithoutBankContextSamples);
            addDiagnostic(
                diagnostics,
                "WARN",
                std::to_string(metrics.bridgeWithoutBankContextCells) + " bridge cells have no adjacent dry bank/landing context" + coordinateSampleSuffix(metrics.bridgeWithoutBankContextSamples),
                bridgeTrace.resolution.qaLabel,
                bridgeTrace.resolution.selectedAssetId,
                "missing_bridge_bank_context",
                bridgeTrace.semanticContext,
                metrics.bridgeWithoutBankContextSamples);
        }

        if (metrics.contextlessStructureCells >= 2)
            addDiagnostic(
                diagnostics,
                "WARN",
                std::to_string(metrics.contextlessStructureCells) + " structure-like tiles are standalone terrain with no ground-context/footprint semantics across " + std::to_string(metrics.structureContextKinds) + " neighboring terrain families and " + std::to_string(metrics.structureDistinctContextSignatures) + " local contexts",
                structureCoverage.qaLabel,
                structureCoverage.selectedAssetId,
                structureCoverage.fallbackReason,
                structureContext,
                metrics.structureSamples);
        if (metrics.structureWithoutApproachCells > 0 && options.routeDensity > 0)
        {
            const ContextualDiagnosticTrace structureTrace = structureDiagnosticTrace(options, map, metrics, metrics.structureWithoutApproachSamples);
            addDiagnostic(
                diagnostics,
                "WARN",
                std::to_string(metrics.structureWithoutApproachCells) + " structure-like tiles lack adjacent road/bridge entrance semantics" + coordinateSampleSuffix(metrics.structureWithoutApproachSamples),
                structureTrace.resolution.qaLabel,
                structureTrace.resolution.selectedAssetId,
                "missing_structure_entrance_context_asset",
                structureTrace.semanticContext,
                metrics.structureWithoutApproachSamples);
        }
        if (metrics.structureBridgeAdjacentCells > 0)
        {
            const ContextualDiagnosticTrace structureTrace = structureDiagnosticTrace(options, map, metrics, metrics.structureBridgeAdjacentSamples);
            addDiagnostic(
                diagnostics,
                "WARN",
                std::to_string(metrics.structureBridgeAdjacentCells) + " structure-like tiles touch bridges but have no bridge/threshold entrance variant semantics" + coordinateSampleSuffix(metrics.structureBridgeAdjacentSamples),
                structureTrace.resolution.qaLabel,
                structureTrace.resolution.selectedAssetId,
                "missing_bridgehead_structure_entrance_asset",
                structureTrace.semanticContext,
                metrics.structureBridgeAdjacentSamples);
        }
        if (metrics.structureWaterfrontCells > 0)
        {
            const ContextualDiagnosticTrace structureTrace = structureDiagnosticTrace(options, map, metrics, metrics.structureWaterfrontSamples);
            addDiagnostic(
                diagnostics,
                "WARN",
                std::to_string(metrics.structureWaterfrontCells) + " structure-like tiles touch waterfront/crossing contexts without matching shoreline or pier ground variants" + coordinateSampleSuffix(metrics.structureWaterfrontSamples),
                structureTrace.resolution.qaLabel,
                structureTrace.resolution.selectedAssetId,
                "missing_waterfront_structure_ground_asset",
                structureTrace.semanticContext,
                metrics.structureWaterfrontSamples);
        }
        if (metrics.structureHostileContextCells > 0)
        {
            const ContextualDiagnosticTrace structureTrace = structureDiagnosticTrace(options, map, metrics, metrics.structureHostileContextSamples);
            addDiagnostic(
                diagnostics,
                "WARN",
                std::to_string(metrics.structureHostileContextCells) + " structure-like tiles touch cave/water/chasm/room-style contexts without explicit matching ground art" + coordinateSampleSuffix(metrics.structureHostileContextSamples),
                structureTrace.resolution.qaLabel,
                structureTrace.resolution.selectedAssetId,
                "wrong_structure_ground_context",
                structureTrace.semanticContext,
                metrics.structureHostileContextSamples);
        }

        if ((options.mountainPattern == MapMountainPattern::Ranges || options.mountainPattern == MapMountainPattern::Mixed) && metrics.mountainRangeComponents > 0)
        {
            if (metrics.mountainRangeCellsNotContinuous > 0)
                addDiagnostic(
                    diagnostics,
                    "WARN",
                    std::to_string(metrics.mountainRangeCellsNotContinuous) + " cells in connected mountain range components are not marked Continuous for range/component art" + coordinateSampleSuffix(metrics.mountainRangeNotContinuousSamples),
                    mountainCoverage.qaLabel,
                    mountainCoverage.selectedAssetId,
                    mountainCoverage.fallbackReason,
                    mountainContext,
                    metrics.mountainRangeNotContinuousSamples);
            if (mapTileContextualSymbolVariantCount(MapTileKind::Mountain) <= 2 && metrics.largestMountainComponent >= 4)
                addDiagnostic(
                    diagnostics,
                    "WARN",
                    "Connected mountain ranges resolve to only " + std::to_string(mapTileContextualSymbolVariantCount(MapTileKind::Mountain)) + " contextual mountain shapes (" + std::to_string(metrics.mountainRangeCells) + " cells in " + std::to_string(metrics.mountainRangeComponents) + " range components, largest " + std::to_string(metrics.largestMountainComponent) + "); production art needs multi-tile range components such as ends, bodies, shoulders, corners, and passes",
                    mountainCoverage.qaLabel,
                    mountainCoverage.selectedAssetId,
                    mountainCoverage.fallbackReason,
                    mountainContext,
                    metrics.mountainRangeSamples);
        }

        if (metrics.lowVarianceTileRuns > 0)
            addDiagnostic(
                diagnostics,
                "WARN",
                lowVarianceTileRunMessage(metrics),
                "style_family_fallback",
                "runtime_v1:same_family_variant_pool",
                "low_contextual_variant_coverage",
                baseDiagnosticContext(options, "same_family_variant"));
    }

    if (metrics.slabbyMountainComponents > 0)
        addDiagnostic(
            diagnostics,
            "WARN",
            std::to_string(metrics.slabbyMountainComponents) + " mountain components have slab-like filled bounding boxes",
            mountainCoverage.qaLabel,
            mountainCoverage.selectedAssetId,
            "slab_like_component_shape",
            mountainContext,
            metrics.mountainRangeSamples);

    if (options.mountainPattern == MapMountainPattern::IndividualPeaks && options.mountainDensity >= 40)
    {
        const int expectedPeakFloor = std::max(3, area * mapGenClampPercent(options.mountainDensity) / 3500);
        if (metrics.individualPeakComponents < expectedPeakFloor)
            addDiagnostic(diagnostics, "WARN", "Individual peak generation is sparse (" + std::to_string(metrics.individualPeakComponents) + " small peak components, expected at least " + std::to_string(expectedPeakFloor) + ")");
    }

    if (options.scope != MapScope::Dungeon && options.forestDensity >= 55 && metrics.forestCells > 0)
    {
        const double forestCoverage = percentOf(metrics.forestCells, area);
        const double largestForestShare = percentOf(metrics.largestForestComponent, metrics.forestCells);
        if (forestCoverage >= 48.0 || (metrics.largestForestComponent >= 120 && largestForestShare >= 70.0))
            addDiagnostic(diagnostics, "WARN", "Forest coverage may be over-dense (" + std::to_string(static_cast<int>(forestCoverage + 0.5)) + "% coverage, largest component " + std::to_string(static_cast<int>(largestForestShare + 0.5)) + "% of forest)");
    }

    if (options.scope != MapScope::Dungeon && options.waterLevel >= 14 && wayCount(map, MapWayKind::River) == 0)
        addDiagnostic(diagnostics, "ERROR", "Water level should create a river way");

    if (options.scope != MapScope::Dungeon && options.roadNetwork != MapRoadNetworkMode::None && options.routeDensity > 0)
    {
        if (roadLikeWayCount(map) == 0)
            addDiagnostic(diagnostics, "ERROR", "Road network requested but no road-like way cells were generated");
        if (map.routes.empty())
            addDiagnostic(diagnostics, "ERROR", "Road network requested but no route metadata was generated");
        if (options.roadNetwork == MapRoadNetworkMode::Connected && !hasDiagonalRoadWay(map))
            addDiagnostic(diagnostics, "WARN", "Connected road network has no diagonal way segments");
    }

    if (options.scope == MapScope::Dungeon && options.caveNetwork)
    {
        if (terrainCount(map, MapTileKind::Cave) == 0)
            addDiagnostic(diagnostics, "ERROR", "Cave preset did not create cave terrain");
        if (wayCount(map, MapWayKind::CaveTunnel) == 0)
            addDiagnostic(diagnostics, "ERROR", "Cave preset did not create cave-tunnel ways");
        if (wayCount(map, MapWayKind::DungeonCorridor) > 0)
            addDiagnostic(diagnostics, "ERROR", "Cave preset generated dungeon-corridor ways");
    }

    if (options.scope == MapScope::Dungeon && !options.caveNetwork)
    {
        if (terrainCount(map, MapTileKind::Room) == 0)
            addDiagnostic(diagnostics, "ERROR", "Dungeon preset did not create room terrain");
        if (wayCount(map, MapWayKind::DungeonCorridor) == 0)
            addDiagnostic(diagnostics, "ERROR", "Dungeon preset did not create corridor ways");
        if (wayCount(map, MapWayKind::CaveTunnel) > 0)
            addDiagnostic(diagnostics, "ERROR", "Dungeon preset generated cave-tunnel ways");
    }

    if (options.tacticalDensity > 0 && tacticalCount(map) == 0)
        addDiagnostic(diagnostics, "WARN", "Tactical density was requested but no tactical feature tiles were generated");

    if (options.settlementDensity >= 25 && options.scope != MapScope::Dungeon && terrainCount(map, MapTileKind::Settlement) == 0)
        addDiagnostic(diagnostics, "WARN", "High settlement density generated no settlement");

    for (const std::string &warning : generated.warnings)
        addDiagnostic(
            diagnostics,
            "WARN",
            warning,
            "style_family_fallback",
            std::string("style_registry:") + options.styleId,
            "missing_style_tile_kind_coverage",
            baseDiagnosticContext(options, "style_coverage"));

    return diagnostics;
}

std::vector<ReviewCase> makeReviewCases(int cycles, int startCycle, bool includeContextFixtures)
{
    std::vector<ReviewCase> cases;
    for (int cycleOffset = 0; cycleOffset < cycles; ++cycleOffset)
    {
        const int cycle = startCycle + cycleOffset;
        const std::uint32_t seed = static_cast<std::uint32_t>(90000 + cycle * 9973);

        MapGenerationOptions balanced;
        balanced.scope = cycle % 3 == 0 ? MapScope::World : MapScope::Local;
        balanced.width = 28 + (cycle % 4) * 4;
        balanced.height = 20 + (cycle % 3) * 3;
        balanced.seed = seed + 1;
        balanced.waterLevel = 18 + (cycle * 7) % 45;
        balanced.mountainDensity = 12 + (cycle * 11) % 45;
        balanced.forestDensity = 18 + (cycle * 13) % 55;
        balanced.settlementDensity = 6 + (cycle * 5) % 28;
        balanced.routeDensity = 20 + (cycle * 9) % 70;
        balanced.tacticalDensity = 8 + (cycle * 7) % 55;
        balanced.mountainPattern = MapMountainPattern::Mixed;
        balanced.roadNetwork = MapRoadNetworkMode::Connected;
        cases.push_back({"balanced_" + std::to_string(cycle), "mixed wilderness/local map with connected travel", balanced});

        MapGenerationOptions ranges = balanced;
        ranges.seed = seed + 2;
        ranges.width = 36;
        ranges.height = 24;
        ranges.waterLevel = cycle % 2 == 0 ? 0 : 18;
        ranges.mountainDensity = 62 + (cycle % 3) * 10;
        ranges.forestDensity = 10 + (cycle * 3) % 20;
        ranges.settlementDensity = 2;
        ranges.routeDensity = 28 + (cycle * 5) % 45;
        ranges.mountainPattern = MapMountainPattern::Ranges;
        ranges.roadNetwork = MapRoadNetworkMode::Sparse;
        cases.push_back({"mountain_ranges_" + std::to_string(cycle), "continuous mountain range stress", ranges});

        MapGenerationOptions peaks = balanced;
        peaks.seed = seed + 3;
        peaks.width = 32;
        peaks.height = 22;
        peaks.waterLevel = 0;
        peaks.mountainDensity = 55;
        peaks.forestDensity = 20;
        peaks.settlementDensity = 4;
        peaks.routeDensity = 0;
        peaks.mountainPattern = MapMountainPattern::IndividualPeaks;
        peaks.roadNetwork = MapRoadNetworkMode::None;
        cases.push_back({"individual_peaks_" + std::to_string(cycle), "separated mountain peak stress", peaks});

        MapGenerationOptions city = balanced;
        city.seed = seed + 4;
        city.scope = MapScope::Local;
        city.width = 34 + (cycle % 2) * 6;
        city.height = 24;
        city.waterLevel = cycle % 2 == 0 ? 0 : 28;
        city.mountainDensity = 0;
        city.forestDensity = 8;
        city.settlementDensity = 50 + (cycle * 7) % 35;
        city.routeDensity = 70;
        city.tacticalDensity = 15 + (cycle * 11) % 45;
        city.roadNetwork = MapRoadNetworkMode::Grid;
        cases.push_back({"city_grid_" + std::to_string(cycle), "grid road and settlement stress", city});

        MapGenerationOptions forest = balanced;
        forest.seed = seed + 5;
        forest.scope = MapScope::Local;
        forest.width = 34;
        forest.height = 26;
        forest.waterLevel = 35 + (cycle * 3) % 35;
        forest.mountainDensity = 8;
        forest.forestDensity = 55 + (cycle * 5) % 35;
        forest.settlementDensity = 5;
        forest.routeDensity = 35;
        forest.tacticalDensity = 35;
        forest.roadNetwork = MapRoadNetworkMode::Connected;
        cases.push_back({"forest_river_" + std::to_string(cycle), "river/forest/route interaction stress", forest});

        MapGenerationOptions dungeon;
        dungeon.scope = MapScope::Dungeon;
        dungeon.width = 30 + (cycle % 3) * 4;
        dungeon.height = 22 + (cycle % 2) * 4;
        dungeon.seed = seed + 6;
        dungeon.tacticalDensity = 20 + (cycle * 13) % 65;
        cases.push_back({"dungeon_rooms_" + std::to_string(cycle), "room/corridor dungeon stress", dungeon});

        MapGenerationOptions cave = dungeon;
        cave.seed = seed + 7;
        cave.width = 32 + (cycle % 3) * 4;
        cave.height = 24 + (cycle % 2) * 3;
        cave.caveNetwork = true;
        cave.tacticalDensity = 25 + (cycle * 9) % 60;
        cases.push_back({"cave_network_" + std::to_string(cycle), "natural cave/tunnel stress", cave});

        MapGenerationOptions dry = balanced;
        dry.seed = seed + 8;
        dry.scope = MapScope::World;
        dry.width = 42;
        dry.height = 28;
        dry.waterLevel = 0;
        dry.mountainDensity = 25 + (cycle * 6) % 45;
        dry.forestDensity = 0;
        dry.settlementDensity = 20;
        dry.routeDensity = 45;
        dry.tacticalDensity = 10;
        dry.roadNetwork = cycle % 2 == 0 ? MapRoadNetworkMode::Sparse : MapRoadNetworkMode::Connected;
        cases.push_back({"dry_routes_" + std::to_string(cycle), "dry travel map without river assumptions", dry});

        if (includeContextFixtures)
        {
            MapGenerationOptions grassWays = dry;
            grassWays.seed = seed + 9;
            grassWays.roadNetwork = MapRoadNetworkMode::Sparse;
            grassWays.routeDensity = 45;
            grassWays.settlementDensity = 6;
            cases.push_back({
                "grass_way_fixture_" + std::to_string(cycle),
                "review-only authored grass-ground road/trail fixture",
                grassWays,
                ReviewFixture::GrassIntegratedWays});
        }
    }
    return cases;
}

void drawTinyDigit(Image &image, int digit, int x, int y, Color color)
{
    static const int patterns[10][15] = {
        {1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1},
        {0, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 1},
        {1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1},
        {1, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 1},
        {1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1, 0, 0, 1},
        {1, 1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 1},
        {1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1},
        {1, 1, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0},
        {1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1},
        {1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1}};
    for (int py = 0; py < 5; ++py)
    {
        for (int px = 0; px < 3; ++px)
        {
            if (patterns[digit][py * 3 + px])
                fillRect(image, x + px * 2, y + py * 2, 2, 2, color);
        }
    }
}

void drawTinyNumber(Image &image, int value, int x, int y, Color color)
{
    const std::string text = std::to_string(value);
    for (size_t i = 0; i < text.size(); ++i)
        drawTinyDigit(image, text[i] - '0', x + static_cast<int>(i) * 8, y, color);
}

Image renderContactSheet(const std::vector<ReviewResult> &results)
{
    constexpr int cellSize = 5;
    constexpr int panelWidth = 240;
    constexpr int panelHeight = 170;
    constexpr int columns = 4;
    const int rows = std::max(1, (static_cast<int>(results.size()) + columns - 1) / columns);
    Image image(columns * panelWidth, rows * panelHeight, {22, 25, 28});

    for (int index = 0; index < static_cast<int>(results.size()); ++index)
    {
        const ReviewResult &result = results[static_cast<size_t>(index)];
        const MapModel &map = result.generated.map;
        const int column = index % columns;
        const int row = index / columns;
        const int panelX = column * panelWidth;
        const int panelY = row * panelHeight;
        const bool hasError = std::any_of(result.diagnostics.begin(), result.diagnostics.end(), [](const Diagnostic &diagnostic) { return diagnostic.severity == "ERROR"; });
        const bool hasWarning = std::any_of(result.diagnostics.begin(), result.diagnostics.end(), [](const Diagnostic &diagnostic) { return diagnostic.severity == "WARN"; });
        const Color border = hasError ? Color{222, 70, 68} : hasWarning ? Color{232, 190, 72}
                                                                         : Color{92, 172, 116};
        drawRect(image, panelX + 3, panelY + 3, panelWidth - 6, panelHeight - 6, border);
        drawTinyNumber(image, index + 1, panelX + 10, panelY + 9, {238, 242, 245});

        const int mapPixelWidth = map.width * cellSize;
        const int mapPixelHeight = map.height * cellSize;
        const int originX = panelX + (panelWidth - mapPixelWidth) / 2;
        const int originY = panelY + 28 + std::max(0, (panelHeight - 36 - mapPixelHeight) / 2);
        for (int y = 0; y < map.height; ++y)
        {
            for (int x = 0; x < map.width; ++x)
            {
                const MapCellModel *cell = mapCellAt(map, x, y);
                fillRect(image, originX + x * cellSize, originY + y * cellSize, cellSize, cellSize, terrainColor(cell ? cell->kind : MapTileKind::Empty));
            }
        }

        for (int y = 0; y < map.height; ++y)
        {
            for (int x = 0; x < map.width; ++x)
            {
                const MapCellModel *cell = mapCellAt(map, x, y);
                if (!cell || cell->wayKind == MapWayKind::None)
                    continue;
                const int mask = mapWayEightNeighborMask(wayContextAt(map, x, y));
                const int centerX = originX + x * cellSize + cellSize / 2;
                const int centerY = originY + y * cellSize + cellSize / 2;
                const Color color = wayColor(cell->wayKind);
                const std::array<std::pair<int, std::pair<int, int>>, 8> directions = {
                    std::make_pair(1, std::make_pair(0, -cellSize / 2)),
                    std::make_pair(2, std::make_pair(cellSize / 2, -cellSize / 2)),
                    std::make_pair(4, std::make_pair(cellSize / 2, 0)),
                    std::make_pair(8, std::make_pair(cellSize / 2, cellSize / 2)),
                    std::make_pair(16, std::make_pair(0, cellSize / 2)),
                    std::make_pair(32, std::make_pair(-cellSize / 2, cellSize / 2)),
                    std::make_pair(64, std::make_pair(-cellSize / 2, 0)),
                    std::make_pair(128, std::make_pair(-cellSize / 2, -cellSize / 2))};
                for (const auto &[bit, offset] : directions)
                {
                    if ((mask & bit) != 0)
                        drawLine(image, centerX, centerY, centerX + offset.first, centerY + offset.second, color, 2);
                }
                fillRect(image, centerX - 1, centerY - 1, 3, 3, color);
            }
        }
    }
    return image;
}

std::string optionsSummary(const MapGenerationOptions &options)
{
    std::ostringstream stream;
    stream << scopeLabel(options.scope)
           << " " << options.width << "x" << options.height
           << " seed=" << options.seed
           << " water=" << options.waterLevel
           << " mtn=" << options.mountainDensity << "/" << mountainPatternLabel(options.mountainPattern)
           << " forest=" << options.forestDensity
           << " settle=" << options.settlementDensity
           << " route=" << options.routeDensity << "/" << roadNetworkLabel(options.roadNetwork)
           << " tact=" << options.tacticalDensity;
    if (options.caveNetwork)
        stream << " cave";
    return stream.str();
}

std::string roundedPercent(int value, int total)
{
    return std::to_string(static_cast<int>(percentOf(value, total) + 0.5)) + "%";
}

std::string metricsSummary(const ReviewResult &result)
{
    const QualityMetrics &metrics = result.metrics;
    const MapModel &map = result.generated.map;
    const int area = std::max(1, map.width * map.height);
    std::ostringstream stream;
    stream << "road edge " << roundedPercent(metrics.roadBorderCells, metrics.roadCells)
           << ", axis " << roundedPercent(metrics.roadAxisOnlyCells, metrics.roadCells)
           << ", road-water " << metrics.roadWaterCrossings
           << ", grid-water " << metrics.gridWaterRoadCells
           << ", road int " << metrics.roadIntegratedGroundCells << "/" << metrics.roadWayCells
           << ", overlay-road " << metrics.roadOverlayOnlyCells
           << ", road types " << metrics.distinctRoadWayKinds
           << ", bridges " << metrics.bridgeCells << "/" << metrics.bridgeDistinctContexts
           << ", bridge comps " << metrics.bridgeComponents << "/" << metrics.bridgeComponentDistinctContexts
           << ", no-bank " << metrics.bridgeWithoutBankContextCells
           << ", structures " << metrics.structureCells
           << ", struct ctx " << metrics.structureDistinctContextSignatures
           << ", no-entry " << metrics.structureWithoutApproachCells
           << ", low-var runs " << metrics.lowVarianceTileRuns
           << ", city-water-near " << metrics.waterAdjacentSettlementCells
           << ", mtn comps " << metrics.mountainComponents
           << ", mtn ranges " << metrics.mountainRangeComponents
           << ", slabby " << metrics.slabbyMountainComponents
           << ", peak comps " << metrics.individualPeakComponents
           << ", forest " << roundedPercent(metrics.forestCells, area)
           << ", forest max " << roundedPercent(metrics.largestForestComponent, metrics.forestCells);
    return stream.str();
}

std::string jsonEscape(const std::string &value)
{
    std::ostringstream stream;
    for (unsigned char ch : value)
    {
        switch (ch)
        {
        case '"':
            stream << "\\\"";
            break;
        case '\\':
            stream << "\\\\";
            break;
        case '\b':
            stream << "\\b";
            break;
        case '\f':
            stream << "\\f";
            break;
        case '\n':
            stream << "\\n";
            break;
        case '\r':
            stream << "\\r";
            break;
        case '\t':
            stream << "\\t";
            break;
        default:
            if (ch < 0x20)
            {
                stream << "\\u"
                       << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(ch)
                       << std::dec << std::setfill(' ');
            }
            else
            {
                stream << static_cast<char>(ch);
            }
            break;
        }
    }
    return stream.str();
}

void writeJsonString(std::ostream &out, const std::string &value)
{
    out << '"' << jsonEscape(value) << '"';
}

void writeCoordinateArrayJson(std::ostream &out, const std::vector<MapCellCoordinate> &coordinates)
{
    out << "[";
    for (size_t i = 0; i < coordinates.size(); ++i)
    {
        if (i > 0)
            out << ",";
        out << "{\"x\":" << coordinates[i].x << ",\"y\":" << coordinates[i].y << "}";
    }
    out << "]";
}

void writeGenerationStageTimingsJson(std::ostream &out, const std::vector<GenerationStageTiming> &timings)
{
    out << "[";
    for (size_t i = 0; i < timings.size(); ++i)
    {
        const GenerationStageTiming &timing = timings[i];
        if (i > 0)
            out << ",";
        out << "\n        {";
        out << "\"stage\": ";
        writeJsonString(out, timing.stage);
        out << ", \"step\": " << timing.step;
        out << ", \"total_steps\": " << timing.totalSteps;
        out << ", \"elapsed_ms\": " << timing.elapsedMs;
        out << "}";
    }
    if (!timings.empty())
        out << "\n      ";
    out << "]";
}

void writeContextualCoverageSamplesJson(std::ostream &out, const std::vector<ContextualCoverageSample> &samples)
{
    out << "[";
    for (size_t i = 0; i < samples.size(); ++i)
    {
        const ContextualCoverageSample &sample = samples[i];
        if (i > 0)
            out << ",";
        out << "\n        {";
        out << "\"domain\": ";
        writeJsonString(out, sample.domain);
        out << ", \"semantic_context\": ";
        writeJsonString(out, sample.semanticContext);
        out << ", \"work_order_id\": ";
        writeJsonString(out, sample.workOrderId);
        out << ", \"selected_asset_id\": ";
        writeJsonString(out, sample.selectedAssetId);
        out << ", \"candidate_preview_asset_id\": ";
        writeJsonString(out, sample.candidatePreviewAssetId);
        out << ", \"fallback_reason\": ";
        writeJsonString(out, sample.fallbackReason);
        out << ", \"qa_label\": ";
        writeJsonString(out, sample.qaLabel);
        out << ", \"target_asset_count\": " << sample.targetAssetCount;
        out << ", \"accepted_asset_count\": " << sample.acceptedAssetCount;
        out << ", \"candidate_asset_count\": " << sample.candidateAssetCount;
        out << "}";
    }
    if (!samples.empty())
        out << "\n      ";
    out << "]";
}

std::string formatElapsedMs(long long elapsedMs);

std::string slowestStageSummary(const ReviewResult &result)
{
    if (result.generationStageTimings.empty())
        return "n/a";

    const GenerationStageTiming *slowest = nullptr;
    for (const GenerationStageTiming &timing : result.generationStageTimings)
    {
        if (timing.stage == "Complete")
            continue;
        if (!slowest || timing.elapsedMs > slowest->elapsedMs)
            slowest = &timing;
    }

    if (!slowest)
        slowest = &result.generationStageTimings.back();

    if (slowest->elapsedMs == 0)
        return "all generator stages <1 ms";

    std::ostringstream stream;
    stream << slowest->stage << " " << slowest->step << "/" << slowest->totalSteps
           << " (" << formatElapsedMs(slowest->elapsedMs) << ")";
    return stream.str();
}

std::string phaseTimingSummary(const ReviewResult &result)
{
    std::ostringstream stream;
    stream << "gen " << formatElapsedMs(result.generationElapsedMs)
           << ", metrics " << formatElapsedMs(result.metricsElapsedMs)
           << ", QA " << formatElapsedMs(result.validationElapsedMs);
    return stream.str();
}

ReviewTimingSummary reviewTimingSummary(const std::vector<ReviewResult> &results)
{
    ReviewTimingSummary summary;
    if (results.empty())
        return summary;

    for (const ReviewResult &result : results)
    {
        summary.totalElapsedMs += result.elapsedMs;
        if (result.elapsedMs > summary.maxElapsedMs)
        {
            summary.maxElapsedMs = result.elapsedMs;
            summary.slowestCase = result.reviewCase.name;
        }
    }
    summary.averageElapsedMs = static_cast<double>(summary.totalElapsedMs) / static_cast<double>(results.size());
    return summary;
}

std::string reportStyleId(const std::vector<ReviewResult> &results)
{
    for (const ReviewResult &result : results)
    {
        if (!result.reviewCase.options.styleId.empty())
            return result.reviewCase.options.styleId;
    }
    return "fantasy_neutral";
}

std::string reportStyleIdFromCases(const std::vector<ReviewCase> &cases)
{
    for (const ReviewCase &reviewCase : cases)
    {
        if (!reviewCase.options.styleId.empty())
            return reviewCase.options.styleId;
    }
    return "fantasy_neutral";
}

std::string contextualAssetCoverageSummary(const std::string &styleId)
{
    const int targetAssets = mapStyleContextualTargetAssetCount(styleId);
    const int acceptedAssets = mapStyleContextualAcceptedAssetCount(styleId);
    const int candidateAssets = mapStyleContextualCandidateAssetCount(styleId);

    std::ostringstream stream;
    stream << acceptedAssets << "/" << targetAssets
           << " accepted contextual assets, "
           << candidateAssets << " candidate preview assets";
    if (candidateAssets > 0 && acceptedAssets < targetAssets)
        stream << " (candidate-only; runtime still falls back where accepted art is missing)";
    return stream.str();
}

void writeContextualAssetCoverageJson(std::ostream &out, const std::string &styleId)
{
    const int targetAssets = mapStyleContextualTargetAssetCount(styleId);
    const int acceptedAssets = mapStyleContextualAcceptedAssetCount(styleId);
    const int candidateAssets = mapStyleContextualCandidateAssetCount(styleId);

    out << "{";
    out << "\"style_id\": ";
    writeJsonString(out, styleId);
    out << ", \"target_asset_count\": " << targetAssets;
    out << ", \"accepted_asset_count\": " << acceptedAssets;
    out << ", \"candidate_asset_count\": " << candidateAssets;
    out << ", \"accepted_completion_percent\": " << mapStyleContextualCompletionPercent(styleId);
    out << "}";
}

void writeJsonReport(
    const std::filesystem::path &path,
    const std::vector<ReviewResult> &results,
    bool artReadiness,
    const std::string &caseFilter,
    std::size_t availableCaseCount,
    int caseOffset,
    int caseLimit,
    long long preflightElapsedMs,
    long long artReadinessWarmupElapsedMs)
{
    std::ofstream out(path);
    const ReviewTimingSummary timing = reviewTimingSummary(results);
    const std::string styleId = reportStyleId(results);
    int errors = 0;
    int warnings = 0;
    for (const ReviewResult &result : results)
    {
        for (const Diagnostic &diagnostic : result.diagnostics)
        {
            if (diagnostic.severity == "ERROR")
                ++errors;
            else if (diagnostic.severity == "WARN")
                ++warnings;
        }
    }

    out << "{\n";
    out << "  \"schema_version\": 1,\n";
    out << "  \"report_type\": \"map_generation_art_readiness\",\n";
    out << "  \"case_count\": " << results.size() << ",\n";
    out << "  \"case_filter\": ";
    writeJsonString(out, caseFilter);
    out << ",\n";
    out << "  \"available_case_count\": " << availableCaseCount << ",\n";
    out << "  \"case_offset\": " << caseOffset << ",\n";
    out << "  \"case_limit\": ";
    if (caseLimit >= 0)
        out << caseLimit;
    else
        out << "null";
    out << ",\n";
    out << "  \"error_count\": " << errors << ",\n";
    out << "  \"warning_count\": " << warnings << ",\n";
    out << "  \"art_readiness\": " << (artReadiness ? "true" : "false") << ",\n";
    out << "  \"context_fixtures\": " << (reportIncludesContextFixtures(results) ? "true" : "false") << ",\n";
    out << "  \"contextual_asset_coverage\": ";
    if (artReadiness)
        writeContextualAssetCoverageJson(out, styleId);
    else
        out << "{}";
    out << ",\n";
    out << "  \"timing\": {";
    out << "\"total_elapsed_ms\": " << timing.totalElapsedMs;
    out << ", \"case_loop_elapsed_ms\": " << timing.totalElapsedMs;
    out << ", \"preflight_elapsed_ms\": " << preflightElapsedMs;
    out << ", \"art_readiness_warmup_elapsed_ms\": " << artReadinessWarmupElapsedMs;
    out << ", \"average_elapsed_ms\": " << std::fixed << std::setprecision(2) << timing.averageElapsedMs << std::defaultfloat;
    out << ", \"max_elapsed_ms\": " << timing.maxElapsedMs;
    out << ", \"slowest_case\": ";
    writeJsonString(out, timing.slowestCase);
    out << "},\n";
    out << "  \"label_counts\": {";
    for (size_t i = 0; i < coreArtReadinessLabels().size(); ++i)
    {
        if (i > 0)
            out << ",";
        const std::string label = coreArtReadinessLabels()[i];
        out << "\n    ";
        writeJsonString(out, label);
        out << ": " << diagnosticLabelCount(results, label);
    }
    out << "\n  },\n";
    out << "  \"results\": [\n";
    for (size_t resultIndex = 0; resultIndex < results.size(); ++resultIndex)
    {
        const ReviewResult &result = results[resultIndex];
        if (resultIndex > 0)
            out << ",\n";
        out << "    {\n";
        out << "      \"case\": ";
        writeJsonString(out, result.reviewCase.name);
        out << ",\n";
        out << "      \"intent\": ";
        writeJsonString(out, result.reviewCase.intent);
        out << ",\n";
        out << "      \"fixture\": ";
        writeJsonString(out, reviewFixtureLabel(result.reviewCase.fixture));
        out << ",\n";
        out << "      \"seed\": " << result.reviewCase.options.seed << ",\n";
        out << "      \"style_id\": ";
        writeJsonString(out, result.reviewCase.options.styleId);
        out << ",\n";
        out << "      \"map\": {\"width\": " << result.generated.map.width << ", \"height\": " << result.generated.map.height << "},\n";
        out << "      \"options_summary\": ";
        writeJsonString(out, reviewCaseOptionsSummary(result.reviewCase));
        out << ",\n";
        out << "      \"metrics_summary\": ";
        writeJsonString(out, metricsSummary(result));
        out << ",\n";
        out << "      \"elapsed_ms\": " << result.elapsedMs << ",\n";
        out << "      \"phase_timing\": {";
        out << "\"generation_elapsed_ms\": " << result.generationElapsedMs;
        out << ", \"metrics_elapsed_ms\": " << result.metricsElapsedMs;
        out << ", \"validation_elapsed_ms\": " << result.validationElapsedMs;
        out << "},\n";
        out << "      \"generation_stage_timings\": ";
        writeGenerationStageTimingsJson(out, result.generationStageTimings);
        out << ",\n";
        out << "      \"contextual_coverage_samples\": ";
        writeContextualCoverageSamplesJson(out, result.contextualCoverageSamples);
        out << ",\n";
        out << "      \"diagnostics\": [";
        for (size_t diagnosticIndex = 0; diagnosticIndex < result.diagnostics.size(); ++diagnosticIndex)
        {
            const Diagnostic &diagnostic = result.diagnostics[diagnosticIndex];
            if (diagnosticIndex > 0)
                out << ",";
            out << "\n        {";
            out << "\"severity\": ";
            writeJsonString(out, diagnostic.severity);
            out << ", \"label\": ";
            writeJsonString(out, diagnostic.label);
            out << ", \"message\": ";
            writeJsonString(out, diagnostic.message);
            out << ", \"semantic_context\": ";
            writeJsonString(out, diagnostic.semanticContext);
            out << ", \"selected_asset_id\": ";
            writeJsonString(out, diagnostic.selectedAssetId);
            out << ", \"fallback_reason\": ";
            writeJsonString(out, diagnostic.fallbackReason);
            out << ", \"coordinates\": ";
            writeCoordinateArrayJson(out, diagnostic.samples);
            out << "}";
        }
        if (!result.diagnostics.empty())
            out << "\n      ";
        out << "]\n";
        out << "    }";
    }
    out << "\n  ]\n";
    out << "}\n";
}

void writeReport(
    const std::filesystem::path &path,
    const std::vector<ReviewResult> &results,
    const std::filesystem::path &contactSheetPath,
    bool wroteContactSheet,
    bool artReadiness,
    const std::string &caseFilter,
    std::size_t availableCaseCount,
    int caseOffset,
    int caseLimit,
    long long preflightElapsedMs,
    long long artReadinessWarmupElapsedMs)
{
    std::ofstream out(path);
    const ReviewTimingSummary timing = reviewTimingSummary(results);
    const std::string styleId = reportStyleId(results);
    int errors = 0;
    int warnings = 0;
    for (const ReviewResult &result : results)
    {
        for (const Diagnostic &diagnostic : result.diagnostics)
        {
            if (diagnostic.severity == "ERROR")
                ++errors;
            else if (diagnostic.severity == "WARN")
                ++warnings;
        }
    }

    out << "# Map Generation Batch Review\n\n";
    out << "- Cases: " << results.size() << "\n";
    if (!caseFilter.empty())
        out << "- Case filter: `" << caseFilter << "`\n";
    if (caseOffset > 0 || caseLimit >= 0)
    {
        out << "- Case slice: offset " << caseOffset << ", limit ";
        if (caseLimit >= 0)
            out << caseLimit;
        else
            out << "all";
        out << " from " << availableCaseCount << " filtered cases\n";
    }
    out << "- Context fixtures: " << (reportIncludesContextFixtures(results) ? "included" : "not included") << "\n";
    out << "- Errors: " << errors << "\n";
    out << "- Warnings: " << warnings << "\n";
    if (artReadiness)
        out << "- Contextual runtime coverage: " << contextualAssetCoverageSummary(styleId) << "\n";
    out << "- Preflight: " << formatElapsedMs(preflightElapsedMs);
    if (artReadiness)
        out << " (contextual style registry warm-up " << formatElapsedMs(artReadinessWarmupElapsedMs) << ")";
    out << "\n";
    out << "- Timing: total " << formatElapsedMs(timing.totalElapsedMs)
        << ", average " << formatElapsedMs(static_cast<long long>(timing.averageElapsedMs))
        << ", slowest `" << (timing.slowestCase.empty() ? "<none>" : timing.slowestCase)
        << "` at " << formatElapsedMs(timing.maxElapsedMs) << "\n";
    out << "- Art readiness checks: " << (artReadiness ? "enabled" : "disabled") << "\n";
    out << "- Contact sheet: " << (wroteContactSheet ? "`" + contactSheetPath.generic_string() + "`" : "skipped") << "\n\n";
    if (artReadiness)
    {
        out << "## Art Readiness Labels\n\n";
        out << "| Label | Diagnostics |\n";
        out << "| --- | ---: |\n";
        for (const char *label : coreArtReadinessLabels())
            out << "| `" << label << "` | " << diagnosticLabelCount(results, label) << " |\n";
        out << "\n";
    }
    out << "| # | Case | Options | Duration | Phase timing | Slowest gen stage | Counts | QA metrics | Diagnostics |\n";
    out << "| ---: | --- | --- | ---: | --- | --- | --- | --- | --- |\n";
    for (size_t index = 0; index < results.size(); ++index)
    {
        const ReviewResult &result = results[index];
        const MapModel &map = result.generated.map;
        std::ostringstream counts;
        counts << "water " << terrainCount(map, MapTileKind::Water)
               << ", mtn " << terrainCount(map, MapTileKind::Mountain)
               << ", forest " << terrainCount(map, MapTileKind::Forest)
               << ", road " << roadLikeWayCount(map)
               << " (trail " << wayCount(map, MapWayKind::Trail)
               << ", dirt " << wayCount(map, MapWayKind::DirtRoad)
               << ", stone " << wayCount(map, MapWayKind::StoneRoad) << ")"
               << ", river " << wayCount(map, MapWayKind::River)
               << ", corridor " << wayCount(map, MapWayKind::DungeonCorridor)
               << ", tunnel " << wayCount(map, MapWayKind::CaveTunnel)
               << ", tact " << tacticalCount(map)
               << ", routes " << map.routes.size();

        std::ostringstream diagnostics;
        if (result.diagnostics.empty())
            diagnostics << "OK";
        for (size_t i = 0; i < result.diagnostics.size(); ++i)
        {
            if (i > 0)
                diagnostics << "<br>";
            diagnostics << diagnosticSummary(result.diagnostics[i]);
        }

        out << "| " << (index + 1)
            << " | `" << result.reviewCase.name << "`<br>" << result.reviewCase.intent
            << " | " << reviewCaseOptionsSummary(result.reviewCase)
            << " | " << formatElapsedMs(result.elapsedMs)
            << " | " << phaseTimingSummary(result)
            << " | " << slowestStageSummary(result)
            << " | " << counts.str()
            << " | " << metricsSummary(result)
            << " | " << diagnostics.str()
            << " |\n";
    }
}

int parseCycles(int argc, char **argv)
{
    for (int i = 1; i + 1 < argc; ++i)
    {
        if (std::string(argv[i]) == "--cycles")
            return std::clamp(std::stoi(argv[i + 1]), 1, 1000);
    }
    return 8;
}

int parseStartCycle(int argc, char **argv)
{
    for (int i = 1; i + 1 < argc; ++i)
    {
        if (std::string(argv[i]) == "--start-cycle")
            return std::max(0, std::stoi(argv[i + 1]));
    }
    return 0;
}

int parseCaseOffset(int argc, char **argv)
{
    for (int i = 1; i + 1 < argc; ++i)
    {
        if (std::string(argv[i]) == "--case-offset")
            return std::max(0, std::stoi(argv[i + 1]));
    }
    return 0;
}

int parseCaseLimit(int argc, char **argv)
{
    for (int i = 1; i + 1 < argc; ++i)
    {
        if (std::string(argv[i]) == "--case-limit")
            return std::max(1, std::stoi(argv[i + 1]));
    }
    return -1;
}

std::filesystem::path parseOutputDirectory(int argc, char **argv)
{
    for (int i = 1; i + 1 < argc; ++i)
    {
        if (std::string(argv[i]) == "--out")
            return argv[i + 1];
    }
    return std::filesystem::current_path() / "map-generation-review";
}

bool parseSkipContactSheet(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--skip-contact-sheet")
            return true;
    }
    return false;
}

bool parseArtReadiness(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--art-readiness")
            return true;
    }
    return false;
}

bool hasFlag(int argc, char **argv, const std::string &flag)
{
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == flag)
            return true;
    }
    return false;
}

std::string parseCaseFilter(int argc, char **argv)
{
    for (int i = 1; i + 1 < argc; ++i)
    {
        if (std::string(argv[i]) == "--case-filter")
            return argv[i + 1];
    }
    return "";
}

void printUsage(const char *executable)
{
    std::cout << "Usage: " << executable << " [options]\n"
              << "\n"
              << "Options:\n"
              << "  --cycles N              Number of 8-case cycles to generate (1-1000, default 8)\n"
              << "  --start-cycle N         First cycle index (default 0)\n"
              << "  --case-filter TEXT      Keep only cases whose name or intent contains TEXT\n"
              << "  --case-offset N         Skip N filtered cases before running (default 0)\n"
              << "  --case-limit N          Run at most N filtered cases after the offset (default all)\n"
              << "  --include-context-fixtures\n"
              << "                          Include review-only authored contextual fixture cases\n"
              << "  --list-cases            Print the selected case names/intents and exit\n"
              << "  --out DIR               Output directory (default ./map-generation-review)\n"
              << "  --art-readiness         Enable contextual production-art readiness diagnostics\n"
              << "  --skip-contact-sheet    Skip writing contact_sheet.bmp\n"
              << "  --help                  Show this help and exit\n"
              << "\n"
              << "Outputs:\n"
              << "  progress.json           Progress sidecar written inside the output directory\n";
}

std::string lowercaseAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::vector<ReviewCase> filterReviewCases(const std::vector<ReviewCase> &cases, const std::string &filter)
{
    if (filter.empty())
        return cases;

    const std::string normalizedFilter = lowercaseAscii(filter);
    std::vector<ReviewCase> filtered;
    for (const ReviewCase &reviewCase : cases)
    {
        const std::string normalizedName = lowercaseAscii(reviewCase.name);
        const std::string normalizedIntent = lowercaseAscii(reviewCase.intent);
        if (normalizedName.find(normalizedFilter) != std::string::npos || normalizedIntent.find(normalizedFilter) != std::string::npos)
            filtered.push_back(reviewCase);
    }
    return filtered;
}

std::vector<ReviewCase> sliceReviewCases(const std::vector<ReviewCase> &cases, int caseOffset, int caseLimit)
{
    const std::size_t offset = std::min<std::size_t>(static_cast<std::size_t>(caseOffset), cases.size());
    const std::size_t availableAfterOffset = cases.size() - offset;
    const std::size_t count = caseLimit >= 0
        ? std::min<std::size_t>(static_cast<std::size_t>(caseLimit), availableAfterOffset)
        : availableAfterOffset;
    return std::vector<ReviewCase>(cases.begin() + static_cast<std::ptrdiff_t>(offset), cases.begin() + static_cast<std::ptrdiff_t>(offset + count));
}

std::string formatElapsedMs(long long elapsedMs)
{
    std::ostringstream stream;
    if (elapsedMs < 1000)
    {
        stream << elapsedMs << " ms";
    }
    else
    {
        stream << std::fixed << std::setprecision(1) << (static_cast<double>(elapsedMs) / 1000.0) << " s";
    }
    return stream.str();
}

std::string formatElapsed(std::chrono::steady_clock::duration duration)
{
    return formatElapsedMs(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
}

std::string formatEstimatedRemaining(
    std::size_t completed,
    std::size_t total,
    std::chrono::steady_clock::duration elapsed)
{
    if (completed == 0 || completed >= total)
        return formatElapsedMs(0);

    const long long elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    const long long remaining = static_cast<long long>(total - completed);
    const long long estimatedMs = (elapsedMs * remaining) / static_cast<long long>(completed);
    return formatElapsedMs(estimatedMs);
}

bool writeProgressJson(
    const std::filesystem::path &path,
    const std::string &status,
    std::size_t totalCases,
    std::size_t completedCases,
    const std::string &currentCase,
    const std::string &currentStage,
    long long elapsedMs,
    const std::string &eta,
    int errorCount,
    int warningCount,
    const std::string &caseFilter,
    std::size_t availableCaseCount,
    int caseOffset,
    int caseLimit)
{
    std::ofstream out(path);
    if (!out)
        return false;

    out << "{\n";
    out << "  \"schema_version\": 1,\n";
    out << "  \"report_type\": \"map_generation_batch_progress\",\n";
    out << "  \"status\": ";
    writeJsonString(out, status);
    out << ",\n";
    out << "  \"total_cases\": " << totalCases << ",\n";
    out << "  \"completed_cases\": " << completedCases << ",\n";
    out << "  \"current_case\": ";
    writeJsonString(out, currentCase);
    out << ",\n";
    out << "  \"current_stage\": ";
    writeJsonString(out, currentStage);
    out << ",\n";
    out << "  \"elapsed_ms\": " << elapsedMs << ",\n";
    out << "  \"eta\": ";
    writeJsonString(out, eta);
    out << ",\n";
    out << "  \"error_count\": " << errorCount << ",\n";
    out << "  \"warning_count\": " << warningCount << ",\n";
    out << "  \"case_filter\": ";
    writeJsonString(out, caseFilter);
    out << ",\n";
    out << "  \"available_case_count\": " << availableCaseCount << ",\n";
    out << "  \"case_offset\": " << caseOffset << ",\n";
    out << "  \"case_limit\": ";
    if (caseLimit >= 0)
        out << caseLimit;
    else
        out << "null";
    out << "\n";
    out << "}\n";
    return true;
}
}

int main(int argc, char **argv)
{
    if (hasFlag(argc, argv, "--help"))
    {
        printUsage(argv[0]);
        return 0;
    }

    const int cycles = parseCycles(argc, argv);
    const int startCycle = parseStartCycle(argc, argv);
    const bool skipContactSheet = parseSkipContactSheet(argc, argv);
    const bool artReadiness = parseArtReadiness(argc, argv);
    const bool includeContextFixtures = hasFlag(argc, argv, "--include-context-fixtures");
    const bool listCases = hasFlag(argc, argv, "--list-cases");
    const std::string caseFilter = parseCaseFilter(argc, argv);
    const int caseOffset = parseCaseOffset(argc, argv);
    const int caseLimit = parseCaseLimit(argc, argv);
    const std::filesystem::path outputDirectory = parseOutputDirectory(argc, argv);

    const std::vector<ReviewCase> filteredReviewCases = filterReviewCases(makeReviewCases(cycles, startCycle, includeContextFixtures), caseFilter);
    if (filteredReviewCases.empty())
    {
        std::cerr << "No review cases matched --case-filter \"" << caseFilter << "\"\n";
        std::cerr << "Use --list-cases with the same --cycles/--include-context-fixtures options to inspect available case names.\n";
        return 2;
    }
    const std::vector<ReviewCase> reviewCases = sliceReviewCases(filteredReviewCases, caseOffset, caseLimit);
    if (reviewCases.empty())
    {
        std::cerr << "No review cases selected after --case-offset " << caseOffset;
        if (caseLimit >= 0)
            std::cerr << " and --case-limit " << caseLimit;
        std::cerr << " from " << filteredReviewCases.size() << " filtered case(s).\n";
        return 2;
    }
    if (listCases)
    {
        for (const ReviewCase &reviewCase : reviewCases)
        {
            std::cout << reviewCase.name << " | " << reviewCase.intent << " | " << reviewCaseOptionsSummary(reviewCase) << '\n';
        }
        return 0;
    }

    std::filesystem::create_directories(outputDirectory);
    std::vector<ReviewResult> results;
    results.reserve(reviewCases.size());
    const auto batchStartedAt = std::chrono::steady_clock::now();
    const std::filesystem::path progressPath = outputDirectory / "progress.json";
    bool progressWarningPrinted = false;
    const auto writeProgress = [&](const std::string &status, std::size_t completedCases, const std::string &currentCase, const std::string &currentStage, const std::string &eta) {
        const long long elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - batchStartedAt).count();
        const std::pair<int, int> diagnosticCounts = diagnosticSeverityCounts(results);
        if (!writeProgressJson(
                progressPath,
                status,
                reviewCases.size(),
                completedCases,
                currentCase,
                currentStage,
                elapsedMs,
                eta,
                diagnosticCounts.first,
                diagnosticCounts.second,
                caseFilter,
                filteredReviewCases.size(),
                caseOffset,
                caseLimit) &&
            !progressWarningPrinted)
        {
            std::cerr << "Warning: failed to write progress file: " << progressPath << '\n';
            progressWarningPrinted = true;
        }
    };
    writeProgress("starting", 0, "", "", formatElapsedMs(0));
    std::cout << "Reviewing " << reviewCases.size() << " generated maps into " << outputDirectory << '\n';
    std::cout << "Progress file: " << progressPath << '\n';
    if (!caseFilter.empty())
        std::cout << "Case filter: \"" << caseFilter << "\"\n";
    if (caseOffset > 0 || caseLimit >= 0)
    {
        std::cout << "Case slice: offset " << caseOffset << ", limit ";
        if (caseLimit >= 0)
            std::cout << caseLimit;
        else
            std::cout << "all";
        std::cout << " from " << filteredReviewCases.size() << " filtered cases\n";
    }
    if (includeContextFixtures)
        std::cout << "Context fixtures: included (review-only authored projections)\n";
    std::cout << "Art readiness checks: " << (artReadiness ? "enabled" : "disabled") << "; contact sheet: " << (skipContactSheet ? "skipped" : "enabled") << '\n';
    long long artReadinessWarmupElapsedMs = 0;
    if (artReadiness)
    {
        const auto warmupStartedAt = std::chrono::steady_clock::now();
        const std::size_t contextualWorkOrders = mapStyleContextualAssetCoverage().size();
        artReadinessWarmupElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - warmupStartedAt).count();
        std::cout << "Contextual style registry warm-up: " << contextualWorkOrders
                  << " work order(s) in " << formatElapsedMs(artReadinessWarmupElapsedMs) << '\n';
        std::cout << "Contextual runtime coverage: " << contextualAssetCoverageSummary(reportStyleIdFromCases(reviewCases)) << '\n';
    }

    const auto mapLoopStartedAt = std::chrono::steady_clock::now();
    const long long preflightElapsedMs = artReadiness
        ? std::chrono::duration_cast<std::chrono::milliseconds>(mapLoopStartedAt - batchStartedAt).count()
        : 0;
    for (std::size_t index = 0; index < reviewCases.size(); ++index)
    {
        const ReviewCase &reviewCase = reviewCases[index];
        const std::size_t completed = index + 1;
        std::cout << "[" << completed << "/" << reviewCases.size() << "] starting "
                  << reviewCase.name << " (" << reviewCaseOptionsSummary(reviewCase) << ")\n"
                  << std::flush;
        writeProgress("running", results.size(), reviewCase.name, "starting", formatEstimatedRemaining(results.size(), reviewCases.size(), std::chrono::steady_clock::now() - mapLoopStartedAt));

        const auto mapStartedAt = std::chrono::steady_clock::now();
        std::vector<GenerationStageTiming> generationStageTimings;
        std::string activeGenerationStage;
        int activeGenerationStep = 0;
        int activeGenerationTotalSteps = 0;
        auto activeGenerationStageStartedAt = mapStartedAt;
        MapGenerationOptions generationOptions = reviewCase.options;
        generationOptions.progressCallback = [&](const MapGenerationProgress &progress) {
            const auto now = std::chrono::steady_clock::now();
            long long previousStageElapsedMs = 0;
            if (!activeGenerationStage.empty())
            {
                previousStageElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - activeGenerationStageStartedAt).count();
                generationStageTimings.push_back(GenerationStageTiming{
                    activeGenerationStage,
                    activeGenerationStep,
                    activeGenerationTotalSteps,
                    previousStageElapsedMs});
            }
            std::cout << "[" << completed << "/" << reviewCases.size() << "] "
                      << reviewCase.name << " generation " << progress.step << "/" << progress.totalSteps
                      << ": " << progress.stage;
            if (previousStageElapsedMs > 0)
                std::cout << " (previous " << formatElapsedMs(previousStageElapsedMs) << ")";
            std::cout << '\n'
                      << std::flush;
            writeProgress("generating", results.size(), reviewCase.name, progress.stage, formatEstimatedRemaining(results.size(), reviewCases.size(), std::chrono::steady_clock::now() - mapLoopStartedAt));
            activeGenerationStage = progress.stage;
            activeGenerationStep = progress.step;
            activeGenerationTotalSteps = progress.totalSteps;
            activeGenerationStageStartedAt = now;
        };

        ReviewResult result;
        result.reviewCase = reviewCase;
        const auto generationStartedAt = std::chrono::steady_clock::now();
        result.generated = generateSemanticMap(generationOptions);
        applyReviewFixture(result.generated.map, reviewCase.fixture);
        result.generationElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - generationStartedAt).count();
        if (!activeGenerationStage.empty())
        {
            generationStageTimings.push_back(GenerationStageTiming{
                activeGenerationStage,
                activeGenerationStep,
                activeGenerationTotalSteps,
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - activeGenerationStageStartedAt).count()});
        }
        const auto metricsStartedAt = std::chrono::steady_clock::now();
        result.metrics = measureQuality(result.generated.map, result.reviewCase.options, artReadiness);
        result.metricsElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - metricsStartedAt).count();
        const auto validationStartedAt = std::chrono::steady_clock::now();
        if (artReadiness)
            result.contextualCoverageSamples = contextualCoverageSamplesForGeneratedMap(result.reviewCase, result.generated, result.metrics);
        result.diagnostics = validateGeneratedMap(result.reviewCase, result.generated, result.metrics, artReadiness);
        result.validationElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - validationStartedAt).count();
        result.generationStageTimings = std::move(generationStageTimings);
        result.elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - mapStartedAt).count();
        const int resultErrors = static_cast<int>(std::count_if(result.diagnostics.begin(), result.diagnostics.end(), [](const Diagnostic &diagnostic) { return diagnostic.severity == "ERROR"; }));
        const int resultWarnings = static_cast<int>(std::count_if(result.diagnostics.begin(), result.diagnostics.end(), [](const Diagnostic &diagnostic) { return diagnostic.severity == "WARN"; }));
        results.push_back(std::move(result));

        const auto now = std::chrono::steady_clock::now();
        const auto batchElapsed = now - batchStartedAt;
        const auto mapLoopElapsed = now - mapLoopStartedAt;
        std::cout << "[" << completed << "/" << reviewCases.size() << "] "
                  << reviewCase.name
                  << " done in " << formatElapsedMs(results.back().elapsedMs)
                  << "; phases " << phaseTimingSummary(results.back())
                  << "; slowest gen stage " << slowestStageSummary(results.back())
                  << " (" << resultErrors << " error(s), " << resultWarnings << " warning(s))"
                  << "; total " << formatElapsed(batchElapsed)
                  << "; eta " << formatEstimatedRemaining(results.size(), reviewCases.size(), mapLoopElapsed)
                  << '\n'
                  << std::flush;
        writeProgress(
            results.size() == reviewCases.size() ? "writing_reports" : "running",
            results.size(),
            results.size() == reviewCases.size() ? "" : reviewCase.name,
            "",
            formatEstimatedRemaining(results.size(), reviewCases.size(), mapLoopElapsed));
    }

    const std::filesystem::path contactSheetPath = outputDirectory / "contact_sheet.bmp";
    const std::filesystem::path reportPath = outputDirectory / "map_generation_batch_report.md";
    const std::filesystem::path jsonReportPath = outputDirectory / "map_generation_batch_report.json";
    writeProgress("writing_reports", results.size(), "", "", formatElapsedMs(0));
    if (!skipContactSheet)
    {
        const Image contactSheet = renderContactSheet(results);
        if (!writeBmp(contactSheetPath, contactSheet))
        {
            std::cerr << "Failed to write contact sheet: " << contactSheetPath << '\n';
            return 2;
        }
    }
    writeReport(
        reportPath,
        results,
        contactSheetPath,
        !skipContactSheet,
        artReadiness,
        caseFilter,
        filteredReviewCases.size(),
        caseOffset,
        caseLimit,
        preflightElapsedMs,
        artReadinessWarmupElapsedMs);
    writeJsonReport(
        jsonReportPath,
        results,
        artReadiness,
        caseFilter,
        filteredReviewCases.size(),
        caseOffset,
        caseLimit,
        preflightElapsedMs,
        artReadinessWarmupElapsedMs);
    writeProgress("complete", results.size(), "", "", formatElapsedMs(0));

    int errors = 0;
    int warnings = 0;
    for (const ReviewResult &result : results)
    {
        for (const Diagnostic &diagnostic : result.diagnostics)
        {
            if (diagnostic.severity == "ERROR")
                ++errors;
            else if (diagnostic.severity == "WARN")
                ++warnings;
        }
    }
    const ReviewTimingSummary timing = reviewTimingSummary(results);

    std::cout << "Generated " << results.size() << " maps into " << outputDirectory << '\n';
    std::cout << "Errors: " << errors << ", warnings: " << warnings << '\n';
    std::cout << "Timing: total " << formatElapsedMs(timing.totalElapsedMs)
              << ", average " << formatElapsedMs(static_cast<long long>(timing.averageElapsedMs))
              << ", slowest " << (timing.slowestCase.empty() ? "<none>" : timing.slowestCase)
              << " at " << formatElapsedMs(timing.maxElapsedMs) << '\n';
    std::cout << "Preflight: " << formatElapsedMs(preflightElapsedMs);
    if (artReadiness)
        std::cout << " (contextual style registry warm-up " << formatElapsedMs(artReadinessWarmupElapsedMs) << ")";
    std::cout << '\n';
    std::cout << "Art readiness checks: " << (artReadiness ? "enabled" : "disabled") << '\n';
    std::cout << "Report: " << reportPath << '\n';
    std::cout << "JSON report: " << jsonReportPath << '\n';
    if (!skipContactSheet)
        std::cout << "Contact sheet: " << contactSheetPath << '\n';
    else
        std::cout << "Contact sheet: skipped\n";
    return errors == 0 ? 0 : 1;
}
