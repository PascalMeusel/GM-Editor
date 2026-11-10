#include "Models/Map/MapPngExportPlanning.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string_view>
#include <utility>

namespace
{
bool pngPointInsideRect(float x, float y, const MapPngExportRect &rect)
{
    return x >= static_cast<float>(rect.x) && y >= static_cast<float>(rect.y) &&
           x < static_cast<float>(rect.x + rect.width) &&
           y < static_cast<float>(rect.y + rect.height);
}

bool pngGeneratedFeatureTouchesRect(const MapGeneratedFeature &feature,
                                    const MapPngExportRect &rect)
{
    if (feature.points.empty())
        return false;
    float minimumX = feature.points.front().x;
    float maximumX = minimumX;
    float minimumY = feature.points.front().y;
    float maximumY = minimumY;
    for (const MapFeaturePoint &point : feature.points)
    {
        minimumX = std::min(minimumX, point.x);
        maximumX = std::max(maximumX, point.x);
        minimumY = std::min(minimumY, point.y);
        maximumY = std::max(maximumY, point.y);
    }
    return maximumX >= static_cast<float>(rect.x) &&
           maximumY >= static_cast<float>(rect.y) &&
           minimumX <= static_cast<float>(rect.x + rect.width) &&
           minimumY <= static_cast<float>(rect.y + rect.height);
}

bool pngSafeFileStem(std::string_view value)
{
    if (value.empty() || value == "." || value == "..")
        return false;
    return std::all_of(value.begin(), value.end(), [](const unsigned char character) {
        return std::isalnum(character) != 0 || character == '-' || character == '_';
    });
}
}

bool pngRectIsValidForMap(const MapPngExportRect &rect, const MapModel &map)
{
    return rect.x >= 0 && rect.y >= 0 && rect.width > 0 && rect.height > 0 &&
           rect.x <= map.width - rect.width && rect.y <= map.height - rect.height;
}

MapNormalizedRect pngNormalizedViewport(const MapModel &map,
                                        const MapPngExportRect &rect)
{
    const float width = static_cast<float>(std::max(1, map.width));
    const float height = static_cast<float>(std::max(1, map.height));
    return {
        static_cast<float>(rect.x) / width,
        static_cast<float>(rect.y) / height,
        static_cast<float>(rect.width) / width,
        static_cast<float>(rect.height) / height};
}

