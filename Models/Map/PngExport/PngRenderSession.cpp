#include "Models/Map/PngExport/PngRenderSession.hpp"

#include "Models/Map/MapImageImportService.hpp"
#include "Models/Map/MapImportedReferencePolicy.hpp"
#include "Models/Map/MapPngExportPlanning.hpp"
#include "Models/Map/PngExport/PngRasterAssets.hpp"

#include <utility>

namespace MapPngExportRaster
{
PngRenderSession::PngRenderSession(const MapModel &mapValue,
                                   const std::filesystem::path &pathValue,
                                   const MapRenderPlan &planValue,
                                   int requestedPixelsPerCellValue,
                                   std::string *errorValue,
                                   float mapOffsetXValue,
                                   float mapOffsetYValue,
                                   MapNormalizedRect mapViewportValue,
                                   const MapModel *fullTerrainSourceValue)
    : map(mapValue), path(pathValue), plan(planValue),
      requestedPixelsPerCell(requestedPixelsPerCellValue), error(errorValue),
      mapOffsetX(mapOffsetXValue), mapOffsetY(mapOffsetYValue),
      mapViewport(mapViewportValue), fullTerrainSource(fullTerrainSourceValue),
      classicSource(fullTerrainSourceValue ? *fullTerrainSourceValue : mapValue),
      classicOffsetX(fullTerrainSourceValue ? static_cast<int>(mapOffsetXValue) : 0),
      classicOffsetY(fullTerrainSourceValue ? static_cast<int>(mapOffsetYValue) : 0)
{
}

MapTileRenderContext PngRenderSession::classicTerrain(int x, int y) const
{
    return makeMapTileRenderContext(classicSource, x + classicOffsetX, y + classicOffsetY);
}

MapWayRenderContext PngRenderSession::classicWay(int x, int y) const
{
    return makeMapWayRenderContext(classicSource, x + classicOffsetX, y + classicOffsetY);
}

bool PngRenderSession::run()
{
    try
    {
        if (!initialize())
            return false;
        drawTerrainLayers();
        drawFeatureLayers();
        drawSymbolLayers();
        return writeOutput();
    }
    catch (const std::exception &exception)
    {
        if (error)
            *error = exception.what();
        return false;
    }
}

bool PngRenderSession::initialize()
{
    const MapPngExportSizing sizing = calculateMapPngExportSizing(map, requestedPixelsPerCell);
    if (!sizing.valid)
    {
        if (error)
            *error = sizing.error;
        return false;
    }
    pixelsPerCell = sizing.pixelsPerCell;
    width = sizing.widthPixels;
    height = sizing.heightPixels;
    playerSafe = plan.audience == MapRenderAudience::Player;
    showBackground = plan.backgroundVisible && (plan.layers & MapRenderPlanBackground) != 0;
    showTerrain = (plan.layers & MapRenderPlanTerrain) != 0;
    showWater = (plan.layers & MapRenderPlanWater) != 0;
    showWays = (plan.layers & MapRenderPlanWays) != 0;
    showSymbols = (plan.layers & MapRenderPlanSymbols) != 0;
    showLabels = (plan.layers & MapRenderPlanLabels) != 0;
    showAnnotations = (plan.layers & MapRenderPlanAnnotations) != 0;
    showRoutes = (plan.layers & MapRenderPlanRoutes) != 0;
    showTokens = (plan.layers & MapRenderPlanTokens) != 0;
    applyFog = plan.applyFog && (plan.layers & MapRenderPlanFog) != 0;
    showGrid = plan.showGrid && (plan.layers & MapRenderPlanGrid) != 0;
    showPoliticalOverlay = plan.showPoliticalOverlay &&
                           (plan.layers & MapRenderPlanPolitical) != 0;
    pixels.assign(static_cast<std::size_t>(width * height), Pixel{74, 96, 53, 255});

    if (showBackground && map.importedReference.enabled &&
        (plan.backgroundExportAssetId != map.importedReference.assetId ||
         plan.backgroundExportProjectRelativePath != map.importedReference.projectRelativePath ||
         plan.backgroundExportContentHash != map.importedReference.contentHash))
    {
        if (error)
            *error = "The map changed after the render preview. Create a new preview.";
        return false;
    }
    if (showBackground && map.importedReference.enabled &&
        (plan.backgroundExportAssetId.empty() ||
         plan.backgroundExportProjectRelativePath.empty() ||
         plan.backgroundExportContentHash.empty()))
    {
        if (error)
            *error = "The managed map image is not available.";
        return false;
    }
    if (showBackground && map.importedReference.enabled &&
        !MapImportedReferencePolicy::isSafeProjectRelativePath(
            plan.backgroundExportProjectRelativePath))
    {
        if (error)
            *error = "The managed map image path is unsafe. Choose the image again to repair it.";
        return false;
    }

    bool importedContentHashMismatch = false;
    const auto importedTransform = MapImageImport::makeNormalizedSamplingTransform(
        map.importedReference.crop, map.importedReference.rotationDegrees, mapViewport);
    const std::filesystem::path importedSourcePath =
        showBackground && map.importedReference.enabled
            ? resolveManagedBackgroundPath(plan.backgroundExportProjectRelativePath)
            : std::filesystem::path{};
    importedBackgroundDrawn = showBackground && map.importedReference.enabled &&
                               drawBackgroundReference(
                                   pixels, width, height, importedSourcePath,
                                   importedTransform, 255,
                                   plan.backgroundExportContentHash,
                                   &importedContentHashMismatch);
    if (showBackground && map.importedReference.enabled && !importedBackgroundDrawn)
    {
        if (error)
            *error = importedContentHashMismatch
                         ? "The original map image no longer matches the imported file."
                         : "The managed map image is missing or cannot be read. Repair the image reference before exporting.";
        return false;
    }
    return true;
}
}
