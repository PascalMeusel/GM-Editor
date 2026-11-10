#include "Models/Map/PngExport/PngWorldSurface.hpp"
#include "Models/Map/MapStyleRegistry.hpp"
#include "Models/Map/MapOverviewRenderPolicy.hpp"
#include "Models/Map/PngExport/PngRasterAssets.hpp"

#include <algorithm>
#include <cmath>

namespace MapPngExportRaster
{
void applyWorldPainterlySurface(std::vector<Pixel> &pixels,
                                int width,
                                int height,
                                const MapModel &map)
{
    if (map.atlasLevel != MapLevel::World ||
        map.generation.recipe.styleVersion < 3 || width <= 0 || height <= 0)
        return;
    const std::string resolvedSnapshotId = map.generation.recipe.styleSnapshotId.empty()
                                               ? mapStyleCurrentProductionSnapshotId()
                                               : map.generation.recipe.styleSnapshotId;
    const MapStylePackedRuntimeAsset *surfaceAsset =
        resolveMapStyleProductionAssetForSnapshot(
            resolvedSnapshotId, MapLevel::World,
            "world_surface_modulation", "watercolor", 0, 0, 0,
            static_cast<float>(width) /
                static_cast<float>(std::max(1, map.width * 24)));
    const RasterAsset *surface = loadRasterAsset(surfaceAsset);
    if (!surface || surface->width <= 0 || surface->height <= 0)
        return;

    const auto mixChannel = [](std::uint8_t left, std::uint8_t right,
                               float amount) {
        return static_cast<std::uint8_t>(std::clamp(
            static_cast<int>(std::lround(
                static_cast<float>(left) +
                (static_cast<float>(right) - static_cast<float>(left)) * amount)),
            0, 255));
    };
    const auto mixPixel = [&](const Pixel &left, const Pixel &right,
                              float amount) {
        return Pixel{mixChannel(left.r, right.r, amount),
                     mixChannel(left.g, right.g, amount),
                     mixChannel(left.b, right.b, amount),
                     mixChannel(left.a, right.a, amount)};
    };
    for (int y = 0; y < height; ++y)
    {
        const float sourceY = (static_cast<float>(y) + 0.5f) *
                                  static_cast<float>(surface->height) /
                                  static_cast<float>(height) -
                              0.5f;
        const int y0 = std::clamp(static_cast<int>(std::floor(sourceY)),
                                  0, surface->height - 1);
        const int y1 = std::min(surface->height - 1, y0 + 1);
        const float ty = std::clamp(sourceY - std::floor(sourceY), 0.0f, 1.0f);
        for (int x = 0; x < width; ++x)
        {
            const float sourceX = (static_cast<float>(x) + 0.5f) *
                                      static_cast<float>(surface->width) /
                                      static_cast<float>(width) -
                                  0.5f;
            const int x0 = std::clamp(static_cast<int>(std::floor(sourceX)),
                                      0, surface->width - 1);
            const int x1 = std::min(surface->width - 1, x0 + 1);
            const float tx = std::clamp(sourceX - std::floor(sourceX), 0.0f, 1.0f);
            const Pixel top = mixPixel(
                surface->pixels[static_cast<std::size_t>(y0 * surface->width + x0)],
                surface->pixels[static_cast<std::size_t>(y0 * surface->width + x1)], tx);
            const Pixel bottom = mixPixel(
                surface->pixels[static_cast<std::size_t>(y1 * surface->width + x0)],
                surface->pixels[static_cast<std::size_t>(y1 * surface->width + x1)], tx);
            const Pixel sample = mixPixel(top, bottom, ty);
            Pixel &destination = pixels[static_cast<std::size_t>(y * width + x)];
            destination = alphaBlend(
                destination, sample,
                gm::map_render::kOverviewPainterlySurfaceOpacity);
        }
    }
}


}
