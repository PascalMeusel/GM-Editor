#pragma once
#include "Views/Editor/Widgets/UiAdaptiveFields.hpp"
#include "BattlefieldCreatorView.hpp"
#include "Controllers/Editor/Widgets/BattlefieldCreator/BattlefieldCreatorController.hpp"
#include "Models/Map/MapAuthoring.hpp"
#include "Views/Editor/Widgets/Map/MapTileVisuals.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"
#include "misc/cpp/imgui_stdlib.h"
#include "imgui.h"
#include "Views/Editor/Widgets/UiTaskLayout.hpp"
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
template <typename... Args>
std::string formatLocalizedUi(const char *format, Args... args)
{
    const std::string localizedFormat = localizedUiLabel(format);
    const int required = std::snprintf(nullptr, 0, localizedFormat.c_str(), args...);
    if (required <= 0)
        return localizedFormat;

    std::vector<char> buffer(static_cast<std::size_t>(required) + 1);
    std::snprintf(buffer.data(), buffer.size(), localizedFormat.c_str(), args...);
    return std::string(buffer.data(), static_cast<std::size_t>(required));
}

const char *battlefieldTerrainToolLabel(BattlefieldTerrainTool tool)
{
    switch (tool)
    {
    case BattlefieldTerrainTool::Inspect:
        return "Inspect Terrain";
    case BattlefieldTerrainTool::Paint:
        return "Paint Terrain";
    case BattlefieldTerrainTool::Erase:
        return "Erase Terrain";
    }
    return "Terrain";
}

const char *battlefieldTerrainToolNextAction(BattlefieldTerrainTool tool)
{
    switch (tool)
    {
    case BattlefieldTerrainTool::Inspect:
        return "Hover a cell to inspect it. Pick Paint or Erase to edit.";
    case BattlefieldTerrainTool::Paint:
        return "Left drag paints the selected terrain. Right click cancels.";
    case BattlefieldTerrainTool::Erase:
        return "Left drag clears terrain to Empty. Right click cancels.";
    }
    return "Choose a terrain tool.";
}

std::string localizedEdgeMaskLabel(int edgeMask)
{
    const int mask = normalizeMapEdgeMask(edgeMask);
    std::string label;
    const auto append = [&](const char *direction) {
        if (!label.empty())
            label += " / ";
        label += localizedUiLabel(direction);
    };
    if ((mask & 1) != 0) append("North Edge");
    if ((mask & 2) != 0) append("East Edge");
    if ((mask & 4) != 0) append("South Edge");
    if ((mask & 8) != 0) append("West Edge");
    return label.empty() ? localizedUiLabel("None") : label;
}

ImU32 colorWithAlpha(ImU32 color, float alpha)
{
    ImVec4 value = ImGui::ColorConvertU32ToFloat4(color);
    value.w = alpha;
    return ImGui::ColorConvertFloat4ToU32(value);
}

void drawTerrainSwatch(MapTileKind terrain, const ImVec2 &size)
{
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const ImVec2 max(min.x + size.x, min.y + size.y);
    drawMapTile(ImGui::GetWindowDrawList(), min, max, terrain, 0, true);
    ImGui::GetWindowDrawList()->AddRect(min, max, IM_COL32(215, 224, 230, 190), 3.0f, 0, 1.5f);
    ImGui::Dummy(size);
}
}
