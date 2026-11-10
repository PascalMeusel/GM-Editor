#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"

using namespace gm::map_creator_view_detail;

std::string MapCreatorView::localizedSelectedObjectSummary() const
{
    const MapModel &map = _controller._draftMap;
    if (_controller._editing.selectedTokenIndex >= 0 && _controller._editing.selectedTokenIndex < static_cast<int>(map.tokens.size()))
    {
        const MapTokenModel &token = map.tokens[static_cast<std::size_t>(_controller._editing.selectedTokenIndex)];
        const std::string fallback = localizedUiLabel(mapTokenKindLabel(token.kind));
        return formatLocalizedUi("Token: %s", token.name.empty() ? fallback.c_str() : token.name.c_str());
    }
    if (_controller._editing.selectedFeatureIndex >= 0 &&
        _controller._editing.selectedFeatureIndex < static_cast<int>(map.features.size()))
    {
        const MapFeatureModel &feature = map.features[static_cast<std::size_t>(_controller._editing.selectedFeatureIndex)];
        const std::string fallback = localizedUiLabel(mapFeatureKindLabel(feature.kind));
        return formatLocalizedUi("Feature: %s", feature.name.empty() ? fallback.c_str() : feature.name.c_str());
    }
    if (_controller._editing.selectedGeneratedFeatureIndex >= 0 &&
        _controller._editing.selectedGeneratedFeatureIndex < static_cast<int>(map.generation.graph.features.size()))
    {
        const MapGeneratedFeature &feature =
            map.generation.graph.features[static_cast<std::size_t>(_controller._editing.selectedGeneratedFeatureIndex)];
        const std::string fallback = localizedUiLabel(generatedFeatureKindLabel(feature.kind));
        const std::string name = feature.name.empty() ? fallback : localizedGeneratedFeatureName(feature);
        return formatLocalizedUi("Generated feature: %s", name.c_str());
    }
    if (_controller._editing.selectedRegionIndex >= 0 &&
        _controller._editing.selectedRegionIndex < static_cast<int>(map.regions.size()))
    {
        const MapRegionModel &region = map.regions[static_cast<std::size_t>(_controller._editing.selectedRegionIndex)];
        const std::string fallback = localizedUiLabel("<Region>");
        const std::string name = region.name.empty() ? fallback : localizedGeneratedMapName(region.name);
        return formatLocalizedUi("Region: %s", name.c_str());
    }
    if (_controller._editing.selectedRouteIndex >= 0 && _controller._editing.selectedRouteIndex < static_cast<int>(map.routes.size()))
    {
        const MapRouteModel &route = map.routes[static_cast<std::size_t>(_controller._editing.selectedRouteIndex)];
        const std::string fallback = localizedUiLabel("<Route>");
        return formatLocalizedUi("Route: %s", route.name.empty() ? fallback.c_str() : route.name.c_str());
    }
    if (_controller._editing.activeTool == MapCreatorController::Tool::InspectSelect && !_terrainSelection.cells.empty() &&
        (_terrainDragging || _terrainSelectionSerial == _controller._editing.authoringSerial))
        return terrainSelectionSummary(_terrainSelection);
    return localizedUiLabel("No object selected");
}

