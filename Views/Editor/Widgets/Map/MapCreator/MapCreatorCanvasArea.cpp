#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorCanvasFrame.hpp"

using namespace gm::map_creator_view_detail;

void MapCreatorView::drawAreaSelectionOverlay(ImDrawList *canvas, ImVec2 origin, float cellSize)
{
    if(_areaSelectionSerial!=_controller._editing.authoringSerial) {_areaCandidates.clear();_areaDragArmed=_areaDragging=false;return;}
    for(const auto &item:_areaCandidates) if(item.selected) {
            const auto &map = _controller._draftMap;
            const ImU32 color = IM_COL32(70, 225, 230, 255);
            const auto at = [&](MapFeaturePoint p) {
                return ImVec2(origin.x + (p.x+_areaDeltaX) * cellSize, origin.y + (p.y+_areaDeltaY) * cellSize);
            };
            if (item.kind == gm::mapui::AreaObjectKind::Generated)
            {
                const auto &feature = map.generation.graph.features[item.index];
                std::vector<ImVec2> points;
                for (auto p : item.previewPoints.empty()?feature.points:item.previewPoints)
                    points.push_back(at(p));
                if (points.size() > 1)
                    canvas->AddPolyline(points.data(), static_cast<int>(points.size()), color,
                                        feature.closed ? ImDrawFlags_Closed : 0, 4);
                else if (points.size() == 1)
                    canvas->AddCircle(points.front(), std::max(12.f, cellSize), color, 24, 3);
            }
            else if (item.kind == gm::mapui::AreaObjectKind::Terrain)
            {
                drawTerrainSelectionCells(canvas, origin, cellSize, item.terrain, _areaDeltaX, _areaDeltaY, color,
                                          IM_COL32(70, 225, 230, 80));

            }
            else if (item.kind == gm::mapui::AreaObjectKind::Route)
            {
                for (std::size_t p = 1; p < item.previewPoints.size(); ++p)
                    canvas->AddLine(at(item.previewPoints[p - 1]), at(item.previewPoints[p]), color, 4);
            }
            else if (item.kind == gm::mapui::AreaObjectKind::Marker || item.kind == gm::mapui::AreaObjectKind::Token)
            {
                const auto point =
                    item.kind == gm::mapui::AreaObjectKind::Marker
                        ? MapFeaturePoint{map.features[item.index].x + .5f, map.features[item.index].y + .5f}
                        : MapFeaturePoint{map.tokens[item.index].x + .5f, map.tokens[item.index].y + .5f};
                canvas->AddCircle(at(point), std::max(12.f, cellSize), color, 24, 3);
            }
            else if (item.kind == gm::mapui::AreaObjectKind::Region)
            {
                const auto &r = map.regions[item.index];
                canvas->AddRect(at({static_cast<float>(r.x), static_cast<float>(r.y)}),
                                at({static_cast<float>(r.x + r.width), static_cast<float>(r.y + r.height)}), color, 0,
                                0, 3);
            }
    }
}

void MapCreatorView::drawAreaSelectionSidebar()
{
    if(_areaSelectionSerial!=_controller._editing.authoringSerial || _areaCandidates.empty())return;
    ImGui::SeparatorText(uiTextForLocale("Area selection", "Bereichsauswahl"));
    ImGui::Text(uiTextForLocale("%d selected", "%d ausgewählt"),int(gm::mapui::selectedAreaCount(_areaCandidates)));
    ImGui::TextWrapped("%s",uiTextForLocale("Uncheck items to exclude them. Drag any highlighted item to move the selection.",
        "Elemente zum Ausschließen abwählen. Ein markiertes Element ziehen, um die Auswahl zu verschieben."));
    ImGui::BeginChild("AreaSelectionMembers",ImVec2(0,std::clamp(ImGui::GetContentRegionAvail().y-48.f,100.f,200.f)),true);
    for(int i=0;i<int(_areaCandidates.size());++i) {
        auto &item=_areaCandidates[i];ImGui::PushID(i);
        const auto label=item.kind==gm::mapui::AreaObjectKind::Terrain?terrainSelectionSummary(item.terrain,true):
            (localizedGeneratedMapName(item.name)+" - "+localizedUiLabel(item.type.c_str()));
        FormUi::Checkbox(label.c_str(),&item.selected);
        ImGui::PopID();
    }
    ImGui::EndChild();
    if(ImGui::Button(uiTextForLocale("Clear selection", "Auswahl aufheben")))_areaCandidates.clear();
}

