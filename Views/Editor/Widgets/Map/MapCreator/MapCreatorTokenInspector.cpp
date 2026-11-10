#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"

using namespace gm::map_creator_view_detail;

void MapCreatorView::drawSelectedTokenInspector() {     MapModel &map = _controller._draftMap;
        MapTokenModel &token = map.tokens[_controller._editing.selectedTokenIndex];
        undoableInputText("Token Name", token.name, "Edit Token");
        int tokenKind = static_cast<int>(token.kind);
        if (undoableCombo("Token Type", tokenKind, tokenKindItems, "Edit Token"))
            token.kind = static_cast<MapTokenKind>(tokenKind);
        undoableCheckbox("Hidden From Players", token.hiddenFromPlayers, "Edit Token");
        ImGui::TextDisabled("%s", uiTextForLocale("Drag the token on the map to place it.",
                                                   "Ziehe den Token auf der Karte zum Platzieren."));
        undoableSliderFloat("Token X", token.x, 0.0f, static_cast<float>(std::max(0, map.width - 1)), "%.3f",
                            "Edit Token");
        undoableSliderFloat("Token Y", token.y, 0.0f, static_cast<float>(std::max(0, map.height - 1)), "%.3f",
                            "Edit Token");
        undoableSliderFloat("Token Size", token.size, 0.5f, 4.0f, "%.1f", "Edit Token");
        if (ImGui::TreeNode(localizedUiLabel("Advanced").c_str()))
        {
            undoableInputText("Linked Entity Type", token.linkedEntityType, "Edit Token");
            undoableInputText("Linked Entity ID", token.linkedEntityId, "Edit Token");
            ImGui::TreePop();
        }
        if (drawUiButton("Remove Token", UiButtonRole::Destructive, ImVec2(-1.0f, 0.0f),
                         "Remove the selected token from this map."))
            ImGui::OpenPopup(localizedUiLabel("Remove Map Token").c_str());
        if (drawMapConfirmationModal("Remove Map Token", "Remove the selected map token?", "Remove"))
            _controller.removeSelectedToken();
}
