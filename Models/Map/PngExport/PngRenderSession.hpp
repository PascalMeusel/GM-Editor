#pragma once

#include "Models/Map/MapModels.hpp"
#include "Models/Map/MapPngExport.hpp"
#include "Models/Map/MapRenderPlan.hpp"
#include "Models/Map/MapTileSemantics.hpp"
#include "Models/Map/PngExport/PngRasterCanvas.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace MapPngExportRaster
{
class PngRenderSession final
{
public:
    PngRenderSession(const MapModel &map,
                     const std::filesystem::path &path,
                     const MapRenderPlan &plan,
                     int requestedPixelsPerCell,
                     std::string *error,
                     float mapOffsetX = 0.0f,
                     float mapOffsetY = 0.0f,
                     MapNormalizedRect mapViewport = {},
                     const MapModel *fullTerrainSource = nullptr);

    bool run();

private:
    bool initialize();
    void drawTerrainLayers();
    void drawFeatureLayers();
    void drawSymbolLayers();
    bool writeOutput();

    MapTileRenderContext classicTerrain(int x, int y) const;
    MapWayRenderContext classicWay(int x, int y) const;

    const MapModel &map;
    const std::filesystem::path &path;
    const MapRenderPlan &plan;
    int requestedPixelsPerCell = 0;
    std::string *error = nullptr;
    float mapOffsetX = 0.0f;
    float mapOffsetY = 0.0f;
    MapNormalizedRect mapViewport{};
    const MapModel *fullTerrainSource = nullptr;
    const MapModel &classicSource;
    int classicOffsetX = 0;
    int classicOffsetY = 0;

    int pixelsPerCell = 0;
    int width = 0;
    int height = 0;
    bool playerSafe = false;
    bool showBackground = false;
    bool showTerrain = false;
    bool showWater = false;
    bool showWays = false;
    bool showSymbols = false;
    bool showLabels = false;
    bool showAnnotations = false;
    bool showRoutes = false;
    bool showTokens = false;
    bool applyFog = false;
    bool showGrid = false;
    bool showPoliticalOverlay = false;
    bool importedBackgroundDrawn = false;
    std::vector<Pixel> pixels;
};
}