std::string MapCreatorView::localizedActiveSelectionSummary() const
{
    using Tool = MapCreatorController::Tool;

    switch (_controller._editing.activeTool)
    {
    case Tool::InspectSelect:
        return localizedSelectedObjectSummary();
    case Tool::PaintTile: {
        const std::string tile = localizedUiLabel(mapTileKindLabel(_controller.activeTileKind()));
        return formatLocalizedUi("Tile: %s, brush %d", tile.c_str(), _controller._editing.brushSize);
    }
    case Tool::EraseTile: {
        const std::string empty = localizedUiLabel(mapTileKindLabel(MapTileKind::Empty));
        return formatLocalizedUi("Tile: %s, brush %d", empty.c_str(), _controller._editing.brushSize);
    }
    case Tool::PaintWay: {
        const std::string way = localizedUiLabel(mapWayKindLabel(_controller.activeWayKind()));
        if (_controller._draftMap.generation.continuousRendering)
            return way + " · " + uiTextForLocale("Stroke width", "Strichbreite") + " " +
                   std::to_string(_controller.activeBrushSize());
        return formatLocalizedUi("Way: %s, brush %d", way.c_str(), _controller._editing.brushSize);
    }
    case Tool::EraseWay: {
        return std::string(uiTextForLocale("Erase drawn route", "Gezeichneten Weg entfernen")) + " · " +
               std::to_string(_controller._editing.brushSize);
    }
    case Tool::PaintElevation:
        return formatLocalizedUi("Elevation: %d, brush %d", _controller._editing.selectedElevation, _controller._editing.brushSize);
    case Tool::PaintTacticalEdge: {
        const MapTacticalPreset preset = static_cast<MapTacticalPreset>(
            std::clamp(_controller._editing.selectedTacticalPresetIndex, 0, static_cast<int>(MapTacticalPreset::ClearEdge)));
        const std::string presetLabel = localizedUiLabel(mapTacticalPresetLabel(preset));
        const std::string directionLabel =
            localizedUiLabel(tacticalDirectionLabels[normalizeMapRotation(_controller._editing.rotationQuarterTurns)]);
        return presetLabel + ", " + directionLabel;
    }
    case Tool::PlaceToken: {
        const MapTokenKind tokenKind = static_cast<MapTokenKind>(
            std::clamp(_controller._editing.selectedTokenKindIndex, 0, static_cast<int>(MapTokenKind::Party)));
        const std::string tokenLabel = localizedUiLabel(mapTokenKindLabel(tokenKind));
        std::string linkSuffix;
        const std::vector<const char *> links = _controller.getLinkedEntityLabels();
        if (_controller._editing.selectedLinkedEntityIndex > 0 &&
            _controller._editing.selectedLinkedEntityIndex < static_cast<int>(links.size()))
        {
            const std::string linkLabel =
                localizedModelPickerLabel(links[static_cast<std::size_t>(_controller._editing.selectedLinkedEntityIndex)]);
            linkSuffix = formatLocalizedUi(", link: %s", linkLabel.c_str());
        }
        return formatLocalizedUi("Token: %s", tokenLabel.c_str()) + linkSuffix;
    }
    case Tool::AddFeature: {
        const MapFeatureKind featureKind = static_cast<MapFeatureKind>(
            std::clamp(_controller._editing.selectedFeatureKindIndex, 0, static_cast<int>(MapFeatureKind::Pin)));
        const std::string featureLabel = localizedUiLabel(mapFeatureKindLabel(featureKind));
        return formatLocalizedUi("Feature: %s", featureLabel.c_str());
    }
    case Tool::AddRegion:
        return uiTextForLocale("Region: drag to draw an area", "Region: Fläche mit der Maus aufziehen");
    case Tool::AddRoute:
        return localizedUiLabel("Route: clicked start cell");
    case Tool::Fog:
        return std::string(_controller._editing.revealFog ? uiTextForLocale("Reveal area", "Bereich aufdecken")
                                                  : uiTextForLocale("Hide area", "Bereich verbergen")) +
               " · " + std::to_string(_controller._editing.brushSize);
    }
    return localizedSelectedObjectSummary();
}

void MapCreatorView::finishUndoablePropertyEditIfIdle()
{
    if (!ImGui::IsAnyItemActive())
        _controller.finishPropertyEditUndo();
}

bool MapCreatorView::undoableCheckbox(const char *label, bool &value, const char *undoLabel)
{
    bool edited = value;
    const bool changed = FormUi::Checkbox(localizedUiLabel(label).c_str(), &edited);
    if (changed && edited != value)
    {
        _controller.beginPropertyEditUndo(undoLabel);
        value = edited;
    }
    finishUndoablePropertyEditIfIdle();
    return changed;
}

bool MapCreatorView::undoableCombo(const char *label, int &value, const char *items, const char *undoLabel)
{
    int edited = value;
    const std::string localizedItems = localizedComboItems(items);
    const bool changed = FormUi::Combo(localizedUiLabel(label).c_str(), &edited, localizedItems.c_str());
    if (changed && edited != value)
    {
        _controller.beginPropertyEditUndo(undoLabel);
        value = edited;
    }
    finishUndoablePropertyEditIfIdle();
    return changed;
}

bool MapCreatorView::undoableInputFloat(const char *label, float &value, const char *undoLabel)
{
    float edited = value;
    const bool changed = FormUi::InputFloat(localizedUiLabel(label).c_str(), &edited);
    if (changed && edited != value)
    {
        _controller.beginPropertyEditUndo(undoLabel);
        value = edited;
    }
    finishUndoablePropertyEditIfIdle();
    return changed;
}

