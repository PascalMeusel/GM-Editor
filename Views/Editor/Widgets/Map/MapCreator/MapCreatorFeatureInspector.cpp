#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"

using namespace gm::map_creator_view_detail;

void MapCreatorView::drawSelectedFeatureInspector() {     MapModel &map = _controller._draftMap;
        MapFeatureModel &feature = map.features[_controller._editing.selectedFeatureIndex];
        undoableInputText("Feature Name", feature.name, "Edit Feature");
        int featureKind = static_cast<int>(feature.kind);
        if (undoableCombo("Feature Type", featureKind, featureKindItems, "Edit Feature"))
            feature.kind = static_cast<MapFeatureKind>(featureKind);
        undoableInputTextMultiline("Feature Text", feature.description, ImVec2(-1.0f, 82.0f), "Edit Feature");
        undoableInputTextMultiline("GM Notes", feature.gmNotes, ImVec2(-1.0f, 82.0f), "Edit Feature");
        if (ImGui::TreeNode(localizedUiLabel("Advanced").c_str()))
        {
            undoableInputInt("Feature X", feature.x, "Edit Feature");
            undoableInputInt("Feature Y", feature.y, "Edit Feature");
            ImGui::TreePop();
        }
        feature.x = std::clamp(feature.x, 0, std::max(0, map.width - 1));
        feature.y = std::clamp(feature.y, 0, std::max(0, map.height - 1));
        undoableInputInt("DC", feature.difficultyClass, "Edit Feature");
        undoableCheckbox("Discovered", feature.discovered, "Edit Feature");
        ImGui::SameLine();
        undoableCheckbox("GM Only", feature.gmOnly, "Edit Feature");
        if (drawUiButton("Remove Feature", UiButtonRole::Destructive, ImVec2(-1.0f, 0.0f),
                         "Remove the selected feature from this map."))
            ImGui::OpenPopup(localizedUiLabel("Remove Map Feature").c_str());
        if (drawMapConfirmationModal("Remove Map Feature", "Remove the selected map feature?", "Remove"))
            _controller.removeSelectedFeature();
}