bool MapCreatorView::handleRegionCanvasInteraction(ImVec2 origin, float cellSize, bool hovered)
{
    using namespace gm::mapui;
    using Tool = MapCreatorController::Tool;
    auto &map = _controller._draftMap;
    const auto &io = ImGui::GetIO();
    const MapFeaturePoint mouse{(io.MousePos.x - origin.x) / cellSize, (io.MousePos.y - origin.y) / cellSize};
    const bool enabled = !_controller._generation.inProgress && !_imageTraceActive && !_regenerationPickingArea &&
                         (_controller._editing.activeTool == Tool::InspectSelect || _controller._editing.activeTool == Tool::AddRegion);
    bool consumed = _regionHandle != 0;
    if (_regionHandle &&
        (!enabled || _regionGestureTool != static_cast<int>(_controller._editing.activeTool) ||
         _regionGestureMapId != map.metadata.id || _regionGestureSerial != _controller._editing.authoringSerial ||
         (_regionDragIndex >= 0 && (_regionDragIndex >= static_cast<int>(map.regions.size()) ||
                                    map.regions[_regionDragIndex].metadata.id != _regionGestureId)) ||
         ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)))
    {
        _regionHandle = RegionNone;
        _regionDragging = false;
        return true; // A cancelled preview never changes the map or undo history.
    }
    if (!enabled)
        return consumed;

    int hit = RegionNone, index = -1;
    const bool boxMode = _boxSelectMode && _controller._editing.activeTool == Tool::InspectSelect;
    if (hovered && !_regionHandle && !boxMode && !io.KeyCtrl && !io.KeyShift && !io.KeyAlt)
    {
        const auto inspect = [&](int candidate) {
            if (candidate < 0 || candidate >= static_cast<int>(map.regions.size()))
                return;
            const int candidateHit = regionHandleAt(RegionBounds::from(map.regions[candidate]), mouse, 7.f / cellSize);
            if (candidateHit)
            {
                hit = candidateHit;
                index = candidate;
            }
        };
        inspect(_controller._editing.selectedRegionIndex);
        if (!hit && _controller._editing.activeTool == Tool::AddRegion)
            for (int i = static_cast<int>(map.regions.size()) - 1; i >= 0 && !hit; --i)
                inspect(i);
    }
    const bool create =
        hovered && !_regionHandle && !hit && !io.KeyCtrl && !io.KeyShift &&
        (_controller._editing.activeTool == Tool::AddRegion || (_controller._editing.activeTool == Tool::InspectSelect && io.KeyAlt));
    if (!_regionHandle && hit && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        _controller.cancelActiveTool();
        return true;
    }
    if (!_regionHandle && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && (hit || create))
    {
        resetGeneratedFeatureGeometryInteraction();
        _boxSelectMode = _boxArmed = _boxDragging = false;
        _controller.finishPropertyEditUndo();
        _regionHandle = hit ? hit : RegionCreate;
        _regionDragIndex = index;
        _regionDragging = false;
        _regionDragStart = mouse;
        _regionGestureTool = static_cast<int>(_controller._editing.activeTool);
        _regionGestureMapId = map.metadata.id;
        _regionGestureSerial = _controller._editing.authoringSerial;
        _regionGestureId = index >= 0 ? map.regions[index].metadata.id : "";
        if (index >= 0)
        {
            _controller.selectRegionObject(index);
            _regionBefore = RegionBounds::from(map.regions[index]);
        }
        else
        {
            const int x = std::clamp(static_cast<int>(std::floor(mouse.x)), 0, map.width - 1);
            const int y = std::clamp(static_cast<int>(std::floor(mouse.y)), 0, map.height - 1);
            _regionBefore = {x, y, std::min(4, map.width - x), std::min(3, map.height - y)};
        }
        _regionPreview = _regionBefore;
        consumed = true;
    }
    if (_regionHandle)
    {
        if (mapCreatorShouldUpdateGeneratedFeatureDrag(_regionDragging))
        {
            _regionDragging = true;
            _regionPreview =
                regionDragBounds(_regionBefore, _regionHandle, _regionDragStart, mouse, map.width, map.height);
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            if (_regionHandle == RegionCreate)
            {
                _controller.addRegionAt(_regionPreview.x, _regionPreview.y);
                _regionPreview.apply(map.regions[_controller._editing.selectedRegionIndex]);
            }
            else if (!(_regionPreview == _regionBefore))
            {
                _controller.captureUndoSnapshot(_regionHandle == RegionMove ? "Move Region" : "Resize Region");
                _regionPreview.apply(map.regions[_regionDragIndex]);
            }
            _regionHandle = RegionNone;
            _regionDragging = false;
            _showCompactInspector = true;
        }
    }

    const int cursorHandle = _regionHandle ? _regionHandle : hit;
    if (cursorHandle)
    {
        const bool horizontal = cursorHandle & (RegionLeft | RegionRight),
                   vertical = cursorHandle & (RegionTop | RegionBottom);
        ImGui::SetMouseCursor(horizontal && vertical ? ((cursorHandle == (RegionLeft | RegionTop) ||
                                                         cursorHandle == (RegionRight | RegionBottom))
                                                            ? ImGuiMouseCursor_ResizeNWSE
                                                            : ImGuiMouseCursor_ResizeNESW)
                              : horizontal           ? ImGuiMouseCursor_ResizeEW
                              : vertical             ? ImGuiMouseCursor_ResizeNS
                                                     : ImGuiMouseCursor_ResizeAll);
    }
    const int selected = _controller._editing.selectedRegionIndex;
    if (_regionHandle || (selected >= 0 && selected < static_cast<int>(map.regions.size()) && !boxMode))
    {
        const RegionBounds bounds = _regionHandle ? _regionPreview : RegionBounds::from(map.regions[selected]);
        const ImVec2 a(origin.x + bounds.x * cellSize, origin.y + bounds.y * cellSize);
        const ImVec2 b(a.x + bounds.width * cellSize, a.y + bounds.height * cellSize);
        auto *draw = ImGui::GetWindowDrawList();
        const ImU32 color = IM_COL32(70, 210, 220, 255);
        if (_regionHandle)
        {
            draw->AddRectFilled(a, b, IM_COL32(45, 185, 200, 35));
            draw->AddRect(a, b, color, 0, 0, 2);
            const std::string dimensions = std::to_string(bounds.width) + " x " + std::to_string(bounds.height);
            const ImVec2 textSize = ImGui::CalcTextSize(dimensions.c_str());
            const ImVec2 label(a.x + 8, a.y + 8);
            draw->AddRectFilled(ImVec2(label.x - 3, label.y - 2),
                                ImVec2(label.x + textSize.x + 3, label.y + textSize.y + 2), IM_COL32(25, 32, 32, 235),
                                2);
            draw->AddText(label, IM_COL32(240, 245, 245, 255), dimensions.c_str());
        }
        for (const ImVec2 point : {a, ImVec2((a.x + b.x) * .5f, a.y), ImVec2(b.x, a.y), ImVec2(b.x, (a.y + b.y) * .5f),
                                   b, ImVec2((a.x + b.x) * .5f, b.y), ImVec2(a.x, b.y), ImVec2(a.x, (a.y + b.y) * .5f)})
        {
            draw->AddRectFilled(ImVec2(point.x - 4, point.y - 4), ImVec2(point.x + 4, point.y + 4),
                                IM_COL32(25, 40, 40, 255));
            draw->AddRect(ImVec2(point.x - 4, point.y - 4), ImVec2(point.x + 4, point.y + 4), color, 0, 0, 1.5f);
        }
    }
    return consumed || hit != RegionNone;
}
