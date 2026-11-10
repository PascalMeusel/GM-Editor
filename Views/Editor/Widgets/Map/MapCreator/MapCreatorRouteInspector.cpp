#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"

using namespace gm::map_creator_view_detail;

void MapCreatorView::drawSelectedRouteInspector() {     MapModel &map = _controller._draftMap;
        MapRouteModel &route = map.routes[_controller._editing.selectedRouteIndex];
        undoableInputText("Route Name", route.name, "Edit Route");
        undoableInputFloat("Miles", route.distanceMiles, "Edit Route");
        undoableInputText("Terrain", route.terrain, "Edit Route");
        undoableInputTextMultiline("Route Notes", route.notes, ImVec2(-1.0f, 82.0f), "Edit Route");
        if (ImGui::TreeNode(localizedUiLabel("Advanced").c_str()))
        {
            undoableInputInt("Start X", route.startX, "Edit Route");
            undoableInputInt("Start Y", route.startY, "Edit Route");
            undoableInputInt("End X", route.endX, "Edit Route");
            undoableInputInt("End Y", route.endY, "Edit Route");
            ImGui::TreePop();
        }
        undoableCheckbox("Route Discovered", route.discovered, "Edit Route");
        if (drawUiButton("Remove Route", UiButtonRole::Destructive, ImVec2(-1.0f, 0.0f),
                         "Remove the selected route from this map."))
            ImGui::OpenPopup(localizedUiLabel("Remove Map Route").c_str());
        if (drawMapConfirmationModal("Remove Map Route", "Remove the selected map route?", "Remove"))
            _controller.removeSelectedRoute();
}
