#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"

using namespace gm::map_creator_view_detail;

void MapCreatorView::drawMapOutline()
{
    MapModel &map = _controller._draftMap;
    const char *groups[] = {uiTextForLocale("Places", "Orte"), uiTextForLocale("Geography", "Geografie"),
                            uiTextForLocale("Routes", "Wege"), uiTextForLocale("Notes", "Notizen")};
    const auto generatedGroup = [](MapGeneratedFeatureKind kind) {
        switch (kind)
        {
        case MapGeneratedFeatureKind::Settlement:
        case MapGeneratedFeatureKind::Landmark:
        case MapGeneratedFeatureKind::BuildingGroup:
        case MapGeneratedFeatureKind::TacticalSetpiece:
            return 0;
        case MapGeneratedFeatureKind::Road:
        case MapGeneratedFeatureKind::Trail:
        case MapGeneratedFeatureKind::Bridge:
            return 2;
        default:
            return 1;
        }
    };
    const auto select = [this]() {
        finishGeneratedFeatureGeometryEdit();
        _controller.endBrushStroke();
        _controller.setActiveTool(MapCreatorController::Tool::InspectSelect);
        _toolCategory = 0;
    };
    for (int group = 0; group < 4; ++group)
    {
        if (!ImGui::TreeNode(groups[group]))
            continue;
        bool any = false;
        for (int index = 0; index < static_cast<int>(map.generation.graph.features.size()); ++index)
        {
            const MapGeneratedFeature &feature = map.generation.graph.features[index];
            if (generatedGroup(feature.kind) != group)
                continue;
            any = true;
            const std::string name = feature.name.empty() ? localizedUiLabel(generatedFeatureKindLabel(feature.kind))
                                                          : localizedGeneratedFeatureName(feature);
            ImGui::PushID(feature.id.c_str());
            if (ImGui::Selectable(name.c_str(), _controller._editing.selectedGeneratedFeatureIndex == index))
            {
                select();
                resetGeneratedFeatureGeometryInteraction();
                _controller.selectGeneratedFeatureObject(index);
            }
            if (feature.locked)
                showHelpTooltip("This feature is locked. Unlock it before changing its geometry.");
            ImGui::PopID();
        }
        if (group == 0)
            for (int index = 0; index < static_cast<int>(map.tokens.size()); ++index)
            {
                any = true;
                const auto &token = map.tokens[index];
                ImGui::PushID(token.metadata.id.c_str());
                if (ImGui::Selectable(token.name.empty() ? localizedUiText("Token") : token.name.c_str(),
                                      _controller._editing.selectedTokenIndex == index))
                {
                    select();
                    _controller.selectTokenObject(index);
                }
                ImGui::PopID();
            }
        if (group == 1)
            for (int index = 0; index < static_cast<int>(map.regions.size()); ++index)
            {
                any = true;
                const auto &region = map.regions[index];
                ImGui::PushID(region.metadata.id.c_str());
                if (ImGui::Selectable(region.name.empty() ? localizedUiText("Region") : region.name.c_str(),
                                      _controller._editing.selectedRegionIndex == index))
                {
                    select();
                    _controller.selectRegionObject(index);
                }
                ImGui::PopID();
            }
        if (group == 2)
            for (int index = 0; index < static_cast<int>(map.routes.size()); ++index)
            {
                any = true;
                const auto &route = map.routes[index];
                ImGui::PushID(route.metadata.id.c_str());
                if (ImGui::Selectable(route.name.empty() ? localizedUiText("Route") : route.name.c_str(),
                                      _controller._editing.selectedRouteIndex == index))
                {
                    select();
                    _controller.selectRouteObject(index);
                }
                ImGui::PopID();
            }
        if (group == 0 || group == 3)
            for (int index = 0; index < static_cast<int>(map.features.size()); ++index)
            {
                const auto &feature = map.features[index];
                if ((group == 0) != (feature.kind == MapFeatureKind::NamedLocation))
                    continue;
                any = true;
                ImGui::PushID(feature.metadata.id.c_str());
                if (ImGui::Selectable(feature.name.empty() ? localizedUiText(mapFeatureKindLabel(feature.kind))
                                                           : feature.name.c_str(),
                                      _controller._editing.selectedFeatureIndex == index))
                {
                    select();
                    _controller.selectFeatureObject(index);
                }
                ImGui::PopID();
            }
        if (!any)
            ImGui::TextDisabled("%s", uiTextForLocale("Nothing here yet", "Noch keine Einträge"));
        ImGui::TreePop();
    }
}

std::string MapCreatorView::inspectorContext() const
{
    using Kind = MapCreatorController::SelectedObjectKind;
    const MapModel &map = _controller._draftMap;
    const Kind kind = _controller.selectedObjectKind();
    std::string objectId;
    switch (kind)
    {
    case Kind::Token:
        objectId = map.tokens[_controller._editing.selectedTokenIndex].metadata.id;
        break;
    case Kind::Feature:
        objectId = map.features[_controller._editing.selectedFeatureIndex].metadata.id;
        break;
    case Kind::GeneratedFeature:
        objectId = map.generation.graph.features[_controller._editing.selectedGeneratedFeatureIndex].id;
        break;
    case Kind::Region:
        objectId = map.regions[_controller._editing.selectedRegionIndex].metadata.id;
        break;
    case Kind::Route:
        objectId = map.routes[_controller._editing.selectedRouteIndex].metadata.id;
        break;
    case Kind::None:
        break;
    }
    // Names are editable; identity keeps typing from jumping back to the top.
    return map.metadata.id + ":" + std::to_string(static_cast<int>(kind)) + ":" + objectId + ":" +
           std::to_string(static_cast<int>(_controller._editing.activeTool)) + ":" + std::to_string(_toolCategory);
}
