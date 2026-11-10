#include "Models/Map/PngExport/PngWorldSymbols.hpp"
#include "Models/Map/MapIllustratedAtlas.hpp"
#include "Models/Map/MapOverviewRenderPolicy.hpp"
#include "Models/Map/PngExport/PngRasterCanvas.hpp"

#include <algorithm>
#include <cmath>

namespace MapPngExportRaster
{
void drawWorldForestPattern(std::vector<Pixel> &pixels, int width, int height,
                            const MapModel &map, int pixelsPerCell)
{
    if (pixelsPerCell < 5)
        return;
    const Pixel ink{gm::map_render::kOverviewForestInk.r,
                    gm::map_render::kOverviewForestInk.g,
                    gm::map_render::kOverviewForestInk.b,
                    gm::map_render::kOverviewForestInk.a};
    const Pixel highlight{gm::map_render::kOverviewForestHighlight.r,
                          gm::map_render::kOverviewForestHighlight.g,
                          gm::map_render::kOverviewForestHighlight.b,
                          gm::map_render::kOverviewForestHighlight.a};
    for (const auto &glyph : gm::map_render::buildOverviewForestGlyphs(map))
    {
        const float centerX = glyph.center.x * pixelsPerCell;
        const float centerY = glyph.center.y * pixelsPerCell;
        const float size = glyph.sizeCells * pixelsPerCell;
        const int line = std::max(1, static_cast<int>(std::lround(pixelsPerCell * 0.045f)));
        const float crowns[][2] = {{centerX - size * 0.38f, centerY},
                                    {centerX + size * 0.35f, centerY + size * 0.06f},
                                    {centerX, centerY - size * 0.29f}};
        drawCartographicLine(pixels, width, height, static_cast<int>(centerX),
                              static_cast<int>(centerY + size * 0.2f),
                              static_cast<int>(centerX), static_cast<int>(centerY + size * 0.64f), line, ink);
        const int radius = std::max(1, static_cast<int>(std::lround(size * 0.45f)));
        for (const auto &crown : crowns)
            blendCircle(pixels, width, height, static_cast<int>(std::lround(crown[0])),
                         static_cast<int>(std::lround(crown[1])), radius + line, ink);
        for (const auto &crown : crowns)
            blendCircle(pixels, width, height, static_cast<int>(std::lround(crown[0])),
                         static_cast<int>(std::lround(crown[1])), radius, {112, 148, 96, 245});
        blendCircle(pixels, width, height, static_cast<int>(centerX - size * 0.1f),
                     static_cast<int>(centerY - size * 0.37f),
                     std::max(1, static_cast<int>(size * 0.21f)), highlight);
    }
}

void drawWorldOverviewRidges(std::vector<Pixel> &pixels, int width, int height,
                             const MapModel &map, int pixelsPerCell)
{
    if ((map.atlasLevel != MapLevel::World && !gm::map_render::usesIllustratedAtlas(map)) || pixelsPerCell <= 0)
        return;
    const Pixel shadow{gm::map_render::kOverviewRidgeShadow.r,
                       gm::map_render::kOverviewRidgeShadow.g,
                       gm::map_render::kOverviewRidgeShadow.b,
                       gm::map_render::kOverviewRidgeShadow.a};
    const Pixel ink{gm::map_render::kOverviewRidgeInk.r,
                    gm::map_render::kOverviewRidgeInk.g,
                    gm::map_render::kOverviewRidgeInk.b,
                    gm::map_render::kOverviewRidgeInk.a};
    const Pixel highlight{gm::map_render::kOverviewRidgeHighlight.r,
                          gm::map_render::kOverviewRidgeHighlight.g,
                          gm::map_render::kOverviewRidgeHighlight.b,
                          gm::map_render::kOverviewRidgeHighlight.a};
    const std::vector<gm::map_render::OverviewRidgePath> ridges =
        gm::map_render::buildOverviewRidgePaths(map);
    for (const gm::map_render::OverviewRidgePath &ridge : ridges)
    {
        for (const gm::map_render::OverviewRidgeMark &mark : ridge.marks)
        {
            const int leftX = static_cast<int>(std::lround(mark.leftBase.x * pixelsPerCell));
            const int leftY = static_cast<int>(std::lround(mark.leftBase.y * pixelsPerCell));
            const int peakX = static_cast<int>(std::lround(mark.peak.x * pixelsPerCell));
            const int peakY = static_cast<int>(std::lround(mark.peak.y * pixelsPerCell));
            const int rightX = static_cast<int>(std::lround(mark.rightBase.x * pixelsPerCell));
            const int rightY = static_cast<int>(std::lround(mark.rightBase.y * pixelsPerCell));
            const int shadeX = static_cast<int>(std::lround(mark.shadeEnd.x * pixelsPerCell));
            const int shadeY = static_cast<int>(std::lround(mark.shadeEnd.y * pixelsPerCell));
            const int secondaryLeftX = static_cast<int>(std::lround(
                mark.secondaryLeftBase.x * pixelsPerCell));
            const int secondaryLeftY = static_cast<int>(std::lround(
                mark.secondaryLeftBase.y * pixelsPerCell));
            const int secondaryPeakX = static_cast<int>(std::lround(
                mark.secondaryPeak.x * pixelsPerCell));
            const int secondaryPeakY = static_cast<int>(std::lround(
                mark.secondaryPeak.y * pixelsPerCell));
            const int secondaryRightX = static_cast<int>(std::lround(
                mark.secondaryRightBase.x * pixelsPerCell));
            const int secondaryRightY = static_cast<int>(std::lround(
                mark.secondaryRightBase.y * pixelsPerCell));
            const int secondaryShadeX = static_cast<int>(std::lround(
                mark.secondaryShadeEnd.x * pixelsPerCell));
            const int secondaryShadeY = static_cast<int>(std::lround(
                mark.secondaryShadeEnd.y * pixelsPerCell));
            const int thickness = std::max(1, static_cast<int>(std::lround(
                pixelsPerCell * (0.045f + mark.emphasis * 0.018f))));
            const auto drawPeak = [&](int peakLeftX, int peakLeftY,
                                      int peakTopX, int peakTopY,
                                      int peakRightX, int peakRightY,
                                      int divisionX, int divisionY,
                                      float thicknessScale) {
                blendTriangle(pixels, width, height,
                              peakLeftX, peakLeftY, peakTopX, peakTopY,
                              divisionX, divisionY, {99, 91, 70, 205});
                blendTriangle(pixels, width, height,
                              divisionX, divisionY, peakTopX, peakTopY,
                              peakRightX, peakRightY, {196, 183, 144, 230});
                const int peakThickness = std::max(
                    1, static_cast<int>(std::lround(thickness * thicknessScale)));
                drawCartographicLine(pixels, width, height,
                                      peakLeftX, peakLeftY, peakTopX, peakTopY,
                                      peakThickness, ink);
                drawCartographicLine(pixels, width, height,
                                      peakTopX, peakTopY, peakRightX, peakRightY,
                                      peakThickness, highlight);
                drawCartographicLine(pixels, width, height,
                                      peakTopX, peakTopY, divisionX, divisionY,
                                      peakThickness, shadow);
            };
            drawPeak(secondaryLeftX, secondaryLeftY,
                     secondaryPeakX, secondaryPeakY,
                     secondaryRightX, secondaryRightY,
                     secondaryShadeX, secondaryShadeY, 0.82f);
            const int shoulderLX = static_cast<int>(std::lround(mark.leftShoulder.x * pixelsPerCell));
            const int shoulderLY = static_cast<int>(std::lround(mark.leftShoulder.y * pixelsPerCell));
            const int shoulderRX = static_cast<int>(std::lround(mark.rightShoulder.x * pixelsPerCell));
            const int shoulderRY = static_cast<int>(std::lround(mark.rightShoulder.y * pixelsPerCell));
            blendTriangle(pixels, width, height, leftX, leftY, shoulderLX, shoulderLY, shadeX, shadeY, {99, 91, 70, 205});
            blendTriangle(pixels, width, height, shoulderLX, shoulderLY, peakX, peakY, shadeX, shadeY, {99, 91, 70, 205});
            blendTriangle(pixels, width, height, shadeX, shadeY, peakX, peakY, shoulderRX, shoulderRY, {196, 183, 144, 230});
            blendTriangle(pixels, width, height, shadeX, shadeY, shoulderRX, shoulderRY, rightX, rightY, {196, 183, 144, 230});
            drawCartographicLine(pixels, width, height, leftX, leftY, shoulderLX, shoulderLY, thickness, ink);
            drawCartographicLine(pixels, width, height, shoulderLX, shoulderLY, peakX, peakY, thickness, ink);
            drawCartographicLine(pixels, width, height, peakX, peakY, shoulderRX, shoulderRY, thickness, highlight);
            drawCartographicLine(pixels, width, height, shoulderRX, shoulderRY, rightX, rightY, thickness, highlight);
            drawCartographicLine(pixels, width, height, peakX, peakY, shadeX, shadeY, thickness, shadow);
            drawCartographicLine(pixels, width, height, shoulderLX, shoulderLY, shadeX, shadeY, 1, shadow);
        }
    }
}

void drawWorldSettlementGlyph(std::vector<Pixel> &pixels, int width, int height,
                              int centerX, int centerY, int pixelsPerCell,
                              std::string_view semanticRole)
{
    const gm::map_render::OverviewSettlementGlyphKind kind =
        gm::map_render::overviewSettlementGlyphKind(semanticRole);
    const gm::map_render::OverviewSettlementGlyphStyle style =
        gm::map_render::overviewSettlementGlyphStyle(kind);
    const Pixel ink{gm::map_render::kOverviewSettlementInk.r,
                    gm::map_render::kOverviewSettlementInk.g,
                    gm::map_render::kOverviewSettlementInk.b,
                    gm::map_render::kOverviewSettlementInk.a};
    const Pixel paper{gm::map_render::kOverviewSettlementPaper.r,
                      gm::map_render::kOverviewSettlementPaper.g,
                      gm::map_render::kOverviewSettlementPaper.b,
                      gm::map_render::kOverviewSettlementPaper.a};
    const Pixel accent{gm::map_render::kOverviewSettlementAccent.r,
                       gm::map_render::kOverviewSettlementAccent.g,
                       gm::map_render::kOverviewSettlementAccent.b,
                       gm::map_render::kOverviewSettlementAccent.a};
    const int outerRadius = std::max(3, static_cast<int>(std::lround(
        pixelsPerCell * style.outerRadiusCells)));
    const int innerRadius = std::max(2, static_cast<int>(std::lround(
        pixelsPerCell * style.innerRadiusCells)));
    const int line = std::max(1, static_cast<int>(std::lround(
        pixelsPerCell * 0.075f)));
    blendCircle(pixels, width, height, centerX, centerY,
                outerRadius + std::max(1, line / 2), ink);
    blendCircle(pixels, width, height, centerX, centerY, outerRadius, paper);
    drawCartographicCircle(pixels, width, height, centerX, centerY,
                           outerRadius, line, ink);

    if (kind == gm::map_render::OverviewSettlementGlyphKind::Town)
    {
        drawCartographicCircle(pixels, width, height, centerX, centerY,
                               innerRadius, 1, ink);
        blendCircle(pixels, width, height, centerX, centerY,
                    std::max(1, static_cast<int>(std::lround(pixelsPerCell * 0.095f))),
                    accent);
        return;
    }
    if (kind == gm::map_render::OverviewSettlementGlyphKind::City)
    {
        drawCartographicCircle(pixels, width, height, centerX, centerY,
                               innerRadius, line, ink);
        blendCircle(pixels, width, height, centerX, centerY,
                    std::max(2, static_cast<int>(std::lround(pixelsPerCell * 0.14f))),
                    accent);
        return;
    }

    drawCartographicCircle(pixels, width, height, centerX, centerY,
                           innerRadius, line, ink);
    constexpr float diagonal = 0.70710678118f;
    constexpr float directions[][2] = {
        {1.0f, 0.0f}, {diagonal, diagonal}, {0.0f, 1.0f},
        {-diagonal, diagonal}, {-1.0f, 0.0f}, {-diagonal, -diagonal},
        {0.0f, -1.0f}, {diagonal, -diagonal}};
    for (const auto &direction : directions)
    {
        const int endX = centerX + static_cast<int>(std::lround(
            direction[0] * innerRadius * 0.88f));
        const int endY = centerY + static_cast<int>(std::lround(
            direction[1] * innerRadius * 0.88f));
        drawCartographicLine(pixels, width, height, centerX, centerY,
                              endX, endY, line + 1, ink);
        drawCartographicLine(pixels, width, height, centerX, centerY,
                              endX, endY, std::max(1, line - 1), accent);
    }
    blendCircle(pixels, width, height, centerX, centerY,
                std::max(2, static_cast<int>(std::lround(pixelsPerCell * 0.15f))),
                ink);
    blendCircle(pixels, width, height, centerX, centerY,
                std::max(1, static_cast<int>(std::lround(pixelsPerCell * 0.075f))),
                accent);
}


}
