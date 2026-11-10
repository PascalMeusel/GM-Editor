#pragma once

#include "Models/Map/MapModels.hpp"
#include "Views/Editor/Widgets/Map/MapVisualRoutes.hpp"
#include "imgui.h"

#include <string>

namespace gm::map_viewer_detail
{
struct CanvasFrame final
{
    MapModel *map = nullptr;
    ImDrawList *drawList = nullptr;
    ImVec2 origin;
    float cellSize = 1.0f;
    float mapPixelWidth = 0.0f;
    float mapPixelHeight = 0.0f;
    MapVisibleCellBounds visibleCells;
    bool showTacticalOverlay = false;
    bool continuousRendering = false;
    bool forceTacticalGrid = false;
    bool importedBackgroundDrawn = false;
    std::string renderLocale;
};
}