bool MapCreatorView::undoableInputInt(const char *label, int &value, const char *undoLabel)
{
    int edited = value;
    const bool changed = FormUi::InputInt(localizedUiLabel(label).c_str(), &edited);
    if (changed && edited != value)
    {
        _controller.beginPropertyEditUndo(undoLabel);
        value = edited;
    }
    finishUndoablePropertyEditIfIdle();
    return changed;
}

bool MapCreatorView::undoableInputText(const char *label, std::string &value, const char *undoLabel)
{
    std::string edited = value;
    const bool changed = FormUi::InputText(localizedUiLabel(label).c_str(), &edited);
    if (changed && edited != value)
    {
        _controller.beginPropertyEditUndo(undoLabel);
        value = edited;
    }
    finishUndoablePropertyEditIfIdle();
    return changed;
}

bool MapCreatorView::undoableInputTextMultiline(const char *label, std::string &value, const ImVec2 &size,
                                                const char *undoLabel)
{
    std::string edited = value;
    const bool changed =
        FormUi::InputTextMultiline(localizedUiLabel(label).c_str(), &edited, size, ImGuiInputTextFlags_WordWrap);
    if (changed && edited != value)
    {
        _controller.beginPropertyEditUndo(undoLabel);
        value = edited;
    }
    finishUndoablePropertyEditIfIdle();

    return changed;
}

bool MapCreatorView::undoableSliderFloat(const char *label, float &value, float minValue, float maxValue,
                                         const char *format, const char *undoLabel)
{
    float edited = value;
    const bool changed = FormUi::SliderFloat(localizedUiLabel(label).c_str(), &edited, minValue, maxValue, format);
    if (changed && edited != value)
    {
        _controller.beginPropertyEditUndo(undoLabel);
        value = edited;
    }
    finishUndoablePropertyEditIfIdle();
    return changed;
}

MapGenerationSelection MapCreatorView::partialRegenerationSelection() const
{
    MapGenerationSelection selection;
    selection.useArea = _regenerationUseArea;
    if (_regenerationUseArea)
    {
        const int minimumX = std::min(_regenerationAreaStartX, _regenerationAreaEndX);
        const int minimumY = std::min(_regenerationAreaStartY, _regenerationAreaEndY);
        const int maximumX = std::max(_regenerationAreaStartX, _regenerationAreaEndX);
        const int maximumY = std::max(_regenerationAreaStartY, _regenerationAreaEndY);
        selection.normalizedArea = {
            static_cast<float>(minimumX) / static_cast<float>(std::max(1, _controller._draftMap.width)),
            static_cast<float>(minimumY) / static_cast<float>(std::max(1, _controller._draftMap.height)),
            static_cast<float>(maximumX - minimumX + 1) / static_cast<float>(std::max(1, _controller._draftMap.width)),
            static_cast<float>(maximumY - minimumY + 1) /
                static_cast<float>(std::max(1, _controller._draftMap.height))};
    }

    switch (std::clamp(_regenerationFamilyIndex, 0, 3))
    {
    case 0:
        selection.featureKinds = {MapGeneratedFeatureKind::River, MapGeneratedFeatureKind::Bridge,
                                  MapGeneratedFeatureKind::WaterBody};
        selection.replaceWayProjection = true;
        break;
    case 1:
        selection.featureKinds = {MapGeneratedFeatureKind::Road, MapGeneratedFeatureKind::Trail,
                                  MapGeneratedFeatureKind::Bridge};
        selection.replaceWayProjection = true;
        break;
    case 2:
        selection.featureKinds = {MapGeneratedFeatureKind::Settlement, MapGeneratedFeatureKind::Landmark,
                                  MapGeneratedFeatureKind::District, MapGeneratedFeatureKind::BuildingGroup};
        selection.replacePlaceProjection = true;
        break;
    case 3:
        selection.featureKinds = {MapGeneratedFeatureKind::TerrainRegion, MapGeneratedFeatureKind::Vegetation,
                                  MapGeneratedFeatureKind::MountainRange, MapGeneratedFeatureKind::Coastline,
                                  MapGeneratedFeatureKind::WaterBody};
        selection.replaceTerrainProjection = true;
        break;
    }
    return selection;
}

