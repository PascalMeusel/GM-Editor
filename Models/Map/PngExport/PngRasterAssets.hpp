#pragma once

#include "Models/Map/MapImageImportService.hpp"
#include "Models/Map/MapModels.hpp"
#include "Models/Map/MapStyleRegistry.hpp"
#include "Models/Map/PngExport/PngRasterCanvas.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace MapPngExportRaster
{
struct RasterAsset
{
    int width = 0;
    int height = 0;
    std::vector<Pixel> pixels;
};

const RasterAsset *loadRasterAsset(const MapStylePackedRuntimeAsset *asset);
std::filesystem::path resolveBackgroundPath(const std::string &storedPath);
std::filesystem::path resolveManagedBackgroundPath(const std::string &storedPath);
std::string pngFileContentHash(const std::filesystem::path &path);
const RasterAsset *loadRasterPath(const std::filesystem::path &path,
                                  const std::string &expectedContentHash = {},
                                  bool *contentHashMismatch = nullptr);
bool drawBackgroundReference(std::vector<Pixel> &pixels, int width, int height,
                             const std::filesystem::path &resolvedPath,
                             const MapImageImport::NormalizedSamplingTransform &transform,
                             std::uint8_t opacity,
                             const std::string &expectedContentHash = {},
                             bool *contentHashMismatch = nullptr);
void drawRasterAsset(std::vector<Pixel> &pixels, int width, int height,
                     int left, int top, int right, int bottom,
                     const MapStylePackedRuntimeAsset *asset,
                     std::uint8_t opacity = 255,
                     Pixel tint = {255, 255, 255, 255});
void drawRasterAssetPreservingAspect(std::vector<Pixel> &pixels, int width, int height,
                                     int centerX, int centerY, int nominalExtent,
                                     const MapStylePackedRuntimeAsset *asset,
                                     std::uint8_t opacity = 255);
void drawRotatedRasterAsset(std::vector<Pixel> &pixels, int width, int height,
                            float centerX, float centerY, float targetLength, float targetWidth,
                            float angleRadians, const MapStylePackedRuntimeAsset *asset,
                            std::uint8_t opacity = 255,
                            Pixel tint = {255, 255, 255, 255});
int stableMapVariant(int x, int y, int salt, int count);
bool isVisualStampAnchor(const MapModel &map, int x, int y,
                         MapTileKind kind, int radius, int salt);
}
