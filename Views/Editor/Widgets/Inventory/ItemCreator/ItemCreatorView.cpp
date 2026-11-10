#include "ItemCreatorViewSupport.hpp"
#include "Views/Editor/Presentation/SelectedPlayerPreview.hpp"

ItemCreatorView::ItemCreatorView(ItemCreatorController &controller)
    : DockableWidget("Item Catalog", 0, ImVec2(980.0f, 640.0f)), _controller(controller)
{
}


void ItemCreatorView::draw()
{
    if (_controller._currentCampaign.presentationModeActive)
    {
        ImGui::TextWrapped("%s", uiTextForLocale("Item drafts are private. Leave presentation mode to view or edit them.",
            "Gegenstandsentwürfe sind privat. Beende den Präsentationsmodus, um sie anzusehen oder zu bearbeiten."));
        return;
    }
    if(!_controller.draftRecoveryReady()) {
        ImGui::TextWrapped("%s",uiTextForLocale("Review Item & note drafts in the sidebar before editing. Existing recovery is retained.",
            "Prüfe zuerst Gegenstands- und Notizentwürfe in der Seitenleiste. Die Sicherung bleibt erhalten."));
        return;
    }
#include "ItemCreatorCatalogPanel.hpp"
#include "ItemCreatorEditorPanel.hpp"
#include "ItemCreatorCombatPanel.hpp"
}
