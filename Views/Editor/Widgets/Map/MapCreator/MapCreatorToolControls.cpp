#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"

using namespace gm::map_creator_view_detail;

void MapCreatorView::drawToolPalette(bool inSidebar)
{
    using Tool = MapCreatorController::Tool;
    switch (_controller._editing.activeTool)
    {
    case Tool::InspectSelect:
        if (_toolCategory != 5)
            _toolCategory = 0;
        break;
    case Tool::PaintTile:
        _toolCategory = _controller.activeTileKind() == MapTileKind::Water ? 2 : 1;
        break;
    case Tool::PaintWay:
        _toolCategory = _controller._editing.selectedWayIndex == 3 || _controller._editing.selectedWayIndex == 4 ? 2 : 3;
        break;
    case Tool::AddRoute:
        _toolCategory = 3;
        break;
    case Tool::AddFeature:
    case Tool::AddRegion:
    case Tool::PlaceToken:
        _toolCategory = 4;
        break;
    case Tool::Fog:
        _toolCategory = 6;
        break;
    default:
        break;
    }
    const std::string selection = localizedSelectedObjectSummary();
    if (selection != _lastSelectionSummary)
    {
        _lastSelectionSummary = selection;
        if (_controller.hasSelectedObject() && !_pendingCanvasInspectorReveal)
            _showCompactInspector = true;
    }
    const char *labels[] = {uiTextForLocale("Select", "Auswahl"), uiTextForLocale("Terrain", "Gelände"),
                            uiTextForLocale("Water", "Wasser"),   uiTextForLocale("Routes", "Wege"),
                            uiTextForLocale("Places & tokens", "Orte & Figuren"), uiTextForLocale("Labels", "Namen"),
                            uiTextForLocale("Fog", "Nebel")};
    float minimum = 76.0f;
    for (const char *label : labels)
        minimum = std::max(minimum, ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0f +
                                        ImGui::GetStyle().CellPadding.x * 2.0f);
    const int fitting = std::clamp(static_cast<int>(ImGui::GetContentRegionAvail().x / minimum), 1, 7);
    const int columns = inSidebar ? std::min(2, fitting) : fitting >= 7 ? 7 : (fitting >= 4 ? 4 : fitting);
    ImGui::BeginDisabled(_controller.generationBusy() || _imageTraceActive);
    if (ImGui::BeginTable("MapAuthoringTools", columns,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
    {
        for (int category = 0; category < 7; ++category)
        {
            ImGui::TableNextColumn();
            if (drawChoiceButton(labels[category], _toolCategory == category))
            {
                finishGeneratedFeatureGeometryEdit();
                _controller.endBrushStroke();
                _controller.finishPropertyEditUndo();
                _toolCategory = category;
                cancelPendingInspectorReveal();
                if (category != 0 && category != 5)
                    _controller.clearObjectSelection();
                switch (category)
                {
                case 1: {
                    const auto palette = _controller.getPalette();
                    if (_controller.activeTileKind() == MapTileKind::Empty ||
                        _controller.activeTileKind() == MapTileKind::Water)
                    {
                        const auto terrain = std::find(palette.begin(), palette.end(), MapTileKind::Dirt);
                        if (terrain != palette.end())
                            _controller._editing.selectedTileIndex = static_cast<int>(terrain - palette.begin());
                    }
                    _controller.setActiveTool(Tool::PaintTile);
                    break;
                }
                case 2: {
                    _controller._editing.selectedWayIndex = 3;
                    const auto palette = _controller.getPalette();
                    const auto water = std::find(palette.begin(), palette.end(), MapTileKind::Water);
                    if (water != palette.end())
                        _controller.selectTileFromLegend(static_cast<int>(water - palette.begin()));
                    break;
                }
                case 3:
                    _controller._editing.selectedWayIndex = 1;
                    _controller.setActiveTool(Tool::PaintWay);
                    break;
                case 4:
                    _controller._editing.selectedFeatureKindIndex = 0;
                    _controller.setActiveTool(Tool::AddFeature);
                    break;
                case 6:
                    _controller.setActiveTool(Tool::Fog);
                    break;
                default:
                    _controller.setActiveTool(Tool::InspectSelect);
                    break;
                }
                _showCompactInspector = category != 0;
                _sidebarPage = 0;
                _boxSelectMode = _boxArmed = _boxDragging = false;
            }
        }
        ImGui::EndTable();
    }
    ImGui::EndDisabled();
}

void MapCreatorView::drawToolControls()
{
    using Tool = MapCreatorController::Tool;

    auto toolButton = [this](const char *label, Tool tool, const char *tooltip) {
        if (drawChoiceButton(label, _controller._editing.activeTool == tool))
            _controller.setActiveTool(tool);
        showHelpTooltip(tooltip);
    };

    if (_toolCategory == 0)
    {
        if (drawChoiceButton(uiTextForLocale("Select an object", "Objekt auswählen"), !_boxSelectMode))
            _boxSelectMode = false;
        if (drawChoiceButton(uiTextForLocale("Select an area", "Bereich auswählen"), _boxSelectMode))
        {
            resetGeneratedFeatureGeometryInteraction();
            _boxSelectMode = true;
            _boxArmed = _boxDragging = false;
        }
        ImGui::TextWrapped(
            "%s", uiTextForLocale("Click an object to edit it, or drag a box around an area.",
                                  "Klicke ein Objekt zum Bearbeiten an oder ziehe einen Rahmen um einen Bereich."));
    }
    if (_controller._editing.activeTool == Tool::Fog)
    {
        ImGui::TextUnformatted(uiTextForLocale("Player visibility", "Spielersicht"));
        if (drawChoiceButton(uiTextForLocale("Hide from players", "Für Spieler verbergen"), !_controller._editing.revealFog))
            _controller._editing.revealFog = false;
        if (drawChoiceButton(uiTextForLocale("Reveal to players", "Für Spieler aufdecken"), _controller._editing.revealFog))
            _controller._editing.revealFog = true;
        ImGui::Spacing();
    }

    if (_toolCategory == 5)
    {
        ImGui::TextWrapped(
            "%s",
            uiTextForLocale(
                "Select a place, route, or region on the map to edit its name. The map outline also lists named objects.",
                "Wähle einen Ort, Weg oder eine Region auf der Karte, um den Namen zu ändern. Der Karteninhalt listet benannte Objekte auf."));
    }
    if (_toolCategory == 2)
    {
        if (drawChoiceButton(uiTextForLocale("Paint lake or water area", "See oder Wasserfläche malen"),
                             _controller._editing.activeTool == Tool::PaintTile &&
                                 _controller.activeTileKind() == MapTileKind::Water))
        {
            const auto palette = _controller.getPalette();
            const auto water = std::find(palette.begin(), palette.end(), MapTileKind::Water);
            if (water != palette.end())
                _controller.selectTileFromLegend(static_cast<int>(water - palette.begin()));
        }
        if (drawChoiceButton(uiTextForLocale("Draw river or canal", "Fluss oder Kanal zeichnen"),
                             _controller._editing.activeTool == Tool::PaintWay))
        {
            if (_controller._editing.selectedWayIndex != 3 && _controller._editing.selectedWayIndex != 4)
                _controller._editing.selectedWayIndex = 3;
            _controller.setActiveTool(Tool::PaintWay);
        }
        ImGui::TextWrapped(
            "%s",
            uiTextForLocale(
                "Paint an area for lakes and coastline changes. Draw a line for a river; either drawing direction works.",
                "Male eine Fläche für Seen und Küstenänderungen. Zeichne eine Linie für einen Fluss; die Zeichenrichtung ist egal."));
    }

    if (_toolCategory == 4)
    {
        toolButton("Place or note##MapCreatorTool", Tool::AddFeature,
                   "Add a point feature using the feature type below.");
        toolButton("Token##MapCreatorTool", Tool::PlaceToken, "Place a token using the token settings below.");
        toolButton("Region##MapCreatorTool", Tool::AddRegion,
                   uiTextForLocale("Drag to draw a region. Drag its edges or corners to resize it.",
                                   "Ziehe eine Region auf. Ziehe ihre Kanten oder Ecken, um die Größe zu ändern."));
    }
    if (_toolCategory == 3)
        toolButton(uiTextForLocale("Draw road or trail", "Straße oder Pfad zeichnen"), Tool::PaintWay,
                   uiTextForLocale("Drag on the map to draw a continuous route.",
                                   "Ziehe auf der Karte, um einen zusammenhängenden Weg zu zeichnen."));
    if (_toolCategory == 2 || _toolCategory == 3)
        toolButton(
            uiTextForLocale("Erase drawn route", "Gezeichneten Weg entfernen"), Tool::EraseWay,
            uiTextForLocale(
                "Erase your drawn routes. Generated and locked routes remain protected; select them to edit their shape.",
                "Entfernt selbst gezeichnete Wege. Erzeugte und gesperrte Wege bleiben geschützt; wähle sie aus, um ihre Form zu bearbeiten."));

    if (_controller.activeToolUsesBrush())
    {
        const bool lineBrush =
            _controller._editing.activeTool == Tool::PaintWay && _controller._draftMap.generation.continuousRendering;
        ImGui::TextUnformatted(lineBrush ? uiTextForLocale("Stroke width", "Strichbreite")
                                         : uiTextForLocale("Brush size (cells)", "Pinselgröße (Felder)"));
        ImGui::SetNextItemWidth(-1.0f);
        if (lineBrush)
        {
            const auto way = _controller.activeWayKind();
            FormUi::SliderInt("##LineWidth", &_controller._editing.strokeWidths.setting(way), 1, MapStrokeWidths::maximum(way));
            ImGui::TextWrapped("%s",
                               uiTextForLocale("Independent of the terrain brush", "Unabhängig vom Geländepinsel"));
        }
        else
            FormUi::SliderInt("##BrushSize", &_controller._editing.brushSize, 1, 24);
        if (!lineBrush)
        {
            if (FormUi::RadioButton(uiTextForLocale("Round", "Rund"), _controller._editing.roundBrush))
                _controller._editing.roundBrush = true;
            ImGui::SameLine();
            if (FormUi::RadioButton(uiTextForLocale("Square", "Eckig"), !_controller._editing.roundBrush))
                _controller._editing.roundBrush = false;
        }
    }
    if (_controller._editing.activeTool == Tool::Fog)
    {
        ImGui::SeparatorText(uiTextForLocale("Whole map", "Ganze Karte"));
        if (ImGui::Button(uiTextForLocale("Hide whole map", "Ganze Karte verdecken"), ImVec2(-1.0f, 0.0f)))
            _controller.setAllDraftFog(true);
        if (ImGui::Button(uiTextForLocale("Reveal whole map", "Ganze Karte aufdecken"), ImVec2(-1.0f, 0.0f)))
            _controller.setAllDraftFog(false);
        ImGui::TextWrapped("%s", uiTextForLocale(
            "These edits can be undone. Save, review the player view, then show it when ready.",
            "Diese Änderungen lassen sich rückgängig machen. Speichere und prüfe die Spieleransicht vor dem Anzeigen."));
        ImGui::TextWrapped("%s",
                           uiTextForLocale("Drag on the map to paint. You can see through fog; players cannot.",
                                           "Ziehe auf der Karte zum Malen. Du siehst durch den Nebel, Spieler nicht."));
        if (ImGui::TreeNode(uiTextForLocale("Fog appearance for you", "Nebeldarstellung für dich")))
        {
            int opacity = static_cast<int>(std::lround(_editorFogOpacity * 100));
            ImGui::TextUnformatted(uiTextForLocale("Mist opacity", "Deckkraft des Nebels"));
            ImGui::SetNextItemWidth(-1.f);
            if (FormUi::SliderInt("##FogOpacity", &opacity, 10, 65, "%d%%"))
                _editorFogOpacity = opacity / 100.f;
            ImGui::TreePop();
        }
    }

    if (_controller._editing.activeTool == Tool::PaintTile)
    {
        const std::vector<MapTileKind> palette = _controller.getPalette();
        if (!palette.empty())
        {
            _controller._editing.selectedTileIndex =
                std::clamp(_controller._editing.selectedTileIndex, 0, static_cast<int>(palette.size()) - 1);
            const std::string selectedLabel =
                localizedUiLabel(mapTileKindLabel(palette[_controller._editing.selectedTileIndex]));
            ImGui::SetNextItemWidth(-1.0f);
            if (FormUi::BeginCombo("##TerrainMaterial", selectedLabel.c_str()))
            {
                for (int index = 0; index < static_cast<int>(palette.size()); ++index)
                    if (ImGui::Selectable(localizedUiLabel(mapTileKindLabel(palette[index])).c_str(),
                                          index == _controller._editing.selectedTileIndex))
                        _controller.selectTileFromLegend(index);
                ImGui::EndCombo();
            }
        }
        if (ImGui::TreeNode(uiTextForLocale("Brush appearance", "Pinseldarstellung")))
        {
            FormUi::Checkbox(uiTextForLocale("Show tile palette beside map", "Kachelpalette neben der Karte"),
                             &_showTileLegend);
            const std::string localizedVariants = localizedComboItems(variantModeItems);
            FormUi::Combo(localizedUiLabel("Variant Mode").c_str(), &_controller._generation.selectedSymbolVariantModeIndex,
                          localizedVariants.c_str());
            FormUi::Checkbox(localizedUiLabel("Auto Rotation").c_str(), &_controller._editing.autoRotateTiles);
            const std::string localizedRotations = localizedComboItems(rotationItems);
            FormUi::Combo(localizedUiLabel("Rotation").c_str(), &_controller._editing.rotationQuarterTurns,
                          localizedRotations.c_str());
            const UiButtonPairResult rotationActions =
                drawResponsiveButtonPair("MapTileRotationActions", "Rotate Left", UiButtonRole::Secondary,
                                         "Rotate the selected tile counter-clockwise.", "Rotate Right",
                                         UiButtonRole::Secondary, "Rotate the selected tile clockwise.");
            if (rotationActions.first)
                _controller._editing.rotationQuarterTurns = normalizeMapRotation(_controller._editing.rotationQuarterTurns - 1);
            if (rotationActions.second)
                _controller._editing.rotationQuarterTurns = normalizeMapRotation(_controller._editing.rotationQuarterTurns + 1);
            ImGui::TreePop();
        }
    }
    else if (_controller._editing.activeTool == Tool::PaintWay)
    {
        const char *wayLabels[] = {"Trail", "Dirt Road",        "Stone Road", "River",
                                   "Canal", "Dungeon Corridor", "Cave Tunnel"};
        _controller._editing.selectedWayIndex = std::clamp(_controller._editing.selectedWayIndex, 0, 6);
        if (FormUi::BeginCombo(localizedUiLabel("Way").c_str(),
                               localizedUiLabel(wayLabels[_controller._editing.selectedWayIndex]).c_str()))
        {
            for (int index = 0; index < 7; ++index)
            {
                if ((_toolCategory == 2) != (index == 3 || index == 4))
                    continue;
                if (index >= 5 && _controller._draftMap.atlasLevel != MapLevel::Tactical)
                    continue;
                if (ImGui::Selectable(localizedUiLabel(wayLabels[index]).c_str(),
                                      index == _controller._editing.selectedWayIndex))
                    _controller._editing.selectedWayIndex = index;
            }
            ImGui::EndCombo();
        }
    }
    else if (_controller._editing.activeTool == Tool::PaintElevation)
    {
        FormUi::SliderInt(localizedUiLabel("Elevation").c_str(), &_controller._editing.selectedElevation, -20, 20);
        drawDisabledWrappedText("Elevation is stored in cell-height steps.");
    }
    else if (_controller._editing.activeTool == Tool::PaintTacticalEdge)
    {
        const std::string localizedPresets = localizedComboItems(tacticalPresetItems);
        const std::string localizedDirections = localizedComboItems(tacticalDirectionItems);
        FormUi::Combo(localizedUiLabel("Preset").c_str(), &_controller._editing.selectedTacticalPresetIndex,
                      localizedPresets.c_str());
        FormUi::Combo(localizedUiLabel("Direction").c_str(), &_controller._editing.rotationQuarterTurns,
                      localizedDirections.c_str());
        const MapTacticalPreset preset = static_cast<MapTacticalPreset>(
            std::clamp(_controller._editing.selectedTacticalPresetIndex, 0, static_cast<int>(MapTacticalPreset::ClearEdge)));
        if (preset == MapTacticalPreset::CustomBlocker)
        {
            const std::string localizedChannels = localizedComboItems(tacticalChannelItems);
            FormUi::Combo(localizedUiLabel("Blocks").c_str(), &_controller._editing.selectedTacticalChannelIndex,
                          localizedChannels.c_str());
            FormUi::Checkbox(localizedUiLabel("One-way (arrow direction)").c_str(), &_controller._editing.tacticalOneWay);
        }
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(),
                           localizedUiLabel(mapTacticalPresetDescription(preset)).c_str());
        const UiButtonPairResult rotationActions =
            drawResponsiveButtonPair("MapEdgeRotationActions", "Rotate Edge Left", UiButtonRole::Secondary,
                                     "Rotate the selected edge counter-clockwise.", "Rotate Edge Right",
                                     UiButtonRole::Secondary, "Rotate the selected edge clockwise.");
        if (rotationActions.first)
            _controller._editing.rotationQuarterTurns = normalizeMapRotation(_controller._editing.rotationQuarterTurns - 1);
        if (rotationActions.second)
            _controller._editing.rotationQuarterTurns = normalizeMapRotation(_controller._editing.rotationQuarterTurns + 1);
    }

    if (_toolCategory != 6 && ImGui::TreeNode(uiTextForLocale("Advanced tools", "Erweiterte Werkzeuge")))
    {
        if (_toolCategory == 3)
            toolButton("Travel Route##MapCreatorTool", Tool::AddRoute,
                       "Add endpoint travel metadata from the clicked cell.");
        toolButton("Erase##MapCreatorTool", Tool::EraseTile, "Erase tiles and clear cell flags in the brush area.");
        toolButton("Clear Way##MapCreatorTool", Tool::EraseWay,
                   "Clear connected way overlays without changing terrain.");
        toolButton("Elevation##MapCreatorTool", Tool::PaintElevation, "Set exact cell elevation with a brush.");
        if (_controller._draftMap.atlasLevel == MapLevel::Tactical)
            toolButton("Edges##MapCreatorTool", Tool::PaintTacticalEdge,
                       "Paint walls, ledges, doors, cover, and other tactical edges.");
        FormUi::Checkbox(localizedUiLabel("Show editing grid").c_str(), &_controller._editing.showEditorGrid);
        ImGui::TreePop();
    }
}
