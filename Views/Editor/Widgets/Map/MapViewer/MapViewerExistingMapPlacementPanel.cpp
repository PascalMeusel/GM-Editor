#include "Views/Editor/Widgets/Map/MapViewer/MapViewerViewDetails.hpp"
#include "Views/Editor/Widgets/Map/MapViewer/MapViewerViewInternal.hpp"

using namespace gm::map_viewer_detail;

void MapViewerView::drawExistingMapPlacementPanel(const MapModel &parent)
{
    std::vector<const MapModel *> candidates;
    std::vector<std::string> names;
    std::vector<const char *> labels;
    for (const MapModel &candidate : _controller._currentCampaign.maps)
    {
        if (candidate.metadata.id == parent.metadata.id || candidate.isMainMap ||
            !candidate.atlasPlacement.parentId.empty() ||
            !MapAtlasService::isDeeperLevel(candidate.atlasLevel, parent.atlasLevel))
            continue;
        candidates.push_back(&candidate);
        std::string name = candidate.name.empty() ? localizedUiLabel("Untitled map") : candidate.name;
        name += " [" + localizedUiLabel(atlasLevelLabel(candidate.atlasLevel)) + "]";
        names.push_back(std::move(name));
    }
    for (const std::string &name : names)
        labels.push_back(name.c_str());

    ImGui::SeparatorText(uiTextForLocale("Place an existing map here", "Bestehende Karte hier platzieren"));
    if (candidates.empty())
    {
        ImGui::TextWrapped("%s", uiTextForLocale(
            "No unplaced maps at a deeper atlas level are available. Create a linked detail map below.",
            "Keine unplatzierten Karten auf einer tieferen Atlasebene vorhanden. Erstelle unten eine verknüpfte Detailkarte."));
        return;
    }
    _existingMapToPlaceIndex = std::clamp(_existingMapToPlaceIndex, 0, static_cast<int>(candidates.size()) - 1);
    FormUi::Combo(uiTextForLocale("Existing map", "Bestehende Karte"), &_existingMapToPlaceIndex,
                  labels.data(), static_cast<int>(labels.size()));
    ImGui::TextWrapped("%s", uiTextForLocale(
        "This locates the map inside the selected area. Its artwork and contents are preserved; geography is not regenerated.",
        "Die Karte wird im ausgewählten Bereich verortet. Gestaltung und Inhalte bleiben erhalten; die Landschaft wird nicht neu erzeugt."));
    if (drawUiButton(uiTextForLocale("Review placement", "Platzierung prüfen"), UiButtonRole::Secondary,
                     ImVec2(-1.0f, 0.0f)))
    {
        _pendingExistingMapId = candidates[static_cast<std::size_t>(_existingMapToPlaceIndex)]->metadata.id;
        ImGui::OpenPopup("##PlaceExistingMapReview");
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(460.0f, 0.0f), ImVec2(560.0f, 10000.0f));
    if (ImGui::BeginPopupModal("##PlaceExistingMapReview", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        const MapModel *chosen = _controller.findMapById(_pendingExistingMapId);
        ImGui::TextWrapped("%s", chosen ? chosen->name.c_str() : uiTextForLocale("Map is no longer available", "Karte ist nicht mehr verfügbar"));
        ImGui::TextWrapped("%s", uiTextForLocale(
            "The selected area becomes this map's geographic location. Existing map content is unchanged.",
            "Der ausgewählte Bereich wird zur geografischen Position dieser Karte. Bestehende Karteninhalte bleiben unverändert."));
        if (drawModalActionButtons(uiTextForLocale("Place map", "Karte platzieren"), UiButtonRole::Primary) && chosen)
            _controller.placeExistingMap(_pendingExistingMapId);
        ImGui::EndPopup();
    }
}
