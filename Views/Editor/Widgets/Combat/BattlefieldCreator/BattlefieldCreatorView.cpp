#include "BattlefieldCreatorViewSupport.hpp"

BattlefieldCreatorView::BattlefieldCreatorView(BattlefieldCreatorController &controller)
    : DockableWidget("Battlefield Creator", 0, ImVec2(1120.0f, 760.0f)), _controller(controller)
{
}

void BattlefieldCreatorView::finishUndoablePropertyEditIfIdle()
{
    if (!ImGui::IsAnyItemActive())
        _controller.finishPropertyEditUndo();
}

bool BattlefieldCreatorView::undoableInputInt(const char *label, int &value, const char *undoLabel)
{
    int edited = value;
    const bool changed = FormUi::InputInt(label, &edited);
    if (changed && edited != value)
    {
        _controller.beginPropertyEditUndo(undoLabel);
        value = edited;
    }
    finishUndoablePropertyEditIfIdle();
    return changed;
}

bool BattlefieldCreatorView::undoableInputText(const char *label, std::string &value, const char *undoLabel)
{
    std::string edited = value;
    const bool changed = FormUi::InputText(label, &edited);
    if (changed && edited != value)
    {
        _controller.beginPropertyEditUndo(undoLabel);
        value = edited;
    }
    finishUndoablePropertyEditIfIdle();
    return changed;
}

bool BattlefieldCreatorView::undoableInputTextMultiline(const char *label, std::string &value, const ImVec2 &size, const char *undoLabel)
{
    std::string edited = value;
    const bool changed = FormUi::InputTextMultiline(label, &edited, size);
    if (changed && edited != value)
    {
        _controller.beginPropertyEditUndo(undoLabel);
        value = edited;
    }
    finishUndoablePropertyEditIfIdle();
    return changed;
}

void BattlefieldCreatorView::draw()
{
    if (_controller._saveFailed)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.36f, 0.30f, 1.0f));
        ImGui::TextWrapped("%s", _controller._saveErrorMessage.c_str());
        ImGui::PopStyleColor();
        if (drawUiButton("Retry Save", true, nullptr, UiButtonRole::Primary))
            _controller.saveBattlefield();
    }

    normalizeEncounter(_controller._draftBattlefield);
    _controller._brushSize = std::clamp(_controller._brushSize, 1, 8);
    const MapTileKind selectedTerrain = _controller.selectedTerrain();
    const std::string selectedTerrainLabel = localizedUiLabel(mapTileKindLabel(selectedTerrain));
    std::string selection = formatLocalizedUi("%s, brush %d", selectedTerrainLabel.c_str(), _controller._brushSize);
    if (_controller._terrainTool == BattlefieldTerrainTool::Erase)
    {
        const std::string emptyLabel = localizedUiLabel("Empty");
        selection = formatLocalizedUi("%s, brush %d", emptyLabel.c_str(), _controller._brushSize);
    }
    else if (_controller._terrainTool == BattlefieldTerrainTool::Inspect)
        selection = localizedUiLabel("No paint target");
    const std::string toolLabel = localizedUiLabel(battlefieldTerrainToolLabel(_controller._terrainTool));
    const std::string nextAction = localizedUiLabel(battlefieldTerrainToolNextAction(_controller._terrainTool));
    drawStatusActionStrip(
        "BattlefieldCreatorState",
        {toolLabel.c_str(),
         selection.c_str(),
         nextAction.c_str(),
         "Battlefield setup controls are separate from terrain tools. Right click the grid to cancel the active paint or erase tool."});

    const bool wide = TaskUi::hasRoomForColumns(70.0f);
    if (wide)
    {
        if (ImGui::BeginTable("BattlefieldCreatorLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn(localizedUiText("Controls"), ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 24.0f);
            ImGui::TableSetupColumn(localizedUiText("Canvas"));
            ImGui::TableNextRow(); ImGui::TableNextColumn(); drawControls();
            ImGui::TableNextColumn(); drawCanvas(); ImGui::EndTable();
        }
    }
    else if (ImGui::BeginTabBar("BattlefieldCompactWorkspace"))
    {
        if (ImGui::BeginTabItem(localizedUiText("Controls"))) { drawControls(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem(localizedUiText("Canvas"))) { drawCanvas(); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }

}
