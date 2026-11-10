#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"

using namespace gm::map_creator_view_detail;

void MapCreatorView::drawSelectedRegionInspector() {     MapModel &map = _controller._draftMap;
        MapRegionModel &region = map.regions[_controller._editing.selectedRegionIndex];
        ImGui::TextWrapped(
            "%s",
            uiTextForLocale(
                "Drag an edge or corner to resize. Drag inside to move. Esc cancels.",
                "Ziehe eine Kante oder Ecke zum Vergrößern oder Verkleinern. Innen ziehen verschiebt. Esc bricht ab."));
        undoableInputText("Region Name", region.name, "Edit Region");
        undoableInputTextMultiline("Region Description", region.description, ImVec2(-1.0f, 82.0f), "Edit Region");
        if (ImGui::TreeNode(localizedUiLabel("Advanced").c_str()))
        {
            undoableInputInt("Region X", region.x, "Edit Region");
            undoableInputInt("Region Y", region.y, "Edit Region");
            undoableInputInt("Region Width", region.width, "Edit Region");
            undoableInputInt("Region Height", region.height, "Edit Region");
            ImGui::TreePop();
        }
        region.width = std::max(1, region.width);
        region.height = std::max(1, region.height);
        undoableInputInt("Movement Cost", region.movementCost, "Edit Region");
        region.movementCost = std::max(1, region.movementCost);
        int lighting = std::clamp(static_cast<int>(region.lighting), 0, 3);
        if (undoableCombo("Lighting", lighting, lightingItems, "Edit Region"))
            region.lighting = static_cast<MapLightingMode>(lighting);
        undoableCheckbox("Difficult Terrain", region.difficultTerrain, "Edit Region");
        ImGui::SameLine();
        undoableCheckbox("Blocks Sight", region.blocksLineOfSight, "Edit Region");
        undoableCheckbox("GM Only Region", region.gmOnly, "Edit Region");
        if (drawUiButton("Remove Region", UiButtonRole::Destructive, ImVec2(-1.0f, 0.0f),
                         "Remove the selected region from this map."))
            ImGui::OpenPopup(localizedUiLabel("Remove Map Region").c_str());
        if (drawMapConfirmationModal("Remove Map Region", "Remove the selected map region?", "Remove"))
            _controller.removeSelectedRegion();
}
