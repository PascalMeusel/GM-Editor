#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"

using namespace gm::map_creator_view_detail;

void MapCreatorView::drawGeneratedPlacePromotionPanel()
{
    using SelectedObjectKind = MapCreatorController::SelectedObjectKind;

    MapModel &map = _controller._draftMap;
    const auto canBecomeCampaignPlace = [](const MapGeneratedFeature &feature) {
        return feature.kind == MapGeneratedFeatureKind::Settlement || feature.kind == MapGeneratedFeatureKind::Landmark;
    };
    const bool hasGeneratedPlaces =
        std::any_of(map.generation.graph.features.begin(), map.generation.graph.features.end(), canBecomeCampaignPlace);
    if (!hasGeneratedPlaces)
        return;

    const bool selectedPlace =
        _controller.selectedObjectKind() == SelectedObjectKind::GeneratedFeature &&
        _controller._editing.selectedGeneratedFeatureIndex >= 0 &&
        _controller._editing.selectedGeneratedFeatureIndex < static_cast<int>(map.generation.graph.features.size()) &&
        canBecomeCampaignPlace(
            map.generation.graph.features[static_cast<std::size_t>(_controller._editing.selectedGeneratedFeatureIndex)]);
    // World places already expose this action in their contextual inspector.
    // Do not repeat a second automatically expanded promotion panel below it.
    if (selectedPlace && map.atlasLevel == MapLevel::World)
        return;
    if (!beginSidePanel("Campaign world", false))
        return;

    ImGui::TextWrapped(
        "%s",
        localizedUiLabel(
            "Generated settlements and landmarks remain map features until you add the ones you want to the campaign world.")
            .c_str());
    drawWorldPlaceSharingHelp(map);
    ImGui::Spacing();

    const float rowHeight = ImGui::GetFrameHeightWithSpacing() * 3.35f;
    const std::size_t placeCount = static_cast<std::size_t>(std::count_if(
        map.generation.graph.features.begin(), map.generation.graph.features.end(), canBecomeCampaignPlace));
    const float listHeight = std::min(rowHeight * static_cast<float>(selectedPlace ? 1 : placeCount),
                                      ImGui::GetFrameHeightWithSpacing() * 10.0f);

    if (ImGui::BeginChild("GeneratedPlacePromotionList", ImVec2(0.0f, listHeight), ImGuiChildFlags_None))
    {
        for (int featureIndex = 0; featureIndex < static_cast<int>(map.generation.graph.features.size());
             ++featureIndex)
        {
            const MapGeneratedFeature &feature = map.generation.graph.features[static_cast<std::size_t>(featureIndex)];
            if (selectedPlace && featureIndex != _controller._editing.selectedGeneratedFeatureIndex)
                continue;
            if (!canBecomeCampaignPlace(feature))
                continue;

            ImGui::PushID(feature.id.c_str());
            const bool selected = _controller.selectedObjectKind() == SelectedObjectKind::GeneratedFeature &&
                                  _controller._editing.selectedGeneratedFeatureIndex == featureIndex;
            const std::string fallbackName = localizedUiLabel(
                feature.kind == MapGeneratedFeatureKind::Settlement ? "Unnamed settlement" : "Unnamed landmark");
            const std::string displayName =
                feature.name.empty() ? fallbackName : localizedGeneratedFeatureName(feature);
            if (ImGui::Selectable(displayName.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick))
            {
                resetGeneratedFeatureGeometryInteraction();
                _controller.selectGeneratedFeatureObject(featureIndex);
            }
            ImGui::TextDisabled(
                "%s",
                localizedUiLabel(feature.kind == MapGeneratedFeatureKind::Settlement ? "City" : "Landmark").c_str());

            const WorldInspector::GeneratedPlacePromotionEligibility eligibility =
                WorldInspector::inspectGeneratedPlacePromotion(_controller._currentCampaign, map, feature.id);
            const std::string localizedReason = gm::localization::trSystemMessage(eligibility.message);
            if (eligibility.status == WorldInspector::GeneratedPlacePromotionStatus::AlreadyPromoted)
            {
                const bool revealed = map.atlasLevel != MapLevel::World ||
                                      mapCreatorPlayerOutputFeatureIsPublic(_controller._currentCampaign, map, feature);
                ImGui::TextWrapped(
                    "%s", revealed ? localizedUiLabel("Added to campaign world").c_str()
                                   : uiTextForLocale("Linked, but not revealed. Review it under World > Places.",
                                                     "Verknüpft, aber nicht enthüllt. Unter Welt > Orte prüfen."));
            }
            else
            {
                if (drawUiButton("Add to campaign world", eligibility.eligible(), localizedReason.c_str(),
                                 UiButtonRole::Primary, ImVec2(-1.0f, 0.0f),
                                 "Create a linked campaign place and save it with this campaign."))
                {
                    _controller.promoteGeneratedPlace(feature.id);
                }
                if (!eligibility.eligible())
                    ImGui::TextDisabled("%s", localizedReason.c_str());
            }

            ImGui::Separator();
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    ImGui::TreePop();
}
