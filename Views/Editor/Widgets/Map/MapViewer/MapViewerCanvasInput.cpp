#include "Views/Editor/Widgets/Map/MapViewer/MapViewerViewDetails.hpp"
#include "Views/Editor/Widgets/Map/MapViewer/MapViewerViewInternal.hpp"

using namespace gm::map_viewer_detail;

#include "Views/Editor/Widgets/Map/MapViewer/MapViewerCanvasFrame.hpp"

void MapViewerView::handleMapCanvasInput(const CanvasFrame &frame)
{
    MapModel *map = frame.map;
    ImDrawList *drawList = frame.drawList;
    const ImVec2 origin = frame.origin;
    const float cellSize = frame.cellSize;
    const float mapPixelWidth = frame.mapPixelWidth;
    const float mapPixelHeight = frame.mapPixelHeight;
    const MapVisibleCellBounds visibleCells = frame.visibleCells;
    const bool showTacticalOverlay = frame.showTacticalOverlay;
    const bool continuousRendering = frame.continuousRendering;
    const bool forceTacticalGrid = frame.forceTacticalGrid;
    const bool importedBackgroundDrawn = frame.importedBackgroundDrawn;
    const std::string &renderLocale = frame.renderLocale;
    ImGui::SetCursorScreenPos(origin);
    ImGui::InvisibleButton("MapViewerCanvasHitbox", ImVec2(mapPixelWidth, mapPixelHeight), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    const auto mouseCell = [&]() {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        return std::pair<int, int>{
            std::clamp(static_cast<int>((mouse.x - origin.x) / cellSize), 0, std::max(0, map->width - 1)),
            std::clamp(static_cast<int>((mouse.y - origin.y) / cellSize), 0, std::max(0, map->height - 1))};
    };
    const ImVec2 mapMouse = ImGui::GetIO().MousePos;
    const bool mouseInsideMap = mapMouse.x >= origin.x && mapMouse.y >= origin.y &&
                                mapMouse.x < origin.x + mapPixelWidth &&
                                mapMouse.y < origin.y + mapPixelHeight;
    bool fogMode = _controller._interactionMode == MapViewerInteractionMode::RevealFog ||
                   _controller._interactionMode == MapViewerInteractionMode::HideFog;
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        ImGui::IsKeyPressed(ImGuiKey_Escape) &&
        (fogMode || _controller._fogStroke.active()))
    {
        _controller.cancelFogStroke();
        _controller.setInteractionMode(MapViewerInteractionMode::InspectSelect);
        fogMode = false;
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
        _controller._interactionMode == MapViewerInteractionMode::InspectSelect)
    {
        const auto [x, y] = mouseCell();
        const float normalizedX = (static_cast<float>(x) + 0.5f) / static_cast<float>(std::max(1, map->width));
        const float normalizedY = (static_cast<float>(y) + 0.5f) / static_cast<float>(std::max(1, map->height));
        for (const MapModel &child : _controller._currentCampaign.maps)
        {
            const MapNormalizedRect &rect = child.atlasPlacement.parentRect;
            if (MapAtlasService::effectiveParentId(child) == map->metadata.id && !child.atlasPlacement.parentId.empty() &&
                normalizedX >= rect.x && normalizedY >= rect.y && normalizedX <= rect.x + rect.width && normalizedY <= rect.y + rect.height)
            {
                _controller.selectMapById(child.metadata.id);
                break;
            }
        }
    }
    else if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        const auto [x, y] = mouseCell();
        if (_controller._interactionMode == MapViewerInteractionMode::MoveParty)
            _controller.previewPartyMoveToCell(x, y);
        else if (_controller._interactionMode == MapViewerInteractionMode::SelectDetailArea)
            _controller.beginDetailSelection(x, y);
        else if (fogMode)
            _controller.beginFogStroke(x, y);
        else if (_controller._interactionMode == MapViewerInteractionMode::PingPlayers)
            _controller.pingPlayersAtCell(x, y);
        else
            _controller.selectCell(x, y);
    }
    if (_controller._interactionMode == MapViewerInteractionMode::SelectDetailArea && _controller._detailSelectionDragging && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        const auto [x, y] = mouseCell();
        _controller.updateDetailSelection(x, y);
    }
    if (_controller._interactionMode == MapViewerInteractionMode::SelectDetailArea && _controller._detailSelectionDragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        const auto [x, y] = mouseCell();
        _controller.finishDetailSelection(x, y);
    }
    if (fogMode && _controller._fogStroke.active() && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
        mouseInsideMap)
    {
        const auto [x, y] = mouseCell();
        _controller.continueFogStroke(x, y);
    }
    if (_controller._fogStroke.active() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        _controller.finishFogStroke();
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        if (_controller._interactionMode == MapViewerInteractionMode::MoveParty)
            _controller.clearPendingPartyMove();
        else if (_controller._interactionMode == MapViewerInteractionMode::SelectDetailArea)
            _controller.clearDetailSelection();
        else if (fogMode)
        {
            _controller.cancelFogStroke();
            _controller.setInteractionMode(MapViewerInteractionMode::InspectSelect);
            fogMode = false;
        }
        else if (_controller._interactionMode == MapViewerInteractionMode::PingPlayers)
            _controller.setInteractionMode(MapViewerInteractionMode::InspectSelect);
        else
            _controller.clearSelectedCell();
    }

    const bool pingMode = _controller._interactionMode == MapViewerInteractionMode::PingPlayers;
    if (mouseInsideMap && (fogMode || pingMode))
    {
        const auto [x, y] = mouseCell();
        const ImVec2 cellMinimum(origin.x + static_cast<float>(x) * cellSize,
                                 origin.y + static_cast<float>(y) * cellSize);
        const ImVec2 cellMaximum(cellMinimum.x + cellSize, cellMinimum.y + cellSize);
        const ImU32 highlight = pingMode
                                    ? IM_COL32(245, 191, 67, 255)
                                    : (_controller._interactionMode == MapViewerInteractionMode::RevealFog
                                           ? IM_COL32(67, 202, 183, 255)
                                           : IM_COL32(215, 105, 86, 255));
        const ImU32 highlightFill = pingMode
                                        ? IM_COL32(245, 191, 67, 42)
                                        : (_controller._interactionMode == MapViewerInteractionMode::RevealFog
                                               ? IM_COL32(67, 202, 183, 42)
                                               : IM_COL32(215, 105, 86, 42));
        drawList->AddRectFilled(cellMinimum, cellMaximum, highlightFill);
        drawList->AddRect(cellMinimum, cellMaximum, highlight, 0.0f, 0, 3.0f);
        if (pingMode)
        {
            const ImVec2 center((cellMinimum.x + cellMaximum.x) * 0.5f,
                                (cellMinimum.y + cellMaximum.y) * 0.5f);
            drawList->AddCircle(center, std::max(4.0f, cellSize * 0.22f), highlight, 20, 2.0f);
        }
    }
}