MapModel cropMapForPngExport(const MapModel &source, const MapPngExportRect &rect)
{
    MapModel cropped = source;
    cropped.width = rect.width;
    cropped.height = rect.height;
    cropped.cells.clear();
    cropped.cells.reserve(static_cast<std::size_t>(rect.width * rect.height));
    for (int y = 0; y < rect.height; ++y)
        for (int x = 0; x < rect.width; ++x)
        {
            const MapCellModel *cell = mapCellAt(source, rect.x + x, rect.y + y);
            cropped.cells.push_back(cell ? *cell : MapCellModel{});
        }

    const float offsetX = static_cast<float>(rect.x);
    const float offsetY = static_cast<float>(rect.y);
    cropped.generation.graph.features.clear();
    for (MapGeneratedFeature feature : source.generation.graph.features)
    {
        // The source has already been filtered through the reviewed full-map
        // plan. Linear ink may enter the crop although its authored controls
        // are outside it, so leave clipping to the rasterizer.
        const bool linear = feature.kind == MapGeneratedFeatureKind::Road ||
                            feature.kind == MapGeneratedFeatureKind::Trail ||
                            feature.kind == MapGeneratedFeatureKind::River;
        if (!linear && !pngGeneratedFeatureTouchesRect(feature, rect))
            continue;
        for (MapFeaturePoint &point : feature.points)
        {
            point.x -= offsetX;
            point.y -= offsetY;
        }
        cropped.generation.graph.features.push_back(std::move(feature));
    }

    cropped.tokens.clear();
    for (MapTokenModel token : source.tokens)
    {
        if (!pngPointInsideRect(token.x, token.y, rect))
            continue;
        token.x -= offsetX;
        token.y -= offsetY;
        cropped.tokens.push_back(std::move(token));
    }

    cropped.features.clear();
    for (MapFeatureModel feature : source.features)
    {
        if (feature.x < rect.x || feature.y < rect.y ||
            feature.x >= rect.x + rect.width || feature.y >= rect.y + rect.height)
            continue;
        feature.x -= rect.x;
        feature.y -= rect.y;
        cropped.features.push_back(std::move(feature));
    }

    cropped.regions.clear();
    for (MapRegionModel region : source.regions)
    {
        const int left = std::max(rect.x, region.x);
        const int top = std::max(rect.y, region.y);
        const int right = std::min(rect.x + rect.width, region.x + region.width);
        const int bottom = std::min(rect.y + rect.height, region.y + region.height);
        if (right <= left || bottom <= top)
            continue;
        region.x = left - rect.x;
        region.y = top - rect.y;
        region.width = right - left;
        region.height = bottom - top;
        cropped.regions.push_back(std::move(region));
    }

    cropped.routes.clear();
    for (MapRouteModel route : source.routes)
    {
        const int minimumX = std::min(route.startX, route.endX);
        const int maximumX = std::max(route.startX, route.endX);
        const int minimumY = std::min(route.startY, route.endY);
        const int maximumY = std::max(route.startY, route.endY);
        if (maximumX < rect.x || maximumY < rect.y ||
            minimumX >= rect.x + rect.width || minimumY >= rect.y + rect.height)
            continue;
        route.startX -= rect.x;
        route.startY -= rect.y;
        route.endX -= rect.x;
        route.endY -= rect.y;
        cropped.routes.push_back(std::move(route));
    }

    for (MapGenerationConstraint &constraint : cropped.generation.recipe.constraints)
    {
        constraint.x -= offsetX;
        constraint.y -= offsetY;
    }

    // Keep managed-image calibration immutable. Viewport exporters pass their
    // normalized full-map rectangle to the sampler; changing the image crop
    // here would rotate around the tile center and select different pixels.
    if (cropped.spatialReference.calibrated)
    {
        const float sourceWidth = static_cast<float>(std::max(1, source.width));
        const float sourceHeight = static_cast<float>(std::max(1, source.height));
        cropped.spatialReference.originX += cropped.spatialReference.extentWidth *
                                            static_cast<float>(rect.x) / sourceWidth;
        cropped.spatialReference.originY += cropped.spatialReference.extentHeight *
                                            static_cast<float>(rect.y) / sourceHeight;
        cropped.spatialReference.extentWidth *= static_cast<float>(rect.width) / sourceWidth;
        cropped.spatialReference.extentHeight *= static_cast<float>(rect.height) / sourceHeight;
    }
    return cropped;
}

int mapPngPixelsPerCellForPreset(const MapModel &map,
                                 MapPngExportSizePreset preset,
                                 int customPixelsPerCell)
{
    if (preset == MapPngExportSizePreset::Tactical100PixelsPerCell)
        return 100;
    if (preset == MapPngExportSizePreset::Custom)
        return customPixelsPerCell;
    if (map.width <= 0 || map.height <= 0)
        return customPixelsPerCell;

    const int targetWidth = preset == MapPngExportSizePreset::UltraHd4K ? 3840 : 1920;
    const int targetHeight = preset == MapPngExportSizePreset::UltraHd4K ? 2160 : 1080;
    const int fit = std::min(targetWidth / map.width, targetHeight / map.height);
    return std::clamp(fit, kMapPngMinimumPixelsPerCell, kMapPngMaximumPixelsPerCell);
}

MapPngExportSizing calculateMapPngExportSizing(const MapModel &map,
                                               int pixelsPerCell)
{
    MapPngExportSizing sizing;
    sizing.pixelsPerCell = pixelsPerCell;
    if (map.width <= 0 || map.height <= 0)
    {
        sizing.error = "Map dimensions are invalid.";
        return sizing;
    }
    if (pixelsPerCell < kMapPngMinimumPixelsPerCell ||
        pixelsPerCell > kMapPngMaximumPixelsPerCell)
    {
        sizing.error = "PNG export detail must be between 8 and 128 pixels per cell.";
        return sizing;
    }

    const std::int64_t width = static_cast<std::int64_t>(map.width) * pixelsPerCell;
    const std::int64_t height = static_cast<std::int64_t>(map.height) * pixelsPerCell;
    if (width <= 0 || height <= 0 ||
        width > kMapPngMaximumExportEdge || height > kMapPngMaximumExportEdge)
    {
        sizing.error = "PNG export would exceed the maximum edge of 16384 pixels.";
        return sizing;
    }

    sizing.widthPixels = static_cast<int>(width);
    sizing.heightPixels = static_cast<int>(height);
    sizing.uncompressedBytes = static_cast<std::uint64_t>(width) *
                               static_cast<std::uint64_t>(height) * 4ull;
    sizing.valid = true;
    return sizing;
}

