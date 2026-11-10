#pragma once

#include "Models/Map/MapModels.hpp"
#include "Models/Map/MapRenderPlan.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

inline constexpr int kMapPngMinimumPixelsPerCell = 8;
inline constexpr int kMapPngMaximumPixelsPerCell = 128;
inline constexpr int kMapPngMaximumExportEdge = 16384;

enum class MapPngExportSizePreset
{
    Screen,
    UltraHd4K,
    Tactical100PixelsPerCell,
    Custom
};

// A side-effect-free sizing result shared by the UI, tests, and exporter.
// The renderer never silently changes a requested resolution: callers can
// explain an invalid request before opening a save dialog.
struct MapPngExportSizing
{
    int pixelsPerCell = 0;
    int widthPixels = 0;
    int heightPixels = 0;
    std::uint64_t uncompressedBytes = 0;
    bool valid = false;
    std::string error;
};

int mapPngPixelsPerCellForPreset(const MapModel &map,
                                 MapPngExportSizePreset preset,
                                 int customPixelsPerCell = 48);

MapPngExportSizing calculateMapPngExportSizing(const MapModel &map,
                                               int pixelsPerCell);

struct MapPngExportOptions
{
    int pixelsPerCell = 48;
    // Presentation locale for application-owned generated labels. Persisted
    // semantic names remain unchanged; unknown/user-authored names are never
    // translated by the exporter.
    std::string locale = "en";
    bool showGrid = false;
    // World and region exports are not useful without the names that give
    // their cartographic symbols meaning. Keep labels on by default for GM
    // and player-safe output; callers may still request an unlabelled plate.
    bool showLabels = true;
    bool showPoliticalOverlay = false;
    bool showBackground = true;
    bool showAnnotations = true;
    bool showRoutes = true;
    bool showTokens = true;
    bool playerSafe = false;
    bool applyFog = true;
};

// One request adapter for exported files and their interactive preview.
// compileMapRenderPlan additionally clamps Player requests to current consent.
inline MapRenderPlanRequest mapPngExportRenderPlanRequest(const MapPngExportOptions &options)
{
    MapRenderPlanRequest request;
    request.audience = options.playerSafe ? MapRenderAudience::Player : MapRenderAudience::GameMaster;
    request.locale = options.locale;
    request.layers = MapRenderPlanTerrain | MapRenderPlanWater | MapRenderPlanWays | MapRenderPlanSymbols;
    if (options.showBackground) request.layers |= MapRenderPlanBackground;
    if (options.showLabels) request.layers |= MapRenderPlanLabels;
    if (options.showPoliticalOverlay) request.layers |= MapRenderPlanPolitical;
    if (options.showAnnotations) request.layers |= MapRenderPlanAnnotations;
    if (options.showRoutes) request.layers |= MapRenderPlanRoutes;
    if (options.showTokens) request.layers |= MapRenderPlanTokens;
    if (options.applyFog || options.playerSafe) request.layers |= MapRenderPlanFog;
    if (options.showGrid) request.layers |= MapRenderPlanGrid;
    request.applyFog = options.applyFog || options.playerSafe;
    request.showGrid = options.showGrid;
    request.showPoliticalOverlay = options.showPoliticalOverlay;
    return request;
}

// Large exports are split on cell boundaries. Adjacent render rectangles may
// overlap by one cell so curves, labels and organic terrain have enough bleed
// at tile edges. `content` identifies the non-overlapping area represented by
// a tile; consumers can use it to assemble the full image without seams.
// Managed image crops and rotations remain calibrated in full-map space, so
// every overlap samples the same EXIF-oriented source pixels.
struct MapPngExportRect
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

MapPngExportSizing calculateMapPngExportSizing(const MapPngExportRect &rect,
                                               int pixelsPerCell);

struct MapPngExportTile
{
    int column = 0;
    int row = 0;
    MapPngExportRect content;
    MapPngExportRect render;
    std::filesystem::path fileName;
    int widthPixels = 0;
    int heightPixels = 0;
};

struct MapPngTiledExportPlan
{
    bool valid = false;
    int pixelsPerCell = 0;
    int fullWidthPixels = 0;
    int fullHeightPixels = 0;
    int columns = 0;
    int rows = 0;
    int overlapCells = 0;
    std::uint64_t totalUncompressedBytes = 0;
    std::string error;
    std::vector<MapPngExportTile> tiles;
};

MapPngTiledExportPlan planMapPngTiledExport(
    const MapModel &map,
    int pixelsPerCell,
    std::string fileStem,
    int maximumTileEdge = kMapPngMaximumExportEdge,
    int overlapCells = 1);

// Writes every planned PNG plus <fileStem>_tiles.json. Existing files are
// never overwritten unless the caller has obtained explicit confirmation.
bool exportMapPngTiled(const MapModel &map,
                       const std::filesystem::path &directory,
                       const std::string &fileStem,
                       const MapPngExportOptions &options,
                       bool overwriteExisting,
                       MapPngTiledExportPlan *writtenPlan = nullptr,
                       std::string *error = nullptr);

// Tiled counterpart to the reviewed-plan whole-map export. Every tile is
// derived from the same full-map visibility snapshot; tiles must never
// compile their own audience or layer policy after cropping.
bool exportMapPngTiled(const MapModel &map,
                       const std::filesystem::path &directory,
                       const std::string &fileStem,
                       const MapRenderPlan &plan,
                       int pixelsPerCell,
                       bool overwriteExisting,
                       MapPngTiledExportPlan *writtenPlan = nullptr,
                       std::string *error = nullptr);

bool exportMapPng(const MapModel &map,
                  const std::filesystem::path &path,
                  const MapPngExportOptions &options,
                  std::string *error = nullptr);

bool exportMapPng(const MapModel &map,
                  const std::filesystem::path &path,
                  const MapRenderPlan &plan,
                  int pixelsPerCell,
                  std::string *error = nullptr);

// Export an exact map-space rectangle using the already reviewed immutable
// plan. This is the "current view" counterpart to whole-map export; it keeps
// player filtering, fog, imported-image crop, and rotation identical to the
// reviewed full-map plan.
bool exportMapPngRegion(const MapModel &map,
                        const std::filesystem::path &path,
                        const MapRenderPlan &plan,
                        const MapPngExportRect &rect,
                        int pixelsPerCell,
                        std::string *error = nullptr);
