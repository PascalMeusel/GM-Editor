#pragma once

#include "Models/Map/MapRenderPlan.hpp"
#include "Models/Map/MapPngExport.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>

namespace gm::mapui
{
struct MapCreatorPreviewFrame
{
    bool valid = false;
    float cellSize = 0.0f;
    float mapX = 0.0f;
    float mapY = 0.0f;
    float clipX = 0.0f;
    float clipY = 0.0f;
    float clipWidth = 0.0f;
    float clipHeight = 0.0f;
};

// Fit a view of the original map. Geometry, consent and imported-image UVs
// remain in full-map coordinates; only the screen origin and clip change.
inline MapCreatorPreviewFrame mapCreatorPreviewFrame(
    const MapModel &map, const MapPngExportRect &view,
    float width, float height, float gutter)
{
    MapCreatorPreviewFrame frame;
    if (map.width <= 0 || map.height <= 0 || view.x < 0 || view.y < 0 ||
        view.width <= 0 || view.height <= 0 ||
        static_cast<std::int64_t>(view.x) + view.width > map.width ||
        static_cast<std::int64_t>(view.y) + view.height > map.height ||
        width <= 0.0f || height <= 0.0f)
        return frame;
    frame.cellSize = std::max(0.01f, std::min(
        std::max(1.0f, width - gutter * 2.0f) / static_cast<float>(view.width),
        std::max(1.0f, height - gutter * 2.0f) / static_cast<float>(view.height)));
    frame.clipWidth = frame.cellSize * view.width;
    frame.clipHeight = frame.cellSize * view.height;
    frame.clipX = (width - frame.clipWidth) * 0.5f;
    frame.clipY = (height - frame.clipHeight) * 0.5f;
    frame.mapX = frame.clipX - frame.cellSize * view.x;
    frame.mapY = frame.clipY - frame.cellSize * view.y;
    frame.valid = true;
    return frame;
}

// Generator designs are a Game Master presentation of the exact map that will
// be opened in the editor.  Keeping the layer contract in one small, testable
// function prevents the preview from quietly growing its own renderer or asset
// selection rules again.
constexpr std::uint32_t mapCreatorPreviewLayers =
    MapRenderPlanBackground |
    MapRenderPlanTerrain |
    MapRenderPlanWater |
    MapRenderPlanWays |
    MapRenderPlanSymbols |
    MapRenderPlanPolitical |
    MapRenderPlanLabels;

// Pure layout decisions keep the minimum-window behavior testable without an
// ImGui frame. TextLineHeight already contains DPI/font scaling, so the same
// rules naturally become more conservative at 150% UI scale.
inline int mapCreatorWorkflowColumnCount(float availableWidth,
                                         float widestButtonWidth)
{
    const float buttonWidth = std::max(1.0f, widestButtonWidth);
    constexpr float spacing = 8.0f;
    const int fitting = static_cast<int>(std::floor(
        (std::max(0.0f, availableWidth) + spacing) / (buttonWidth + spacing)));
    if (fitting >= 5)
        return 5;
    // Four columns would leave the fifth step alone on a second row. Three
    // columns form a calmer 3 + 2 layout and use less vertical space than the
    // former 2 + 2 + 1 layout at 1280 x 720 with large German labels.
    if (fitting >= 3)
        return 3;
    return fitting >= 2 ? 2 : 1;
}

inline bool mapCreatorUsesStackedWorkspace(float availableWidth,
                                           float textLineHeight)
{
    // A side-by-side workspace needs room for a usable control rail and a map
    // canvas. At larger text scales the canvas allowance grows as well, which
    // avoids squeezing the 1280x720 / 150% layout into two unreadable columns.
    const float minimumCanvasWidth = std::max(520.0f, std::max(1.0f, textLineHeight) * 36.0f);
    return availableWidth < 340.0f + minimumCanvasWidth + 16.0f;
}

inline int mapCreatorCandidateColumnCount(float availableWidth,
                                          float textLineHeight,
                                          int candidateCount,
                                          bool stackedWorkspace = false)
{
    if (candidateCount <= 0)
        return 1;
    // A stacked workspace gives the design selector the full workspace width;
    // it is therefore often the best place for two comparison cards. Do not
    // collapse it to one column merely because the controls and preview are
    // vertically arranged. The slightly larger scaled minimum keeps German
    // labels and the validity state readable at 150%.
    const float scaledMinimum = std::max(1.0f, textLineHeight) *
                                (stackedWorkspace ? 15.0f : 14.0f);
    const float minimumCardWidth = std::max(300.0f, scaledMinimum);
    const float spacing = 8.0f;
    const int fitting = static_cast<int>(std::floor(
        (std::max(0.0f, availableWidth) + spacing) / (minimumCardWidth + spacing)));
    return std::clamp(fitting, 1, candidateCount);
}

inline bool mapCreatorShowsSeparateControlRail(bool stackedWorkspace,
                                               bool designStep)
{
    // On the design step the full-width preview already contains all relevant
    // actions. Repeating a small "design review" rail above it made the map and
    // the primary action fall below the fold in minimum-height layouts.
    return !(stackedWorkspace && designStep);
}

inline int mapCreatorPairedActionColumnCount(float availableWidth,
                                             float firstLabelWidth,
                                             float secondLabelWidth,
                                             float framePaddingX,
                                             float cellPaddingX)
{
    const float requiredWidth = std::max(0.0f, firstLabelWidth) +
                                std::max(0.0f, secondLabelWidth) +
                                std::max(0.0f, framePaddingX) * 4.0f +
                                std::max(0.0f, cellPaddingX) * 4.0f;
    return availableWidth >= requiredWidth ? 2 : 1;
}

inline float mapCreatorPreviewViewportWidth(float availableWidth,
                                            float viewportHeight,
                                            float mapAspect)
{
    const float safeWidth = std::max(160.0f, availableWidth);
    const float safeHeight = std::max(180.0f, viewportHeight);
    const float safeAspect = std::max(0.1f, mapAspect);
    // Keep the framed viewport close to the map's aspect ratio. A 4:3 world
    // should not sit inside a nearly ultrawide dark box with most of the frame
    // empty; zooming still scrolls inside this bounded viewport.
    const float aspectWidth = safeHeight * safeAspect + 24.0f;
    return std::min(safeWidth, std::max(320.0f, aspectWidth));
}

inline float mapCreatorPreviewZoomAfterStep(float currentZoom, int direction)
{
    constexpr float minimumZoom = 1.0f;
    constexpr float maximumZoom = 4.0f;
    constexpr float step = 0.25f;
    return std::clamp(currentZoom + static_cast<float>(direction) * step,
                      minimumZoom, maximumZoom);
}

inline float mapCreatorPreviewZoomAfterWheel(float currentZoom, float wheelDelta)
{
    if (std::abs(wheelDelta) < 0.001f)
        return std::clamp(currentZoom, 1.0f, 4.0f);
    return mapCreatorPreviewZoomAfterStep(currentZoom, wheelDelta > 0.0f ? 1 : -1);
}

inline float mapCreatorPreviewScrollForPointerAnchor(float previousScroll,
                                                     float pointerInsideViewport,
                                                     float previousContentExtent,
                                                     float nextContentExtent)
{
    if (previousContentExtent <= 0.0f || nextContentExtent <= 0.0f)
        return 0.0f;
    const float normalizedAnchor = std::clamp(
        (std::max(0.0f, previousScroll) + std::max(0.0f, pointerInsideViewport)) /
            previousContentExtent,
        0.0f, 1.0f);
    return std::max(0.0f,
                    normalizedAnchor * nextContentExtent -
                        std::max(0.0f, pointerInsideViewport));
}

inline bool mapCreatorPreviewLabelsNeedZoom(int visibleLabelCount,
                                            float fittedMapWidth,
                                            float zoom)
{
    if (visibleLabelCount <= 1 || zoom > 1.25f)
        return false;
    // World labels need room for a short name plus its marker. This is a
    // presentation hint, not a validity score: it tells the GM to zoom before
    // judging names when a fitted overview is necessarily dense.
    constexpr float comfortablePixelsPerLabel = 104.0f;
    return static_cast<float>(visibleLabelCount) * comfortablePixelsPerLabel >
           std::max(1.0f, fittedMapWidth);
}

inline float mapCreatorPreviewContentGutter(bool showLabels,
                                            float previewWidth,
                                            float previewHeight)
{
    if (!showLabels)
        return 4.0f;
    // Reserve label bleed around the map instead of clipping names whose
    // anchors sit near a coast or map edge. Small cards retain proportionally
    // more map space; large selected previews cap the gutter at 36 pixels.
    return std::clamp(std::min(previewWidth, previewHeight) * 0.07f,
                      10.0f, 36.0f);
}

inline MapRenderPlanRequest mapCreatorPreviewRenderPlanRequest(std::string locale)
{
    MapRenderPlanRequest request;
    request.audience = MapRenderAudience::GameMaster;
    request.locale = std::move(locale);
    request.layers = mapCreatorPreviewLayers;
    request.applyFog = false;
    request.showPoliticalOverlay = false;
    request.showGrid = false;
    return request;
}

inline MapRenderPlanRequest mapCreatorCandidateThumbnailRenderPlanRequest(
    std::string locale)
{
    MapRenderPlanRequest request =
        mapCreatorPreviewRenderPlanRequest(std::move(locale));
    // Comparison cards communicate landmass, waterways, routes and important
    // symbols. Names and borders need the selected interactive view's space;
    // drawing them into a 120-pixel thumbnail creates false visual defects.
    request.layers &= ~static_cast<std::uint32_t>(MapRenderPlanLabels);
    request.layers &= ~static_cast<std::uint32_t>(MapRenderPlanPolitical);
    request.showPoliticalOverlay = false;
    return request;
}

inline MapRenderPlan compileMapCreatorPreviewRenderPlan(const MapModel &map,
                                                        std::string locale)
{
    return compileMapRenderPlan(map, mapCreatorPreviewRenderPlanRequest(std::move(locale)));
}
} // namespace gm::mapui
