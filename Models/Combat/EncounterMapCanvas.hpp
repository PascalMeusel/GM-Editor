#pragma once
#include <algorithm>
#include <cmath>

inline constexpr float encounterCanvasMinimumZoom = 0.0625f;
inline constexpr float encounterCanvasMaximumZoom = 2.6f;

inline float encounterCanvasFitZoom(int width, int height, float viewportWidth, float viewportHeight)
{
    if (width <= 0 || height <= 0 || !std::isfinite(viewportWidth) || !std::isfinite(viewportHeight))
        return 1.0f;
    return std::clamp(std::min(std::max(1.0f, viewportWidth - 16.0f) / (32.0f * width),
                               std::max(1.0f, viewportHeight - 16.0f) / (32.0f * height)),
                      encounterCanvasMinimumZoom, encounterCanvasMaximumZoom);
}

inline float encounterCanvasZoomScroll(float scroll, float viewport, float previousZoom, float nextZoom)
{
    const float center = std::max(0.0f, viewport) * 0.5f;
    return std::max(0.0f, (scroll + center) * nextZoom / std::max(previousZoom, 0.001f) - center);
}
