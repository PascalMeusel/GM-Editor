#include "NPCOverviewViewMonsterSupport.hpp"
#include "Views/Editor/Widgets/Magic/SpellcastingValueUi.hpp"
#include "Views/Editor/Presentation/SelectedPlayerPreview.hpp"

void NPCOverviewView::drawNpcTab()
{
#include "NPCOverviewNpcDirectoryPanel.hpp"
    reconcileReadingContext();
    if (_reading)
    {
        drawNpcReading();
        if (!compact) ImGui::EndTable();
        return;
    }
#include "NPCOverviewNpcProfilePanel.hpp"
#include "NPCOverviewNpcMagicPanel.hpp"
#include "NPCOverviewNpcConnectionsPanel.hpp"
}
