#include "BattlefieldCreatorViewSupport.hpp"

void BattlefieldCreatorView::drawControls()
{
    EncounterModel &battlefield = _controller._draftBattlefield;

    if (!_controller._currentCampaign.encounters.empty() && ImGui::TreeNode(uiTextForLocale("Open battlefield", "Schlachtfeld öffnen")))
    {
        std::vector<std::string> labelStorage;
        std::vector<const char *> labels;
        for (const auto &encounter : _controller._currentCampaign.encounters)
            labelStorage.push_back(encounter.name.empty() ? localizedUiLabel("<Unnamed Battlefield>") : encounter.name);
        for (const std::string &label : labelStorage)
            labels.push_back(label.c_str());
        if (_controller._selectedBattlefieldIndex < 0)
            _controller._selectedBattlefieldIndex = 0;
        FormUi::Combo(localizedUiLabel("Battlefield").c_str(), &_controller._selectedBattlefieldIndex, labels.data(), static_cast<int>(labels.size()));
        if (ImGui::Button(localizedUiLabel("Load Battlefield").c_str(), ImVec2(-1.0f, 0.0f)))
            _controller.loadSelectedBattlefield();
        ImGui::TreePop();
    }

    drawSectionTitle("Battlefield Setup");
    if (ImGui::Button(localizedUiLabel("New Battlefield").c_str(), ImVec2(-1.0f, 0.0f)))
        ImGui::OpenPopup(localizedUiLabel("New Battlefield Review").c_str());

    ImGui::Separator();
    undoableInputText("Name", battlefield.name, "Edit Battlefield Details");
    undoableInputTextMultiline("Notes", battlefield.description, ImVec2(-1.0f, 64.0f), "Edit Battlefield Details");

    if (!_controller._currentCampaign.maps.empty() && ImGui::TreeNode(uiTextForLocale("Use a campaign map", "Kampagnenkarte verwenden")))
    {
        std::vector<std::string> mapLabelStorage;
        std::vector<const char *> mapLabels;
        for (const auto &map : _controller._currentCampaign.maps)
            mapLabelStorage.push_back(map.name.empty() ? localizedUiLabel("<Unnamed Map>") : map.name);
        for (const std::string &label : mapLabelStorage)
            mapLabels.push_back(label.c_str());
        FormUi::Combo(localizedUiLabel("Context Map").c_str(), &_controller._selectedMapIndex, mapLabels.data(), static_cast<int>(mapLabels.size()));
        if (ImGui::Button(localizedUiLabel("Generate From Party Position").c_str(), ImVec2(-1.0f, 0.0f)))
            ImGui::OpenPopup(localizedUiLabel("Generate Battlefield From Party").c_str());

        const bool canUseMap = _controller.selectedMapCanBeBattlefield();
        if (!canUseMap)
            ImGui::BeginDisabled();
        if (ImGui::Button(localizedUiLabel("Use Selected Map Directly").c_str(), ImVec2(-1.0f, 0.0f)))
            ImGui::OpenPopup(localizedUiLabel("Use Map As Battlefield").c_str());
        if (!canUseMap)
            ImGui::EndDisabled();
        if (!canUseMap)
            ImGui::TextWrapped(localizedUiLabel("The map needs a complete grid between 4 and 512 cells per side.").c_str());
        ImGui::TreePop();
    }

    if (ImGui::TreeNode(uiTextForLocale("Grid settings", "Rastereinstellungen")))
    {
    const std::string contextName = battlefield.mapContextName.empty() ? localizedUiLabel("None") : battlefield.mapContextName;
    ImGui::TextDisabled(localizedUiLabel("Context: %s").c_str(), contextName.c_str());
    if (!battlefield.mapContextName.empty())
        ImGui::TextDisabled(localizedUiLabel("Map position: %d, %d").c_str(), battlefield.mapContextX, battlefield.mapContextY);

    ImGui::Separator();
    const int previousWidth = battlefield.battlefieldWidth;
    const int previousHeight = battlefield.battlefieldHeight;
    undoableInputInt("Width", battlefield.battlefieldWidth, "Edit Battlefield Dimensions");
    undoableInputInt("Height", battlefield.battlefieldHeight, "Edit Battlefield Dimensions");
    undoableInputInt("Feet per Cell", battlefield.cellSizeFeet, "Edit Battlefield Dimensions");
    const int requestedWidth = battlefield.battlefieldWidth;
    const int requestedHeight = battlefield.battlefieldHeight;
    battlefield.battlefieldWidth = previousWidth;
    battlefield.battlefieldHeight = previousHeight;
    resizeEncounterBattlefield(battlefield, requestedWidth, requestedHeight);
    normalizeEncounter(battlefield);
    ImGui::TextDisabled(localizedUiLabel("Grid dimensions are independent of feet per cell.").c_str());

    ImGui::TreePop();
    }
    drawSectionTitle("Terrain Tools");
    if (drawUiButton("Inspect", _controller._terrainTool == BattlefieldTerrainTool::Inspect ? UiButtonRole::Primary : UiButtonRole::Secondary, ImVec2(108.0f, 0.0f), "Inspect cells without changing terrain."))
        _controller._terrainTool = BattlefieldTerrainTool::Inspect;
    TaskUi::sameLineIfFits();
    if (drawUiButton("Paint", _controller._terrainTool == BattlefieldTerrainTool::Paint ? UiButtonRole::Primary : UiButtonRole::Secondary, ImVec2(108.0f, 0.0f), "Paint with the selected terrain."))
        _controller._terrainTool = BattlefieldTerrainTool::Paint;
    TaskUi::sameLineIfFits();
    if (drawUiButton("Erase", _controller._terrainTool == BattlefieldTerrainTool::Erase ? UiButtonRole::Destructive : UiButtonRole::Secondary, ImVec2(108.0f, 0.0f), "Clear cells back to Empty terrain."))
        _controller._terrainTool = BattlefieldTerrainTool::Erase;

    if (_controller._terrainTool == BattlefieldTerrainTool::Paint)
    {
    const std::vector<MapTileKind> palette = _controller.getTerrainPalette();
    if (_controller._selectedTerrainIndex < 0 || _controller._selectedTerrainIndex >= static_cast<int>(palette.size()))
        _controller._selectedTerrainIndex = 0;
    const std::string currentTerrainLabel = localizedUiLabel(mapTileKindLabel(palette[_controller._selectedTerrainIndex]));
    if (FormUi::BeginCombo(localizedUiLabel("Terrain").c_str(), currentTerrainLabel.c_str()))
    {
        for (int i = 0; i < static_cast<int>(palette.size()); ++i)
        {
            const bool selected = _controller._selectedTerrainIndex == i;
            const std::string terrainLabel = localizedUiLabel(mapTileKindLabel(palette[i]));
            if (ImGui::Selectable(terrainLabel.c_str(), selected))
            {
                _controller._selectedTerrainIndex = i;
                _controller._terrainTool = BattlefieldTerrainTool::Paint;
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if(_controller._terrainTool==BattlefieldTerrainTool::Paint)
        FormUi::SliderInt(localizedUiLabel("Brush").c_str(), &_controller._brushSize, 1, 8);
    _controller._brushSize = std::clamp(_controller._brushSize, 1, 8);

    ImGui::TextDisabled(localizedUiLabel("Selected Terrain").c_str());
    drawTerrainSwatch(_controller.selectedTerrain(), ImVec2(42.0f, 42.0f));
    TaskUi::sameLineIfFits();
    ImGui::BeginGroup();
    const std::string terrainLabel = localizedUiLabel(mapTileKindLabel(_controller.selectedTerrain()));
    const std::string terrainPurpose = localizedUiLabel(mapTileKindPurpose(_controller.selectedTerrain()));
    ImGui::Text(localizedUiLabel("%s").c_str(), terrainLabel.c_str());
    ImGui::TextDisabled(localizedUiLabel("%s").c_str(), terrainPurpose.c_str());
    ImGui::EndGroup();

    ImGui::TextDisabled(localizedUiLabel("Terrain Palette").c_str());
    ImGui::BeginChild("BattlefieldTerrainPalette", ImVec2(-1.0f, 176.0f), true);
    for (int i = 0; i < static_cast<int>(palette.size()); ++i)
    {
        ImGui::PushID(i);
        const bool selected = i == _controller._selectedTerrainIndex;
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        drawMapTile(ImGui::GetWindowDrawList(), ImVec2(cursor.x, cursor.y + 2.0f), ImVec2(cursor.x + 24.0f, cursor.y + 26.0f), palette[i], 0, false);
        ImGui::GetWindowDrawList()->AddRect(ImVec2(cursor.x, cursor.y + 2.0f), ImVec2(cursor.x + 24.0f, cursor.y + 26.0f), selected ? IM_COL32(255, 216, 112, 255) : IM_COL32(215, 224, 230, 160), 3.0f, 0, selected ? 2.0f : 1.0f);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 32.0f);
        const std::string paletteTerrainLabel = localizedUiLabel(mapTileKindLabel(palette[i]));
        std::string label = std::string(mapTileKindCode(palette[i])) + "  " + paletteTerrainLabel;
        if (ImGui::Selectable(label.c_str(), selected, 0, ImVec2(0.0f, 28.0f)))
        {
            _controller._selectedTerrainIndex = i;
            _controller._terrainTool = BattlefieldTerrainTool::Paint;
        }
        showHelpTooltip(localizedUiText(mapTileKindPurpose(palette[i])));
        ImGui::PopID();
    }
    ImGui::EndChild();

    }
    if (ImGui::CollapsingHeader(localizedUiLabel("Advanced Terrain Tools").c_str()))
    {
        if (drawUiButton("Review Fill With Selected Terrain", UiButtonRole::Advanced, ImVec2(-1.0f, 0.0f), "Review before replacing every cell on this battlefield with the selected terrain."))
            ImGui::OpenPopup(localizedUiLabel("Fill Battlefield Terrain").c_str());
        if (drawUiButton("Review Similar Terrain Generation", UiButtonRole::Advanced, ImVec2(-1.0f, 0.0f), "Review before regenerating the whole battlefield from the selected terrain."))
            ImGui::OpenPopup(localizedUiLabel("Generate Similar Battlefield Terrain").c_str());
    }
    if (drawUiButton(
            "Undo Battlefield Edit",
            _controller._hasUndo,
            "Paint, fill, generate, or import terrain first.",
            UiButtonRole::Secondary,
            ImVec2(-1.0f, 0.0f),
            "Restores recent battlefield terrain edits."))
        _controller.undoLastAction();
    if (drawUiButton("Clear Active Tool", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f), "Switches back to Inspect Terrain without changing the selected terrain."))
        _controller.cancelTerrainTool();

    const MapModel *selectedMap = nullptr;
    if (_controller._selectedMapIndex >= 0 &&
        _controller._selectedMapIndex < static_cast<int>(_controller._currentCampaign.maps.size()))
    {
        selectedMap = &_controller._currentCampaign.maps[static_cast<std::size_t>(_controller._selectedMapIndex)];
    }

    if (beginBoundedConfirmationModal(localizedUiLabel("New Battlefield Review").c_str()))
    {
        ImGui::TextWrapped(localizedUiLabel("Start a clean battlefield draft. Unsaved battlefield edits will be replaced.").c_str());
        drawInlineLabelValue("Current draft", battlefield.name.empty() ? localizedUiText("Unnamed battlefield") : battlefield.name.c_str());
        if (drawModalActionButtons("Start New", UiButtonRole::Advanced))
            _controller.startNewBattlefield();
        ImGui::EndPopup();
    }

    if (beginBoundedConfirmationModal(localizedUiLabel("Generate Battlefield From Party").c_str()))
    {
        ImGui::TextWrapped(localizedUiLabel("Generate a focused battlefield around the party position on the selected context map.").c_str());
        drawInlineLabelValue("Context map", selectedMap ? (selectedMap->name.empty() ? localizedUiText("Unnamed map") : selectedMap->name.c_str()) : localizedUiText("No map selected"));
        drawInlineLabelValue("Result", localizedUiText("Replaces the current battlefield draft"));
        if (drawModalActionButtons("Generate", UiButtonRole::Primary))
            _controller.generateFromPartyLocation();
        ImGui::EndPopup();
    }

    if (beginBoundedConfirmationModal(localizedUiLabel("Use Map As Battlefield").c_str()))
    {
        ImGui::TextWrapped(localizedUiLabel("Use the selected map directly as the battlefield grid.").c_str());
        drawInlineLabelValue("Selected map", selectedMap ? (selectedMap->name.empty() ? localizedUiText("Unnamed map") : selectedMap->name.c_str()) : localizedUiText("No map selected"));
        if (drawModalActionButtons("Use Map", UiButtonRole::Primary))
            _controller.useSelectedMapAsBattlefield();
        ImGui::EndPopup();
    }

    if (beginBoundedConfirmationModal(localizedUiLabel("Fill Battlefield Terrain").c_str()))
    {
        ImGui::TextWrapped(localizedUiLabel("Replace every battlefield cell with the selected terrain.").c_str());
        const std::string fillTerrainLabel = localizedUiLabel(mapTileKindLabel(_controller.selectedTerrain()));
        drawInlineLabelValue("Terrain", fillTerrainLabel.c_str());
        const std::string dimensions = formatLocalizedUi("%d x %d", battlefield.battlefieldWidth, battlefield.battlefieldHeight);
        drawInlineLabelValue("Grid", dimensions.c_str());
        if (drawModalActionButtons("Fill Terrain", UiButtonRole::Advanced))
            _controller.fillTerrain(_controller.selectedTerrain());
        ImGui::EndPopup();
    }

    if (beginBoundedConfirmationModal(localizedUiLabel("Generate Similar Battlefield Terrain").c_str()))
    {
        ImGui::TextWrapped(localizedUiLabel("Regenerate the whole battlefield using the selected terrain as the source style.").c_str());
        const std::string sourceTerrainLabel = localizedUiLabel(mapTileKindLabel(_controller.selectedTerrain()));
        drawInlineLabelValue("Source terrain", sourceTerrainLabel.c_str());
        const std::string dimensions = formatLocalizedUi("%d x %d", battlefield.battlefieldWidth, battlefield.battlefieldHeight);
        drawInlineLabelValue("Grid", dimensions.c_str());
        if (drawModalActionButtons("Generate", UiButtonRole::Primary))
            _controller.generateTerrain(_controller.selectedTerrain());
        ImGui::EndPopup();
    }

    FormUi::SliderFloat(localizedUiLabel("Zoom").c_str(), &_controller._zoom, 0.4f, 2.4f, "%.1fx");
    ImGui::Separator();
    if (ImGui::Button(localizedUiLabel("Save Battlefield").c_str(), ImVec2(-1.0f, 0.0f)))
        _controller.saveBattlefield();
}
