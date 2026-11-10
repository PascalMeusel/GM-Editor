#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"

using namespace gm::map_creator_view_detail;

void MapCreatorView::drawTokenSetupPanel()
{
    using Tool = MapCreatorController::Tool;

    if (!beginSidePanel("Token Setup", _controller._editing.activeTool == Tool::PlaceToken))
        return;

    const std::string localizedTokenKinds = localizedComboItems(tokenKindItems);
    FormUi::Combo(localizedUiLabel("New Token Type").c_str(), &_controller._editing.selectedTokenKindIndex,
                  localizedTokenKinds.c_str());
    const std::vector<const char *> rawEntityLabels = _controller.getLinkedEntityLabels();
    std::vector<std::string> entityLabelStorage;
    entityLabelStorage.reserve(rawEntityLabels.size());
    for (const char *label : rawEntityLabels)
        entityLabelStorage.push_back(localizedModelPickerLabel(label));
    std::vector<const char *> entityLabels;
    entityLabels.reserve(entityLabelStorage.size());
    for (const std::string &label : entityLabelStorage)
        entityLabels.push_back(label.c_str());
    if (_controller._editing.selectedLinkedEntityIndex >= static_cast<int>(entityLabels.size()))
        _controller._editing.selectedLinkedEntityIndex = 0;
    FormUi::Combo(localizedUiLabel("Link Token To").c_str(), &_controller._editing.selectedLinkedEntityIndex,
                  entityLabels.data(), static_cast<int>(entityLabels.size()));

    ImGui::TreePop();
}

void MapCreatorView::drawFeatureNoteSetupPanel()
{
    using Tool = MapCreatorController::Tool;

    if (!beginSidePanel("Feature & Note Setup", _controller._editing.activeTool == Tool::AddFeature))
        return;

    const std::string localizedFeatureKinds = localizedComboItems(featureKindItems);
    FormUi::Combo(localizedUiLabel("New Feature Type").c_str(), &_controller._editing.selectedFeatureKindIndex,
                  localizedFeatureKinds.c_str());

    ImGui::TreePop();
}

void MapCreatorView::drawRegionSetupPanel()
{
    using Tool = MapCreatorController::Tool;

    if (!beginSidePanel("Region Setup", _controller._editing.activeTool == Tool::AddRegion))
        return;

    ImGui::TextWrapped(
        "%s",
        uiTextForLocale(
            "Left-drag to draw a region. Drag its edges or corners to resize; drag inside to move. Esc cancels.",
            "Ziehe mit der linken Maustaste eine Region auf. Kanten oder Ecken ändern die Größe, innen ziehen verschiebt. Esc bricht ab."));
    const std::string localizedLighting = localizedComboItems(lightingItems);
    FormUi::Combo(localizedUiLabel("New Region Lighting").c_str(), &_controller._editing.selectedLightingIndex,
                  localizedLighting.c_str());

    ImGui::TreePop();
}

void MapCreatorView::drawRouteSetupPanel()
{
    using Tool = MapCreatorController::Tool;

    if (!beginSidePanel("Route Setup", _controller._editing.activeTool == Tool::AddRoute))
        return;

    if (drawUiButton("Add Route", UiButtonRole::Primary, ImVec2(-1.0f, 0.0f),
                     "Add a full-width route without clicking a start cell."))
        _controller.addRoute();

    ImGui::TreePop();
}
