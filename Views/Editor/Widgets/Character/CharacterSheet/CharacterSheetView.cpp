#include "CharacterSheetViewSummarySupport.hpp"
#include "Views/Editor/Widgets/Magic/SpellcastingValueUi.hpp"

CharacterSheetView::CharacterSheetView(CharacterSheetController &controller)
    : DockableWidget("Character Sheets", 0, ImVec2(1080.0f, 700.0f)), _controller(controller)
{
}

void CharacterSheetView::draw()
{
#include "CharacterSheetDirectorySummaryPanel.hpp"
#include "CharacterSheetProfilePanel.hpp"
#include "CharacterSheetCombatAdvancedPanel.hpp"
#include "CharacterSheetMagicPanel.hpp"
#include "CharacterSheetNotesStoryPanel.hpp"
#include "CharacterSheetInventoryPanel.hpp"
}
