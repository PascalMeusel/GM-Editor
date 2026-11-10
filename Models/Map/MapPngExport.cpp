#include "Models/Map/MapLocalIllustration.hpp"
#include "Models/Map/MapTacticalCoverInk.hpp"
#include "Models/Map/MapSettlementSiteInk.hpp"
#include "Models/Map/MapTacticalStructureInk.hpp"
#include "Models/Map/MapTravelInk.hpp"
#include "Models/Map/MapPngExport.hpp"
#include "Models/Map/MapPngExportPlanning.hpp"
#include "Models/Map/PngExport/PngRasterCanvas.hpp"
#include "Models/Map/PngExport/PngRasterAssets.hpp"
#include "Models/Map/PngExport/PngAtlasInk.hpp"
#include "Models/Map/PngExport/PngLabelRenderer.hpp"
#include "Models/Map/PngExport/PngRenderSession.hpp"
#include "Models/Map/PngExport/PngSemanticOverlays.hpp"
#include "Models/Map/PngExport/PngTextRenderer.hpp"
#include "Models/Map/PngExport/PngTerrainAssets.hpp"
#include "Models/Map/PngExport/PngTerrainSampler.hpp"
#include "Models/Map/PngExport/PngVisualPolicy.hpp"
#include "Models/Map/PngExport/PngWorldSurface.hpp"
#include "Models/Map/PngExport/PngWorldSymbols.hpp"
#include "Models/Map/MapFogTexture.hpp"
#include "Models/Map/MapAtlasWaterInk.hpp"
#include "Models/Map/MapClassicIllustration.hpp"
#include "Models/Map/MapGeneratedNamePresentation.hpp"
#include "Models/Map/MapImageImportService.hpp"
#include "Models/Map/MapImportedReferencePolicy.hpp"
#include "Models/Map/MapDungeonInk.hpp"
#include "Models/Map/MapCaveInk.hpp"
#include "Models/Map/MapOverviewRenderPolicy.hpp"
#include "Models/Map/MapOverviewLabelLod.hpp"
#include "Models/Map/MapRoutePresentation.hpp"
#include "Models/Map/MapStyleRegistry.hpp"
#include "Models/Project/ApplicationPaths.hpp"
#include "Models/Media/RgbaImageLoader.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

using namespace MapPngExportRaster;

namespace
{
static bool exportMapPngRasterized(const MapModel &map,
                                   const std::filesystem::path &path,
                                   const MapRenderPlan &plan,
                                   int requestedPixelsPerCell,
                                   std::string *error,
                                   float mapOffsetX = 0.0f,
                                   float mapOffsetY = 0.0f,
                                   MapNormalizedRect mapViewport = MapNormalizedRect{},
                                   const MapModel *fullTerrainSource = nullptr)
{
    return PngRenderSession(map, path, plan, requestedPixelsPerCell,
                            error, mapOffsetX, mapOffsetY, mapViewport,
                            fullTerrainSource).run();
}
}

bool exportMapPng(const MapModel &sourceMap,
                  const std::filesystem::path &path,
                  const MapPngExportOptions &options,
                  std::string *error)
{
    const MapRenderPlanRequest request = mapPngExportRenderPlanRequest(options);
    if (!mapPermitsPngExport(sourceMap, request.audience, error))
        return false;
    const MapRenderPlan plan = compileMapRenderPlan(sourceMap, request);
    return exportMapPngRasterized(filterMapForRenderPlan(sourceMap, plan), path,
                                  plan, options.pixelsPerCell, error);
}

bool exportMapPng(const MapModel &map,
                  const std::filesystem::path &path,
                  const MapRenderPlan &plan,
                  int pixelsPerCell,
                  std::string *error)
{
    if (!mapPermitsPngExport(map, plan.audience, error))
        return false;
    if (!mapRenderPlanContentMatchesMap(plan, map))
    {
        if (error)
            *error = "The map changed after the render preview. Create a new preview.";
        return false;
    }
    return exportMapPngRasterized(filterMapForRenderPlan(map, plan), path,
                                  plan, pixelsPerCell, error);
}

bool exportMapPngRegion(const MapModel &map,
                        const std::filesystem::path &path,
                        const MapRenderPlan &plan,
                        const MapPngExportRect &rect,
                        int pixelsPerCell,
                        std::string *error)
{
    if (!mapPermitsPngExport(map, plan.audience, error))
        return false;
    if (!mapRenderPlanContentMatchesMap(plan, map))
    {
        if (error)
            *error = "The map changed after the render preview. Create a new preview.";
        return false;
    }
    if (!pngRectIsValidForMap(rect, map))
    {
        if (error)
            *error = "The selected map view is invalid.";
        return false;
    }
    const MapPngExportSizing sizing = calculateMapPngExportSizing(rect, pixelsPerCell);
    if (!sizing.valid)
    {
        if (error)
            *error = sizing.error;
        return false;
    }

    const MapModel filtered = filterMapForRenderPlan(map, plan);
    const MapNormalizedRect mapViewport = pngNormalizedViewport(map, rect);
    return exportMapPngRasterized(cropMapForPngExport(filtered, rect), path,
                                  plan, pixelsPerCell, error,
                                  static_cast<float>(rect.x),
                                  static_cast<float>(rect.y),
                                  mapViewport, &filtered);
}
