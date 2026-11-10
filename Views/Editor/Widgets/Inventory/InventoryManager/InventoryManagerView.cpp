#include "InventoryManagerViewSupport.hpp"
#include "Views/Editor/Presentation/SelectedPlayerPreview.hpp"

InventoryManagerView::InventoryManagerView(InventoryManagerController &controller)
    : DockableWidget("Inventory & Item Assignment", 0, ImVec2(980.0f, 680.0f)), _controller(controller)
{
}


void InventoryManagerView::draw()
{
    if (_controller._currentCampaign.presentationModeActive)
    {
        ImGui::TextWrapped("%s", uiTextForLocale("Inventory details are private. Leave presentation mode to view or edit them.",
            "Inventardetails sind privat. Beende den Präsentationsmodus, um sie anzusehen oder zu bearbeiten."));
        return;
    }
#include "InventoryManagerViewSelectionPanel.hpp"
#include "InventoryManagerViewEntriesPanel.hpp"
#include "InventoryManagerViewLootPanel.hpp"
}