MapPngExportSizing calculateMapPngExportSizing(const MapPngExportRect &rect,
                                               int pixelsPerCell)
{
    MapModel dimensionsOnly;
    dimensionsOnly.width = rect.width;
    dimensionsOnly.height = rect.height;
    return calculateMapPngExportSizing(dimensionsOnly, pixelsPerCell);
}

MapPngTiledExportPlan planMapPngTiledExport(const MapModel &map,
                                            int pixelsPerCell,
                                            std::string fileStem,
                                            int maximumTileEdge,
                                            int overlapCells)
{
    MapPngTiledExportPlan plan;
    plan.pixelsPerCell = pixelsPerCell;
    plan.overlapCells = overlapCells;
    if (map.width <= 0 || map.height <= 0)
    {
        plan.error = "Map dimensions are invalid.";
        return plan;
    }
    if (pixelsPerCell < kMapPngMinimumPixelsPerCell ||
        pixelsPerCell > kMapPngMaximumPixelsPerCell)
    {
        plan.error = "PNG export detail must be between 8 and 128 pixels per cell.";
        return plan;
    }
    if (!pngSafeFileStem(fileStem))
    {
        plan.error = "The tiled PNG file name is invalid.";
        return plan;
    }
    if (maximumTileEdge < pixelsPerCell || maximumTileEdge > kMapPngMaximumExportEdge)
    {
        plan.error = "The tiled PNG edge limit is invalid.";
        return plan;
    }
    if (overlapCells < 0)
    {
        plan.error = "The tiled PNG overlap must not be negative.";
        return plan;
    }
    const int maximumRenderCells = maximumTileEdge / pixelsPerCell;
    if (maximumRenderCells <= overlapCells * 2)
    {
        plan.error = "The tiled PNG edge limit leaves no room for map content.";
        return plan;
    }

    const std::int64_t fullWidth = static_cast<std::int64_t>(map.width) * pixelsPerCell;
    const std::int64_t fullHeight = static_cast<std::int64_t>(map.height) * pixelsPerCell;
    if (fullWidth > std::numeric_limits<int>::max() ||
        fullHeight > std::numeric_limits<int>::max())
    {
        plan.error = "The requested tiled PNG dimensions are too large.";
        return plan;
    }
    plan.fullWidthPixels = static_cast<int>(fullWidth);
    plan.fullHeightPixels = static_cast<int>(fullHeight);

    const int contentCells = std::max(1, maximumRenderCells - overlapCells * 2);
    plan.columns = (map.width + contentCells - 1) / contentCells;
    plan.rows = (map.height + contentCells - 1) / contentCells;
    plan.tiles.reserve(static_cast<std::size_t>(plan.columns * plan.rows));
    for (int row = 0; row < plan.rows; ++row)
    {
        for (int column = 0; column < plan.columns; ++column)
        {
            MapPngExportTile tile;
            tile.column = column;
            tile.row = row;
            tile.content.x = column * contentCells;
            tile.content.y = row * contentCells;
            tile.content.width = std::min(contentCells, map.width - tile.content.x);
            tile.content.height = std::min(contentCells, map.height - tile.content.y);
            const int renderRight = std::min(map.width,
                tile.content.x + tile.content.width + overlapCells);
            const int renderBottom = std::min(map.height,
                tile.content.y + tile.content.height + overlapCells);
            tile.render.x = std::max(0, tile.content.x - overlapCells);
            tile.render.y = std::max(0, tile.content.y - overlapCells);
            tile.render.width = renderRight - tile.render.x;
            tile.render.height = renderBottom - tile.render.y;
            tile.widthPixels = tile.render.width * pixelsPerCell;
            tile.heightPixels = tile.render.height * pixelsPerCell;
            std::ostringstream name;
            name << fileStem << "_r" << std::setw(3) << std::setfill('0') << row
                 << "_c" << std::setw(3) << std::setfill('0') << column << ".png";
            tile.fileName = name.str();
            plan.totalUncompressedBytes += static_cast<std::uint64_t>(tile.widthPixels) *
                                           static_cast<std::uint64_t>(tile.heightPixels) * 4ull;
            plan.tiles.push_back(std::move(tile));
        }
    }
    plan.valid = !plan.tiles.empty();
    return plan;
}

bool mapPermitsPngExport(const MapModel &map,
                         MapRenderAudience audience,
                         std::string *error)
{
    if (audience != MapRenderAudience::Player ||
        map.visibility == MapVisibility::PlayersVisible)
        return true;
    if (error)
        *error = "Player-safe PNG export requires a map visible to players. Save the map and choose Review player view first.";
    return false;
}
