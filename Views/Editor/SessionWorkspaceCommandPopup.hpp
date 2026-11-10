#pragma once
#include "Views/Editor/Widgets/UiTaskLayout.hpp"
#include "Views/Editor/Widgets/UiPopupPlacement.hpp"

// These anchored command menus can grow after a persistence error. ImGui only
// chooses their best anchor when appearing; constrain later growth before Begin
// computes decorations and clipping, without moving a newly opened popup.
inline void prepareSessionCommandPopup(const char *label)
{
    TaskUi::preparePickerWindow();
    const auto *viewport=ImGui::GetWindowViewport();
    auto *window=PopupUi::establishedPopup(label);
    if(!window)return;
    // Use the pinned ImGui sizing function: it includes the menu's measured
    // wrapped content, padding and the constraints just set above.
    PopupUi::contain(window,viewport,ImGui::CalcWindowNextAutoFitSize(window));
}
