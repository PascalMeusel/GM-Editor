#pragma once

#include "Models/Map/MapModels.hpp"

#include <algorithm>
#include <cmath>

namespace gm::mapui
{
constexpr float mapImageMinimumVisiblePercent = 1.0f;

struct MapImageCropMarginsPercent
{
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;

    float visibleWidth() const
    {
        return std::max(0.0f, 100.0f - left - right);
    }

    float visibleHeight() const
    {
        return std::max(0.0f, 100.0f - top - bottom);
    }
};

inline float mapImageInspectorFiniteOr(float value, float fallback)
{
    return std::isfinite(value) ? value : fallback;
}

inline MapImageCropMarginsPercent mapImageCropMarginsPercent(
    float cropX, float cropY, float cropWidth, float cropHeight)
{
    const float left = std::clamp(mapImageInspectorFiniteOr(cropX, 0.0f), 0.0f, 0.99f);
    const float top = std::clamp(mapImageInspectorFiniteOr(cropY, 0.0f), 0.0f, 0.99f);
    const float width = std::clamp(mapImageInspectorFiniteOr(cropWidth, 1.0f),
                                   0.01f, 1.0f - left);
    const float height = std::clamp(mapImageInspectorFiniteOr(cropHeight, 1.0f),
                                    0.01f, 1.0f - top);
    MapImageCropMarginsPercent result;
    result.left = left * 100.0f;
    result.top = top * 100.0f;
    result.right = std::max(0.0f, 1.0f - left - width) * 100.0f;
    result.bottom = std::max(0.0f, 1.0f - top - height) * 100.0f;
    return result;
}

inline MapNormalizedRect mapImageCropFromMarginsPercent(
    MapImageCropMarginsPercent margins)
{
    auto normalizePair = [](float &leading, float &trailing) {
        leading = std::clamp(mapImageInspectorFiniteOr(leading, 0.0f), 0.0f,
                             100.0f - mapImageMinimumVisiblePercent);
        trailing = std::clamp(mapImageInspectorFiniteOr(trailing, 0.0f), 0.0f,
                              100.0f - mapImageMinimumVisiblePercent);
        const float maximumHidden = 100.0f - mapImageMinimumVisiblePercent;
        const float hidden = leading + trailing;
        if (hidden > maximumHidden)
        {
            const float scale = maximumHidden / hidden;
            leading *= scale;
            trailing *= scale;
        }
    };
    normalizePair(margins.left, margins.right);
    normalizePair(margins.top, margins.bottom);

    MapNormalizedRect crop;
    crop.x = margins.left / 100.0f;
    crop.y = margins.top / 100.0f;
    crop.width = margins.visibleWidth() / 100.0f;
    crop.height = margins.visibleHeight() / 100.0f;
    return crop;
}

inline void mapImageInspectorUseWholeImage(float &cropX, float &cropY,
                                            float &cropWidth, float &cropHeight)
{
    cropX = 0.0f;
    cropY = 0.0f;
    cropWidth = 1.0f;
    cropHeight = 1.0f;
}

inline float mapImageInspectorRotateQuarterTurn(float degrees, int clockwiseQuarterTurns)
{
    float rotated = mapImageInspectorFiniteOr(degrees, 0.0f) +
                    static_cast<float>(clockwiseQuarterTurns) * 90.0f;
    rotated = std::fmod(rotated, 360.0f);
    if (rotated > 180.0f)
        rotated -= 360.0f;
    else if (rotated <= -180.0f)
        rotated += 360.0f;
    return std::abs(rotated) < 0.0001f ? 0.0f : rotated;
}

inline bool mapImageInspectorCropIsValid(float cropX, float cropY,
                                         float cropWidth, float cropHeight)
{
    return std::isfinite(cropX) && std::isfinite(cropY) &&
           std::isfinite(cropWidth) && std::isfinite(cropHeight) &&
           cropX >= 0.0f && cropY >= 0.0f &&
           cropWidth >= 0.01f && cropHeight >= 0.01f &&
           cropX + cropWidth <= 1.0001f &&
           cropY + cropHeight <= 1.0001f;
}

inline bool mapImageInspectorCalibrationIsValid(bool calibrated,
                                                float extentWidth,
                                                float extentHeight,
                                                int distanceUnitIndex)
{
    if (!calibrated)
        return true;
    return std::isfinite(extentWidth) && std::isfinite(extentHeight) &&
           extentWidth > 0.0f && extentHeight > 0.0f &&
           distanceUnitIndex >= static_cast<int>(MapDistanceUnit::Kilometers) &&
           distanceUnitIndex <= static_cast<int>(MapDistanceUnit::Feet);
}

inline bool mapImageInspectorHasPendingChanges(
    const MapImportedReference &reference,
    const MapSpatialReference &spatial,
    float cropX, float cropY, float cropWidth, float cropHeight,
    float rotationDegrees, bool calibrated,
    float extentWidth, float extentHeight, int distanceUnitIndex)
{
    const auto different = [](float lhs, float rhs) {
        return !std::isfinite(lhs) || !std::isfinite(rhs) ||
               std::abs(lhs - rhs) > 0.0001f;
    };
    return different(reference.crop.x, cropX) ||
           different(reference.crop.y, cropY) ||
           different(reference.crop.width, cropWidth) ||
           different(reference.crop.height, cropHeight) ||
           different(reference.rotationDegrees, rotationDegrees) ||
           spatial.calibrated != calibrated ||
           different(spatial.extentWidth, extentWidth) ||
           different(spatial.extentHeight, extentHeight) ||
           static_cast<int>(spatial.unit) != distanceUnitIndex;
}

inline int mapImageInspectorFieldColumnCount(float availableWidth,
                                             float textLineHeight)
{
    const float minimumTwoColumnWidth =
        std::max(520.0f, std::max(1.0f, textLineHeight) * 30.0f);
    return availableWidth >= minimumTwoColumnWidth ? 2 : 1;
}
} // namespace gm::mapui
