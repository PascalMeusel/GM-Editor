#pragma once

#include "Models/Session/PlayerSafeDataFilter.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

// Camera presets are derived only from the explicitly player-visible map
// selected for the local display. They never resize malformed grids or change
// fog/publication state, so callers can safely offer them before a review.
namespace LocalPlayerDisplayCameraPresets
{
inline bool hasExactMapGeometry(const MapModel &map)
{
    if (map.width <= 0 || map.height <= 0)
        return false;
    const std::size_t width = static_cast<std::size_t>(map.width);
    const std::size_t height = static_cast<std::size_t>(map.height);
    return height <= (std::numeric_limits<std::size_t>::max)() / width &&
           map.cells.size() == width * height;
}

inline LocalPlayerDisplayCameraState wholeMap(const MapModel &map)
{
    (void)map;
    return {0.5f, 0.5f, 1.0f, false};
}

inline std::optional<LocalPlayerDisplayCameraState> revealedArea(const MapModel &map)
{
    if (!hasExactMapGeometry(map))
        return std::nullopt;

    int minimumX = map.width;
    int minimumY = map.height;
    int maximumX = -1;
    int maximumY = -1;
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const MapCellModel &cell = map.cells[static_cast<std::size_t>(y * map.width + x)];
            if (cell.fogged)
                continue;
            minimumX = std::min(minimumX, x);
            minimumY = std::min(minimumY, y);
            maximumX = std::max(maximumX, x);
            maximumY = std::max(maximumY, y);
        }
    }
    if (maximumX < minimumX || maximumY < minimumY)
        return std::nullopt;

    constexpr float padding = 0.10f;
    float left = static_cast<float>(minimumX);
    float top = static_cast<float>(minimumY);
    float right = static_cast<float>(maximumX + 1);
    float bottom = static_cast<float>(maximumY + 1);
    const float revealedWidth = right - left;
    const float revealedHeight = bottom - top;
    left = std::max(0.0f, left - revealedWidth * padding);
    top = std::max(0.0f, top - revealedHeight * padding);
    right = std::min(static_cast<float>(map.width), right + revealedWidth * padding);
    bottom = std::min(static_cast<float>(map.height), bottom + revealedHeight * padding);
    const float framedWidth = right - left;
    const float framedHeight = bottom - top;
    if (!std::isfinite(framedWidth) || !std::isfinite(framedHeight) || framedWidth <= 0.0f || framedHeight <= 0.0f)
        return std::nullopt;

    LocalPlayerDisplayCameraState camera;
    camera.normalizedCenterX = std::clamp((left + right) * 0.5f / static_cast<float>(map.width), 0.0f, 1.0f);
    camera.normalizedCenterY = std::clamp((top + bottom) * 0.5f / static_cast<float>(map.height), 0.0f, 1.0f);
    camera.zoom = std::clamp(std::min(static_cast<float>(map.width) / framedWidth,
                                      static_cast<float>(map.height) / framedHeight),
                              0.5f, 8.0f);
    camera.followGmCamera = false;
    return camera;
}

inline std::optional<LocalPlayerDisplayCameraState> wholeMapForPublicDisplay(const Campaign &campaign)
{
    const MapModel *map = findPlayerSafePublishedMap(campaign, campaign.localPlayerDisplay.publicMapId);
    if (map == nullptr || !hasExactMapGeometry(*map))
        return std::nullopt;
    return wholeMap(*map);
}

inline std::optional<LocalPlayerDisplayCameraState> revealedAreaForPublicDisplay(const Campaign &campaign)
{
    const MapModel *map = findPlayerSafePublishedMap(campaign, campaign.localPlayerDisplay.publicMapId);
    return map == nullptr ? std::nullopt : revealedArea(*map);
}
} // namespace LocalPlayerDisplayCameraPresets
