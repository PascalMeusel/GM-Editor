#pragma once

namespace UiTooltipPolicy
{
inline bool shouldShow(
    bool itemActive,
    bool pointerInsideApplication,
    bool itemHovered)
{
    if (itemActive || !pointerInsideApplication)
        return false;

    return itemHovered;
}
}
