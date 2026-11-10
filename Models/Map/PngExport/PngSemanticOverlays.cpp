#include "Models/Map/PngExport/PngSemanticOverlays.hpp"
#include "Models/Map/MapGeneratedNamePresentation.hpp"
#include "Models/Map/MapOverviewRenderPolicy.hpp"
#include "Models/Map/MapRoutePresentation.hpp"
#include "Models/Map/PngExport/PngLabelLayout.hpp"
#include "Models/Map/PngExport/PngTextRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace MapPngExportRaster
{
void drawRectOutline(std::vector<Pixel> &pixels, int width, int height,
                     int left, int top, int right, int bottom, int thickness, Pixel color)
{
    if (right <= left || bottom <= top)
        return;
    drawLine(pixels, width, height, left, top, right, top, thickness, color);
    drawLine(pixels, width, height, right, top, right, bottom, thickness, color);
    drawLine(pixels, width, height, right, bottom, left, bottom, thickness, color);
    drawLine(pixels, width, height, left, bottom, left, top, thickness, color);
}

void drawLayeredLine(std::vector<Pixel> &pixels, int width, int height,
                     int x0, int y0, int x1, int y1, int thickness, Pixel color)
{
    const Pixel outline{24, 27, 25, 255};
    drawLine(pixels, width, height, x0, y0, x1, y1, thickness + 3, outline);
    drawLine(pixels, width, height, x0, y0, x1, y1, std::max(1, thickness), color);
}

void drawCircleOutline(std::vector<Pixel> &pixels, int width, int height,
                       int centerX, int centerY, int radius, int thickness, Pixel color)
{
    constexpr int segmentCount = 24;
    constexpr float tau = 6.28318530718f;
    for (int segment = 0; segment < segmentCount; ++segment)
    {
        const float angleA = tau * static_cast<float>(segment) / static_cast<float>(segmentCount);
        const float angleB = tau * static_cast<float>(segment + 1) / static_cast<float>(segmentCount);
        drawLayeredLine(pixels, width, height,
                        centerX + static_cast<int>(std::round(std::cos(angleA) * radius)),
                        centerY + static_cast<int>(std::round(std::sin(angleA) * radius)),
                        centerX + static_cast<int>(std::round(std::cos(angleB) * radius)),
                        centerY + static_cast<int>(std::round(std::sin(angleB) * radius)),
                        thickness, color);
    }
}

void drawPartyStartSymbol(std::vector<Pixel> &pixels, int width, int height,
                          int centerX, int centerY, int extent, int thickness, Pixel color)
{
    const Pixel outline{24, 27, 25, 255};
    const int headRadius = std::max(2, extent / 7);
    const int shoulderY = centerY + headRadius * 2;
    for (int offset : {-headRadius * 3, 0, headRadius * 3})
    {
        fillCircle(pixels, width, height, centerX + offset, centerY - headRadius,
                   headRadius + 2, outline);
        fillCircle(pixels, width, height, centerX + offset, centerY - headRadius,
                   headRadius, color);
        drawLayeredLine(pixels, width, height,
                        centerX + offset, centerY + headRadius,
                        centerX + offset, shoulderY, thickness, color);
    }
    drawLayeredLine(pixels, width, height,
                    centerX - headRadius * 4, shoulderY,
                    centerX + headRadius * 4, shoulderY, thickness, color);
}

void drawOppositionStartSymbol(std::vector<Pixel> &pixels, int width, int height,
                               int centerX, int centerY, int extent, int thickness, Pixel color)
{
    const int arm = std::max(5, extent / 2);
    drawLayeredLine(pixels, width, height, centerX - arm, centerY - arm,
                    centerX + arm, centerY + arm, thickness, color);
    drawLayeredLine(pixels, width, height, centerX + arm, centerY - arm,
                    centerX - arm, centerY + arm, thickness, color);
}

void drawObjectiveSymbol(std::vector<Pixel> &pixels, int width, int height,
                         int centerX, int centerY, int extent, int thickness, Pixel color)
{
    const int radius = std::max(5, extent / 2);
    drawCircleOutline(pixels, width, height, centerX, centerY, radius, thickness, color);
    drawLayeredLine(pixels, width, height, centerX - radius - 3, centerY,
                    centerX + radius + 3, centerY, thickness, color);
    drawLayeredLine(pixels, width, height, centerX, centerY - radius - 3,
                    centerX, centerY + radius + 3, thickness, color);
    fillCircle(pixels, width, height, centerX, centerY, std::max(2, thickness + 1), color);
}

void drawChokepointSymbol(std::vector<Pixel> &pixels, int width, int height,
                          int centerX, int centerY, int extent, int thickness, Pixel color)
{
    const int arm = std::max(5, extent / 2);
    const int waist = std::max(2, extent / 5);
    drawLayeredLine(pixels, width, height, centerX - arm, centerY - arm,
                    centerX - waist, centerY, thickness, color);
    drawLayeredLine(pixels, width, height, centerX - arm, centerY + arm,
                    centerX - waist, centerY, thickness, color);
    drawLayeredLine(pixels, width, height, centerX + arm, centerY - arm,
                    centerX + waist, centerY, thickness, color);
    drawLayeredLine(pixels, width, height, centerX + arm, centerY + arm,
                    centerX + waist, centerY, thickness, color);
}

bool startsWith(const std::string &value, const char *prefix)
{
    return prefix && value.rfind(prefix, 0) == 0;
}

void drawTacticalRegionOverlays(std::vector<Pixel> &pixels, int width, int height,
                                const MapModel &map, int pixelsPerCell, bool playerSafe)
{
    if (map.atlasLevel != MapLevel::Tactical)
        return;

    for (const MapRegionModel &region : map.regions)
    {
        if ((playerSafe && region.gmOnly) || region.width <= 0 || region.height <= 0)
            continue;
        const bool partyStart = startsWith(region.name, "Party Start");
        const bool oppositionStart = startsWith(region.name, "Opposition Start");
        const bool objective = startsWith(region.name, "Objective ");
        const bool chokepoint = startsWith(region.name, "Chokepoint");
        if (!partyStart && !oppositionStart && !objective && !chokepoint)
            continue;

        const int left = std::clamp(region.x * pixelsPerCell, 0, width - 1);
        const int top = std::clamp(region.y * pixelsPerCell, 0, height - 1);
        const int right = std::clamp((region.x + region.width) * pixelsPerCell, 0, width - 1);
        const int bottom = std::clamp((region.y + region.height) * pixelsPerCell, 0, height - 1);
        const int thickness = std::max(2, pixelsPerCell / 10);
        const Pixel color = partyStart ? Pixel{45, 166, 151, 255} :
                            (oppositionStart ? Pixel{183, 75, 67, 255} :
                             (objective ? Pixel{225, 173, 64, 255} : Pixel{170, 128, 72, 255}));
        drawRectOutline(pixels, width, height, left, top, right, bottom, thickness, color);
        if (partyStart || oppositionStart)
            drawRectOutline(pixels, width, height, left + thickness * 2, top + thickness * 2,
                             right - thickness * 2, bottom - thickness * 2, std::max(1, thickness / 2), color);
        const int centerX = (left + right) / 2;
        const int centerY = (top + bottom) / 2;
        const int symbolExtent = std::max(8, std::min(right - left, bottom - top) / 3);
        const int symbolThickness = std::max(2, pixelsPerCell / 9);
        if (partyStart)
            drawPartyStartSymbol(pixels, width, height, centerX, centerY, symbolExtent, symbolThickness, color);
        else if (oppositionStart)
            drawOppositionStartSymbol(pixels, width, height, centerX, centerY, symbolExtent, symbolThickness, color);
        else if (objective)
            drawObjectiveSymbol(pixels, width, height, centerX, centerY, symbolExtent, symbolThickness, color);
        else if (chokepoint)
            drawChokepointSymbol(pixels, width, height, centerX, centerY, symbolExtent, symbolThickness, color);
    }
}

std::vector<MapFeaturePoint> smoothFeaturePoints(const std::vector<MapFeaturePoint> &source)
{
    if (source.size() < 3)
        return source;
    // Three Chaikin passes turn cell-by-cell A* paths into a stable, organic
    // centre line without moving either endpoint away from its semantic anchor.
    std::vector<MapFeaturePoint> result = source;
    for (int pass = 0; pass < 3; ++pass)
    {
        std::vector<MapFeaturePoint> softened;
        softened.reserve(result.size() * 2);
        softened.push_back(result.front());
        for (std::size_t index = 1; index < result.size(); ++index)
        {
            const MapFeaturePoint &a = result[index - 1];
            const MapFeaturePoint &b = result[index];
            softened.push_back({a.x * 0.75f + b.x * 0.25f, a.y * 0.75f + b.y * 0.25f});
            softened.push_back({a.x * 0.25f + b.x * 0.75f, a.y * 0.25f + b.y * 0.75f});
        }
        softened.push_back(result.back());
        result = std::move(softened);
    }
    return result;
}

void drawCompactLabel(std::vector<Pixel> &pixels, int width, int height,
                      int anchorX, int anchorY, const std::string &text,
                      int pixelsPerCell, Pixel accent)
{
    if (text.empty())
        return;
    const PngLabelFont *font = pngLabelFont();
    if (!font)
        return;
    const PngTextMetrics metrics = measurePngText(*font, text, std::clamp(pixelsPerCell * 0.42f, 10.0f, 18.0f));
    const int padding = std::max(3, pixelsPerCell / 10);
    const int plateWidth = metrics.width + padding * 2;
    const int plateHeight = metrics.height + padding;
    const int left = std::clamp(anchorX - plateWidth / 2, 1, std::max(1, width - plateWidth - 1));
    const int top = std::clamp(anchorY - plateHeight - std::max(5, pixelsPerCell / 2), 1,
                               std::max(1, height - plateHeight - 1));
    blendRect(pixels, width, height, left, top, left + plateWidth, top + plateHeight, {22, 23, 21, 220});
    drawRectOutline(pixels, width, height, left, top, left + plateWidth, top + plateHeight, 1, accent);
    drawPngText(pixels, width, height, left + padding, top + padding / 2, *font, metrics, {244, 238, 219, 255});
}

void drawPoliticalBoundaries(std::vector<Pixel> &pixels, int width, int height,
                             const MapModel &map, int pixelsPerCell)
{
    if (map.atlasLevel != MapLevel::World || pixelsPerCell <= 0)
        return;
    const std::vector<gm::map_render::OverviewPoliticalBorderRun> runs =
        gm::map_render::buildOverviewPoliticalBorderRuns(map);
    const int haloThickness = std::clamp(
        static_cast<int>(std::lround(pixelsPerCell * 0.12f)), 2, 4);
    const int inkThickness = std::clamp(
        static_cast<int>(std::lround(pixelsPerCell * 0.045f)), 1, 2);
    constexpr Pixel halo{gm::map_render::kOverviewBorderHalo.r,
                         gm::map_render::kOverviewBorderHalo.g,
                         gm::map_render::kOverviewBorderHalo.b,
                         gm::map_render::kOverviewBorderHalo.a};
    constexpr Pixel ink{gm::map_render::kOverviewBorderInk.r,
                        gm::map_render::kOverviewBorderInk.g,
                        gm::map_render::kOverviewBorderInk.b,
                        gm::map_render::kOverviewBorderInk.a};
    for (const gm::map_render::OverviewPoliticalBorderRun &run : runs)
    {
        const std::vector<gm::map_render::OverviewPoliticalStrokeSegment> dashes =
            gm::map_render::buildOverviewPoliticalDashes(run);
        for (const gm::map_render::OverviewPoliticalStrokeSegment &dash : dashes)
        {
            const int startX = static_cast<int>(std::lround(
                dash.start.x * pixelsPerCell));
            const int startY = static_cast<int>(std::lround(
                dash.start.y * pixelsPerCell));
            const int endX = static_cast<int>(std::lround(
                dash.end.x * pixelsPerCell));
            const int endY = static_cast<int>(std::lround(
                dash.end.y * pixelsPerCell));
            if (halo.a != 0)
                drawCartographicLine(pixels, width, height,
                                      startX, startY, endX, endY,
                                      haloThickness, halo);
            drawCartographicLine(pixels, width, height,
                                  startX, startY, endX, endY,
                                  inkThickness, ink);
        }
    }
}

void drawManualAnnotations(std::vector<Pixel> &pixels, int width, int height,
                           const MapModel &map, int pixelsPerCell, bool showLabels,
                           std::string_view locale)
{
    for (const MapFeatureModel &feature : map.features)
    {
        const int x = std::clamp(feature.x * pixelsPerCell + pixelsPerCell / 2, 0, width - 1);
        const int y = std::clamp(feature.y * pixelsPerCell + pixelsPerCell / 2, 0, height - 1);
        const Pixel color = feature.kind == MapFeatureKind::Trap || feature.kind == MapFeatureKind::Hazard
                                ? Pixel{201, 92, 72, 255}
                                : (feature.kind == MapFeatureKind::SecretDoor
                                       ? Pixel{147, 116, 181, 255}
                                       : Pixel{225, 173, 64, 255});
        const int radius = std::max(4, pixelsPerCell / 5);
        fillCircle(pixels, width, height, x, y, radius + 2, {29, 27, 23, 255});
        fillCircle(pixels, width, height, x, y, radius, color);
        if (showLabels)
            drawCompactLabel(pixels, width, height, x, y,
                             gm::map_presentation::generatedName(feature.name, locale),
                             pixelsPerCell, color);
    }
}

void drawRoutes(std::vector<Pixel> &pixels, int width, int height,
                const MapRenderPlan &plan,
                int pixelsPerCell, float mapOffsetX, float mapOffsetY)
{
    for (const MapRenderPrimitive &primitive : plan.primitives)
    {
        if (primitive.kind != MapRenderPrimitiveKind::Route)
            continue;
        gm::map_route_presentation::Path presentationPath;
        presentationPath.source =
            gm::map_route_presentation::sourceFromAssetRole(
                primitive.assetRole);
        presentationPath.points = primitive.points;
        if (presentationPath.points.size() < 2)
            continue;

        const Pixel color = presentationPath.source ==
                                    gm::map_route_presentation::Source::DirectFallback
                                ? Pixel{224, 136, 70, 235}
                                : Pixel{233, 188, 77, 235};
        const int thickness = std::max(2, pixelsPerCell / 12);
        const bool fallback = presentationPath.isFallback();
        const float dashLength = std::clamp(pixelsPerCell * 0.42f, 4.0f, 12.0f);
        const float gapLength = std::clamp(pixelsPerCell * 0.24f, 3.0f, 8.0f);
        for (std::size_t index = 1; index < presentationPath.points.size(); ++index)
        {
            const MapFeaturePoint &a = presentationPath.points[index - 1];
            const MapFeaturePoint &b = presentationPath.points[index];
            const float startX = (a.x - mapOffsetX) * pixelsPerCell;
            const float startY = (a.y - mapOffsetY) * pixelsPerCell;
            const float endX = (b.x - mapOffsetX) * pixelsPerCell;
            const float endY = (b.y - mapOffsetY) * pixelsPerCell;
            if (!fallback)
            {
                drawLayeredLine(pixels, width, height,
                                static_cast<int>(std::lround(startX)),
                                static_cast<int>(std::lround(startY)),
                                static_cast<int>(std::lround(endX)),
                                static_cast<int>(std::lround(endY)),
                                thickness, color);
                continue;
            }
            const float dx = endX - startX;
            const float dy = endY - startY;
            const float length = std::sqrt(dx * dx + dy * dy);
            if (length <= 0.001f)
                continue;
            for (float offset = 0.0f; offset < length;
                 offset += dashLength + gapLength)
            {
                const float from = offset / length;
                const float to = std::min(length, offset + dashLength) / length;
                drawLayeredLine(
                    pixels, width, height,
                    static_cast<int>(std::lround(startX + dx * from)),
                    static_cast<int>(std::lround(startY + dy * from)),
                    static_cast<int>(std::lround(startX + dx * to)),
                    static_cast<int>(std::lround(startY + dy * to)),
                    thickness, color);
            }
        }
    }
}

void drawTokens(std::vector<Pixel> &pixels, int width, int height,
                const MapModel &map, int pixelsPerCell, bool showLabels)
{
    for (const MapTokenModel &token : map.tokens)
    {
        // Token coordinates identify cells. Keep the CPU exporter on the same
        // cell-centre convention as editor, review, and player display.
        const int x = std::clamp(static_cast<int>(std::lround((token.x + 0.5f) * pixelsPerCell)), 0, width - 1);
        const int y = std::clamp(static_cast<int>(std::lround((token.y + 0.5f) * pixelsPerCell)), 0, height - 1);
        const int radius = std::max(5, static_cast<int>(std::lround(token.size * pixelsPerCell * 0.34f)));
        Pixel color{224, 174, 68, 255};
        if (token.kind == MapTokenKind::Player || token.kind == MapTokenKind::Party)
            color = {52, 171, 159, 255};
        else if (token.kind == MapTokenKind::NPC)
            color = {100, 153, 205, 255};
        else if (token.kind == MapTokenKind::Monster)
            color = {193, 76, 68, 255};
        fillCircle(pixels, width, height, x, y, radius + 3, {20, 22, 22, 255});
        fillCircle(pixels, width, height, x, y, radius, color);
        drawCircleOutline(pixels, width, height, x, y, radius, std::max(1, pixelsPerCell / 16), {239, 229, 204, 255});
        if (showLabels)
            drawCompactLabel(pixels, width, height, x, y, token.name, pixelsPerCell, color);
    }
}


}
