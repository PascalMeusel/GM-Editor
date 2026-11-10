#include "Models/Map/PngExport/PngRenderSession.hpp"
#include "Models/Map/MapIllustratedAtlas.hpp"
#include "Models/Map/MapFogTexture.hpp"
#include "Models/Map/MapOverviewRenderPolicy.hpp"
#include "Models/Map/PngExport/PngLabelRenderer.hpp"
#include "Models/Map/PngExport/PngSemanticOverlays.hpp"

#define STB_IMAGE_WRITE_STATIC
#if defined(_WIN32)
#define STBIW_WINDOWS_UTF8
#endif
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "extern/glfw/deps/stb_image_write.h"

#include <algorithm>

namespace MapPngExportRaster
{
bool PngRenderSession::writeOutput()
{
        if (showPoliticalOverlay)
            drawPoliticalBoundaries(pixels, width, height, map, pixelsPerCell);

        if (showLabels)
            drawGeneratedMapLabels(pixels, width, height, map, pixelsPerCell,
                                   playerSafe, plan.locale, plan.labelInkMask);

        if (showAnnotations)
            drawManualAnnotations(pixels, width, height, map, pixelsPerCell,
                                  showLabels, plan.locale);
        if (showRoutes)
            drawRoutes(pixels, width, height, plan, pixelsPerCell,
                       mapOffsetX, mapOffsetY);
        if (showTokens)
            drawTokens(pixels, width, height, map, pixelsPerCell, showLabels);

        // Apply fog after every semantic/render layer for both continuous and
        // legacy raster maps. Previously the mask lived inside the continuous
        // branch, allowing unrevealed legacy cells into player-safe PNGs.
        if (applyFog)
        {
            for (int y = 0; y < map.height; ++y)
            {
                for (int x = 0; x < map.width; ++x)
                {
                    const MapCellModel *cell = mapCellAt(map, x, y);
                    if (cell && !cell->fogged)
                        continue;
                    for(int py=y*pixelsPerCell;py<(y+1)*pixelsPerCell;++py)
                        for(int px=x*pixelsPerCell;px<(x+1)*pixelsPerCell;++px)
                        {
                            const auto color=gm::map_render::fogTextureAt(
                                mapOffsetX+(px+.5f)/pixelsPerCell,mapOffsetY+(py+.5f)/pixelsPerCell);
                            pixels[static_cast<std::size_t>(py)*width+px]={color[0],color[1],color[2],255};
                        }
                }
            }
        }

        if (showGrid)
        {
            for (int x = 0; x <= map.width; ++x)
                drawLine(pixels, width, height, x * pixelsPerCell, 0, x * pixelsPerCell, height - 1, 1, {48, 54, 48, 120});
            for (int y = 0; y <= map.height; ++y)
                drawLine(pixels, width, height, 0, y * pixelsPerCell, width - 1, y * pixelsPerCell, 1, {48, 54, 48, 120});
        }

        if (!path.parent_path().empty())
            std::filesystem::create_directories(path.parent_path());
        // The vendored stb encoder's level 8 hash search can become
        // disproportionately slow on large, nearly uniform terrain fields.
        // Level 4 keeps exports interactive while retaining compact PNGs.
        static const bool compressionConfigured = [] { stbi_write_png_compression_level = 4; return true; }();
        (void)compressionConfigured;
        const std::string utf8OutputPath = path.u8string();
        if (stbi_write_png(utf8OutputPath.c_str(), width, height, 4, pixels.data(), width * static_cast<int>(sizeof(Pixel))) == 0)
        {
            if (error)
                *error = "PNG writer rejected the output path.";
            return false;
        }
        return true;
}
}