void MapCreatorView::drawSelectableTileLegend(const std::vector<MapTileKind> &tiles)
{
    ImGui::BeginChild("MapLegend", ImVec2(0.0f, 0.0f), true);
    ImGui::TextUnformatted(localizedUiLabel("Terrain palette").c_str());
    ImGui::SameLine();
    drawHelpMarker("Click a legend tile to make it the active paint brush.", "?", "MapCreatorLegendHelp");
    ImGui::Separator();

    if (tiles.empty())
    {
        ImGui::TextWrapped(localizedUiLabel("Choose a map type to see usable elements.").c_str());
        ImGui::EndChild();
        return;
    }

    for (int tileIndex = 0; tileIndex < static_cast<int>(tiles.size()); ++tileIndex)
    {
        const MapTileKind kind = tiles[tileIndex];
        ImGui::PushID(tileIndex);
        const ImVec2 rowMin = ImGui::GetCursorScreenPos();
        const float rowWidth = std::max(120.0f, ImGui::GetContentRegionAvail().x);
        const float textWrapWidth = std::max(70.0f, rowWidth - 52.0f);
        const std::string tileLabel = localizedUiLabel(mapTileKindLabel(kind));
        const std::string purpose = localizedUiLabel(mapTileKindPurpose(kind));
        const float labelHeight = ImGui::CalcTextSize(tileLabel.c_str(), nullptr, false, textWrapWidth).y;
        const float purposeHeight = ImGui::CalcTextSize(purpose.c_str(), nullptr, false, textWrapWidth).y;
        const float rowHeight = std::max(54.0f, 9.0f + labelHeight + purposeHeight + 7.0f);
        const ImVec2 rowMax(rowMin.x + rowWidth, rowMin.y + rowHeight);
        const bool selected = _controller._editing.selectedTileIndex == tileIndex;

        if (selected)
            ImGui::GetWindowDrawList()->AddRectFilled(rowMin, rowMax, IM_COL32(34, 82, 86, 155), 4.0f);

        const ImVec2 swatchMin(rowMin.x + 7.0f, rowMin.y + 8.0f);
        const ImVec2 swatchMax(swatchMin.x + 30.0f, swatchMin.y + 30.0f);
        drawMapTile(ImGui::GetWindowDrawList(), swatchMin, swatchMax, kind, 0, false);
        ImGui::GetWindowDrawList()->AddRect(swatchMin, swatchMax,
                                            selected ? IM_COL32(250, 220, 110, 255) : IM_COL32(215, 224, 230, 190),
                                            3.0f, 0, selected ? 2.0f : 1.0f);
        ImGui::GetWindowDrawList()->AddText(nullptr, 0.0f, ImVec2(rowMin.x + 45.0f, rowMin.y + 7.0f),
                                            IM_COL32(235, 242, 245, 255), tileLabel.c_str(), nullptr, textWrapWidth);
        ImGui::GetWindowDrawList()->AddText(nullptr, 0.0f, ImVec2(rowMin.x + 45.0f, rowMin.y + 9.0f + labelHeight),
                                            IM_COL32(150, 158, 164, 255), purpose.c_str(), nullptr, textWrapWidth);

        ImGui::InvisibleButton("TileLegendRow", ImVec2(rowWidth, rowHeight));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(localizedUiLabel("Paint with %s. %s").c_str(), tileLabel.c_str(), purpose.c_str());
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            _controller.selectTileFromLegend(tileIndex);
        ImGui::PopID();
    }

    ImGui::EndChild();
}

void MapCreatorView::drawMapSidePanels()
{
    ensureDefaultMapLayer(_controller._draftMap);
    _controller.normalizeObjectSelection();

    ImGui::Separator();
    using Tool = MapCreatorController::Tool;
    if (_controller.hasSelectedObject() || _controller._editing.activeTool == Tool::InspectSelect)
    {
        drawSelectedObjectPanel();
        if (_controller._editing.selectedGeneratedFeatureIndex >= 0)
            drawGeneratedPlacePromotionPanel();
    }
    else if (_controller._editing.activeTool == Tool::Fog)
    { /* Fog controls are in the active tool panel. */
    }
    else if (_controller._editing.activeTool == Tool::PlaceToken)
        drawTokenSetupPanel();
    else if (_controller._editing.activeTool == Tool::AddFeature)
        drawFeatureNoteSetupPanel();
    else if (_controller._editing.activeTool == Tool::AddRegion)
        drawRegionSetupPanel();
    else if (_controller._editing.activeTool == Tool::AddRoute)
        drawRouteSetupPanel();
}
