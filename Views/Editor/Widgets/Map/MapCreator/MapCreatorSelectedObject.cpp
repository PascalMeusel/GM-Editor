#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"

using namespace gm::map_creator_view_detail;

void MapCreatorView::drawSelectedObjectPanel()
{
    using SelectedObjectKind = MapCreatorController::SelectedObjectKind;
    using Tool = MapCreatorController::Tool;

    MapModel &map = _controller._draftMap;
    _controller.normalizeObjectSelection();

    const bool hasObjects = !map.tokens.empty() || !map.features.empty() || !map.generation.graph.features.empty() ||
                            !map.regions.empty() || !map.routes.empty();
    const bool forceOpen = _controller._editing.activeTool == Tool::InspectSelect || _controller.hasSelectedObject();
    if (_drawingCompactInspector && _controller.hasSelectedObject())
        ImGui::TreePush("SelectedObjectInline");
    else if (!beginSidePanel("Selected Object", forceOpen))
        return;

    const auto drawObjectPicker = [&]() {
        std::string selectionSummary = localizedSelectedObjectSummary();
        // Normal selection happens on the map or in the human-oriented outline.
        // Retain the exhaustive picker for legacy/unnamed objects as an explicit advanced control.
        const bool showAllObjects =
            hasObjects && ImGui::TreeNode(uiTextForLocale("All objects (advanced)", "Alle Objekte (erweitert)"));
        if (showAllObjects && FormUi::BeginCombo(localizedUiLabel("Object").c_str(), selectionSummary.c_str()))
        {
            SelectedObjectKind kind = _controller.selectedObjectKind();
            if (!map.tokens.empty())
            {
                ImGui::TextDisabled(localizedUiLabel("Tokens").c_str());
                ImGui::PushID("TokenObjectPicker");
                for (int index = 0; index < static_cast<int>(map.tokens.size()); ++index)
                {
                    const MapTokenModel &token = map.tokens[index];
                    const std::string fallback = localizedUiLabel(mapTokenKindLabel(token.kind));
                    const std::string label =
                        formatLocalizedUi("Token: %s", token.name.empty() ? fallback.c_str() : token.name.c_str());
                    const bool selected = kind == SelectedObjectKind::Token && _controller._editing.selectedTokenIndex == index;
                    ImGui::PushID(index);
                    if (ImGui::Selectable(label.c_str(), selected))
                        _controller.selectTokenObject(index);
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                    ImGui::PopID();
                }
                ImGui::PopID();
            }
            if (!map.features.empty())
            {
                ImGui::TextDisabled(localizedUiLabel("Features").c_str());
                ImGui::PushID("FeatureObjectPicker");
                for (int index = 0; index < static_cast<int>(map.features.size()); ++index)
                {
                    const MapFeatureModel &feature = map.features[index];
                    const std::string fallback = localizedUiLabel(mapFeatureKindLabel(feature.kind));
                    const std::string label = formatLocalizedUi(
                        "Feature: %s", feature.name.empty() ? fallback.c_str() : feature.name.c_str());
                    const bool selected =
                        kind == SelectedObjectKind::Feature && _controller._editing.selectedFeatureIndex == index;
                    ImGui::PushID(index);
                    if (ImGui::Selectable(label.c_str(), selected))
                        _controller.selectFeatureObject(index);
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                    ImGui::PopID();
                }
                ImGui::PopID();
            }
            if (!map.generation.graph.features.empty())
            {
                ImGui::TextDisabled(localizedUiLabel("Generated features").c_str());
                ImGui::PushID("GeneratedFeatureObjectPicker");
                for (int index = 0; index < static_cast<int>(map.generation.graph.features.size()); ++index)
                {
                    const MapGeneratedFeature &feature = map.generation.graph.features[static_cast<std::size_t>(index)];
                    const std::string fallback = localizedUiLabel(generatedFeatureKindLabel(feature.kind));
                    const std::string name = feature.name.empty() ? fallback : localizedGeneratedFeatureName(feature);
                    const std::string label = formatLocalizedUi("Generated feature: %s", name.c_str());
                    const bool selected = kind == SelectedObjectKind::GeneratedFeature &&
                                          _controller._editing.selectedGeneratedFeatureIndex == index;
                    ImGui::PushID(index);
                    if (ImGui::Selectable(label.c_str(), selected))
                    {
                        resetGeneratedFeatureGeometryInteraction();
                        _controller.selectGeneratedFeatureObject(index);
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                    ImGui::PopID();
                }
                ImGui::PopID();
            }
            if (!map.regions.empty())
            {
                ImGui::TextDisabled(localizedUiLabel("Regions").c_str());
                ImGui::PushID("RegionObjectPicker");
                for (int index = 0; index < static_cast<int>(map.regions.size()); ++index)
                {
                    const MapRegionModel &region = map.regions[index];
                    const std::string fallback = localizedUiLabel("<Region>");
                    const std::string regionName =
                        region.name.empty() ? fallback : localizedGeneratedMapName(region.name);
                    const std::string label = formatLocalizedUi("Region: %s", regionName.c_str());
                    const bool selected =
                        kind == SelectedObjectKind::Region && _controller._editing.selectedRegionIndex == index;
                    ImGui::PushID(index);
                    if (ImGui::Selectable(label.c_str(), selected))
                        _controller.selectRegionObject(index);
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                    ImGui::PopID();
                }
                ImGui::PopID();
            }
            if (!map.routes.empty())
            {
                ImGui::TextDisabled(localizedUiLabel("Routes").c_str());
                ImGui::PushID("RouteObjectPicker");
                for (int index = 0; index < static_cast<int>(map.routes.size()); ++index)
                {
                    const MapRouteModel &route = map.routes[index];
                    const std::string fallback = localizedUiLabel("<Route>");
                    const std::string label =
                        formatLocalizedUi("Route: %s", route.name.empty() ? fallback.c_str() : route.name.c_str());
                    const bool selected = kind == SelectedObjectKind::Route && _controller._editing.selectedRouteIndex == index;
                    ImGui::PushID(index);
                    if (ImGui::Selectable(label.c_str(), selected))
                        _controller.selectRouteObject(index);
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                    ImGui::PopID();
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
            selectionSummary = localizedSelectedObjectSummary();
        }
        else if (!hasObjects)
        {
            drawDisabledWrappedText("No tokens, features, generated features, regions, or routes yet.");
        }
        if (showAllObjects)
            ImGui::TreePop();
    };

    SelectedObjectKind kind = _controller.selectedObjectKind();
    if (kind == SelectedObjectKind::None)
    {
        if (hasObjects)
            ImGui::TextDisabled(localizedUiLabel("No object selected.").c_str());
        drawObjectPicker();
        ImGui::TreePop();
        return;
    }

    ImGui::Separator();
    switch (kind)
    {
    case SelectedObjectKind::Token: drawSelectedTokenInspector(); break;
    case SelectedObjectKind::Feature: drawSelectedFeatureInspector(); break;
    case SelectedObjectKind::GeneratedFeature: drawSelectedGeneratedFeatureInspector(); break;
    case SelectedObjectKind::Region: drawSelectedRegionInspector(); break;
    case SelectedObjectKind::Route: drawSelectedRouteInspector(); break;
    case SelectedObjectKind::None: break;
    }


    if (_controller.hasSelectedObject() &&
        drawSmallUiButton("Clear Selection", UiButtonRole::Secondary, "Clear the selected map object."))
    {
        resetGeneratedFeatureGeometryInteraction();
        _controller.clearObjectSelection();
    }
    drawObjectPicker();
    ImGui::TreePop();
}
