#pragma once

#include <algorithm>

namespace gm::ux
{
struct ResponsiveTwoActionLayout
{
    bool inlineActions = false;
    float firstWidth = 1.0f;
    float secondWidth = 1.0f;
};

struct ResponsiveWindowLayout
{
    float width = 1.0f;
    float height = 1.0f;
    float minimumWidth = 1.0f;
    float minimumHeight = 1.0f;
    float maximumWidth = 1.0f;
    float maximumHeight = 1.0f;
};

// Fits a requested window into a stable portion of the current work area. The
// minimum is reduced as a last resort so even unusually small viewports cannot
// produce contradictory ImGui size constraints.
inline ResponsiveWindowLayout responsiveWindowLayout(float workAreaWidth,
                                                       float workAreaHeight,
                                                       float preferredWidth,
                                                       float preferredHeight,
                                                       float minimumWidth,
                                                       float minimumHeight,
                                                       float workAreaFraction = 0.90f)
{
    const float safeFraction = std::clamp(workAreaFraction, 0.10f, 1.0f);
    const float maximumWidth = std::max(1.0f, workAreaWidth * safeFraction);
    const float maximumHeight = std::max(1.0f, workAreaHeight * safeFraction);
    const float fittedMinimumWidth = std::min(maximumWidth, std::max(1.0f, minimumWidth));
    const float fittedMinimumHeight = std::min(maximumHeight, std::max(1.0f, minimumHeight));
    return {
        std::clamp(std::max(1.0f, preferredWidth), fittedMinimumWidth, maximumWidth),
        std::clamp(std::max(1.0f, preferredHeight), fittedMinimumHeight, maximumHeight),
        fittedMinimumWidth,
        fittedMinimumHeight,
        maximumWidth,
        maximumHeight};
}

// Keeps translated action labels visible: use a row only when both measured
// minimum widths fit, otherwise give each action a full-width row.
inline ResponsiveTwoActionLayout responsiveTwoActionLayout(float availableWidth,
                                                           float spacing,
                                                           float firstMinimumWidth,
                                                           float secondMinimumWidth)
{
    const float safeAvailable = std::max(1.0f, availableWidth);
    const float safeSpacing = std::max(0.0f, spacing);
    const float safeFirst = std::max(1.0f, firstMinimumWidth);
    const float safeSecond = std::max(1.0f, secondMinimumWidth);
    if (safeFirst + safeSpacing + safeSecond <= safeAvailable)
        return {true, safeFirst, safeSecond};
    return {false, safeAvailable, safeAvailable};
}

// Returns the largest readable grid width up to maxColumns. This is intended
// for panels that contain nested forms or tables, where merely fitting the
// heading text would still create unusably narrow content columns.
inline int responsiveColumnCount(float availableWidth,
                                 float spacing,
                                 float minimumColumnWidth,
                                 int maxColumns)
{
    const float safeAvailable = std::max(1.0f, availableWidth);
    const float safeSpacing = std::max(0.0f, spacing);
    const float safeMinimum = std::max(1.0f, minimumColumnWidth);
    const int safeMaximum = std::max(1, maxColumns);
    const int fitting = std::max(1, static_cast<int>((safeAvailable + safeSpacing) /
                                                      (safeMinimum + safeSpacing)));
    return std::clamp(fitting, 1, safeMaximum);
}
} // namespace gm::ux
