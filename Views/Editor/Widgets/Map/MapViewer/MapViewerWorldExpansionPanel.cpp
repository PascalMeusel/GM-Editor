#include "Views/Editor/Widgets/Map/MapViewer/MapViewerViewDetails.hpp"
#include "Models/Map/MapWorldExpansion.hpp"

void MapViewerView::drawWorldExpansionPanel(const MapModel &map)
{
    ImGui::SeparatorText(uiTextForLocale("Expand world map", "Weltkarte erweitern"));
    ImGui::TextDisabled("%d x %d", map.width, map.height);
    if (map.importedReference.enabled || !map.backgroundImagePath.empty())
    {
        ImGui::TextWrapped("%s", uiTextForLocale(
            "Image-backed worlds cannot be extended here. Edit the source image or create a linked region instead.",
            "Weltkarten mit Bildvorlage können hier nicht erweitert werden. Bearbeite das Quellbild oder erstelle stattdessen eine verknüpfte Region."));
        return;
    }

    const int remainingEast = std::max(0, MapWorldExpansion::maximumCellsPerSide - map.width);
    const int remainingSouth = std::max(0, MapWorldExpansion::maximumCellsPerSide - map.height);
    if (remainingEast == 0 && remainingSouth == 0)
    {
        ImGui::TextWrapped("%s", uiTextForLocale(
            "This world has reached the current 120-by-120-cell limit.",
            "Diese Welt hat die aktuelle Grenze von 120 mal 120 Feldern erreicht."));
        return;
    }
    _expandEastCells = std::clamp(_expandEastCells, 0, remainingEast);
    _expandSouthCells = std::clamp(_expandSouthCells, 0, remainingSouth);
    FormUi::InputInt(uiTextForLocale("New columns to the east", "Neue Spalten im Osten"), &_expandEastCells);
    FormUi::InputInt(uiTextForLocale("New rows to the south", "Neue Zeilen im Süden"), &_expandSouthCells);
    _expandEastCells = std::clamp(_expandEastCells, 0, remainingEast);
    _expandSouthCells = std::clamp(_expandSouthCells, 0, remainingSouth);
    ImGui::TextWrapped("%s", uiTextForLocale(
        "Existing places and child maps keep their coordinates. New cells start hidden until you draw or reveal them.",
        "Bestehende Orte und Detailkarten behalten ihre Koordinaten. Neue Felder bleiben verborgen, bis du sie gestaltest oder aufdeckst."));
    const bool canExpand = MapWorldExpansion::available(map, _expandEastCells, _expandSouthCells);
    if (drawUiButton(uiTextForLocale("Review expansion", "Erweiterung prüfen"), canExpand,
                     uiTextForLocale("Add at least one row or column.", "Füge mindestens eine Zeile oder Spalte hinzu."),
                     UiButtonRole::Primary, ImVec2(-1.0f, 0.0f)))
        ImGui::OpenPopup("##ExpandWorldMapReview");
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(460.0f, 0.0f), ImVec2(560.0f, 10000.0f));
    if (ImGui::BeginPopupModal("##ExpandWorldMapReview", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped("%s %d x %d  →  %d x %d",
                           uiTextForLocale("World map:", "Weltkarte:"), map.width, map.height,
                           map.width + _expandEastCells, map.height + _expandSouthCells);
        ImGui::TextWrapped("%s", uiTextForLocale(
            "The current map and its linked places stay in place. New cells begin hidden; republish the player display when ready.",
            "Die aktuelle Karte und ihre verknüpften Orte bleiben an ihrer Position. Neue Felder sind zunächst verborgen; veröffentliche die Spieleranzeige anschließend erneut."));
        if (drawModalActionButtons(uiTextForLocale("Expand world", "Welt erweitern"), UiButtonRole::Primary))
            _controller.expandWorldMap(_expandEastCells, _expandSouthCells);
        ImGui::EndPopup();
    }
}
