#include "Models/Map/PngExport/PngTerrainAssets.hpp"
#include "Models/Map/MapOverviewRenderPolicy.hpp"
#include "Models/Map/MapStyleRegistry.hpp"
#include "Models/Map/PngExport/PngRasterAssets.hpp"
#include "Models/Map/PngExport/PngVisualPolicy.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace MapPngExportRaster
{
void drawOverviewMountainRanges(std::vector<Pixel> &pixels, int width, int height,
                                const MapModel &map, int pixelsPerCell)
{
    // World and Region both use the connected macro-range painting. World
    // stamps are derived from feature spines or coherent mountain cells and
    // are never repeated once per cell.
    const std::string &v32SnapshotId = mapStyleV32ProductionSnapshotId();
    const std::string &v33SnapshotId = mapStyleV33ProductionSnapshotId();
    const std::string resolvedSnapshotId = map.generation.recipe.styleSnapshotId.empty()
                                               ? mapStyleCurrentProductionSnapshotId()
                                               : map.generation.recipe.styleSnapshotId;
    const bool overviewLevel = map.atlasLevel == MapLevel::World ||
                               map.atlasLevel == MapLevel::Region;
    if (!overviewLevel || pixelsPerCell <= 0 ||
        map.generation.recipe.styleVersion < 3 ||
        ((v32SnapshotId.empty() || resolvedSnapshotId != v32SnapshotId) &&
         (v33SnapshotId.empty() || resolvedSnapshotId != v33SnapshotId)) ||
        !mapStyleProductionSnapshotAvailable(resolvedSnapshotId))
        return;

    const std::vector<gm::map_render::OverviewMountainStamp> stamps =
        gm::map_render::buildOverviewMountainStamps(map);
    for (const gm::map_render::OverviewMountainStamp &stamp : stamps)
    {
        const bool v33AssetSnapshot = resolvedSnapshotId == v33SnapshotId;
        const gm::map_render::OverviewRenderColor mountainTint =
            v33AssetSnapshot ? gm::map_render::kOverviewV33MountainTint
                             : gm::map_render::kOverviewMountainTint;
        const MapStylePackedRuntimeAsset *asset = resolveMapStyleProductionAssetForSnapshot(
            resolvedSnapshotId, map.atlasLevel,
            "landscape_stamp", "connected_ridge", stamp.anchorX, stamp.anchorY,
            2421 + stamp.sectionIndex);
        drawRotatedRasterAsset(
            pixels, width, height,
            stamp.centerX * pixelsPerCell,
            stamp.centerY * pixelsPerCell,
            pixelsPerCell * stamp.lengthCells * gm::map_render::kOverviewMountainLengthScale,
            pixelsPerCell * stamp.widthCells *
                (v33AssetSnapshot
                     ? gm::map_render::kOverviewV33MountainWidthScale
                     : gm::map_render::kOverviewMountainWidthScale),
            stamp.angleRadians - (asset ? asset->sourceAxisRadians : 0.0f),
            asset,
            mountainTint.a,
            {mountainTint.r,
             mountainTint.g,
             mountainTint.b,
             255});
    }
}

const RasterAsset *terrainRasterAsset(const std::string &snapshotId, MapLevel level, MapTileKind kind,
                                     int sectorX, int sectorY, std::uint32_t seed)
{
    static thread_local std::unordered_map<std::string, const RasterAsset *> cache;
    const std::string key = snapshotId + "|" + std::to_string(static_cast<int>(level)) + "|" +
                            std::to_string(static_cast<int>(kind)) + "|" + std::to_string(sectorX) + "|" +
                            std::to_string(sectorY) + "|" + std::to_string(seed);
    const auto found = cache.find(key);
    if (found != cache.end())
        return found->second;
    const MapStylePackedRuntimeAsset *asset = resolveMapStyleProductionAssetForSnapshot(
        snapshotId, level, "terrain_material", terrainSemanticRole(level, kind), sectorX, sectorY,
        static_cast<int>(kind) * 97 + static_cast<int>(seed & 0x7fffu));
    const RasterAsset *raster = loadRasterAsset(asset);
    cache[key] = raster;
    return raster;
}

Pixel sampleTerrainAsset(const std::string &snapshotId, MapLevel level, MapTileKind kind,
                         int pixelX, int pixelY, int pixelsPerCell, std::uint32_t seed)
{
    const int repeatPixels = std::max(1, static_cast<int>(std::round(mapRenderScaleProfile(level).materialCellsPerRepeat * pixelsPerCell)));
    const int sectorPixels = repeatPixels * 3;
    const auto floorDivision = [](int value, int divisor) {
        int quotient = value / divisor;
        if (value < 0 && value % divisor != 0)
            --quotient;
        return quotient;
    };
    const int sectorX = floorDivision(pixelX, sectorPixels);
    const int sectorY = floorDivision(pixelY, sectorPixels);
    // Mirror-repeat eliminates hard seams even when an accepted source image
    // has a small residual edge mismatch.  The previous modulo repeat exposed
    // a conspicuous square ocean grid at world scale.
    const auto mirroredCoordinate = [](int coordinate, int period) {
        const int doubled = std::max(2, period * 2);
        int local = coordinate % doubled;
        if (local < 0)
            local += doubled;
        return local >= period ? doubled - local - 1 : local;
    };
    const int localX = mirroredCoordinate(pixelX, repeatPixels);
    const int localY = mirroredCoordinate(pixelY, repeatPixels);
    const auto sample = [&](int assetX, int assetY) {
        const RasterAsset *raster = terrainRasterAsset(snapshotId, level, kind, assetX, assetY, seed);
        if (!raster || raster->width <= 0 || raster->height <= 0)
            return terrainColor(kind);
        // Accepted source paintings deliberately keep a little framing room at
        // their edges. Sampling that frame as terrain produced a dark stripe at
        // every mirror turnaround, even though the pixels were technically
        // continuous. Crop a small safe interior before repeating the material.
        const int marginX = raster->width >= 64 ? std::max(1, raster->width / 24) : 0;
        const int marginY = raster->height >= 64 ? std::max(1, raster->height / 24) : 0;
        const int sampleWidth = std::max(1, raster->width - marginX * 2);
        const int sampleHeight = std::max(1, raster->height - marginY * 2);
        const int x = std::clamp(marginX + localX * sampleWidth / repeatPixels, marginX, raster->width - marginX - 1);
        const int y = std::clamp(marginY + localY * sampleHeight / repeatPixels, marginY, raster->height - marginY - 1);
        return raster->pixels[static_cast<std::size_t>(y * raster->width + x)];
    };
    const auto mix = [](Pixel left, Pixel right, float amount) {
        const auto channel = [&](std::uint8_t a, std::uint8_t b) {
            return static_cast<std::uint8_t>(std::clamp(static_cast<int>(std::lround(a + (b - a) * amount)), 0, 255));
        };
        return Pixel{channel(left.r, right.r), channel(left.g, right.g), channel(left.b, right.b), channel(left.a, right.a)};
    };
    const float rawX = static_cast<float>(pixelX - sectorX * sectorPixels) / static_cast<float>(sectorPixels);
    const float rawY = static_cast<float>(pixelY - sectorY * sectorPixels) / static_cast<float>(sectorPixels);
    const float amountX = rawX * rawX * (3.0f - 2.0f * rawX);
    const float amountY = rawY * rawY * (3.0f - 2.0f * rawY);
    return mix(mix(sample(sectorX, sectorY), sample(sectorX + 1, sectorY), amountX),
               mix(sample(sectorX, sectorY + 1), sample(sectorX + 1, sectorY + 1), amountX), amountY);
}


}
