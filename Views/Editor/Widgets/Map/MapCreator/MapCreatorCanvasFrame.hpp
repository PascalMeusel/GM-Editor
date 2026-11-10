#pragma once

#include "Models/Map/MapModels.hpp"
#include "Views/Editor/Widgets/Map/MapVisualRoutes.hpp"
#include "imgui.h"

#include <cstdint>
#include <string>

namespace gm::map_creator_view_detail
{
struct CanvasFrame final
{
    MapModel *map = nullptr;
    const MapModel *canvasSource = nullptr;
    ImDrawList *drawList = nullptr;
    ImVec2 origin;
    ImVec2 canvasOrigin;
    float cellSize = 1.0f;
    MapVisibleCellBounds visibleCells;
    bool showTacticalOverlay = false;
    bool showEditorGrid = false;
    bool continuousRendering = false;
    bool importedBackgroundDrawn = false;
    std::string renderLocale;
    std::uint64_t canvasContentVersion = 0;

    bool pointIsVisible(float x, float y, float padding) const
    {
        return visibleCells.maxX >= visibleCells.minX && visibleCells.maxY >= visibleCells.minY &&
               x + padding >= static_cast<float>(visibleCells.minX) &&
               x - padding <= static_cast<float>(visibleCells.maxX + 1) &&
               y + padding >= static_cast<float>(visibleCells.minY) &&
               y - padding <= static_cast<float>(visibleCells.maxY + 1);
    }
};
}
