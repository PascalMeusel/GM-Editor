#pragma once

#include "Views/Editor/Widgets/UiGuidanceTopics.hpp"

namespace UiGuidance
{
// Replace an existing hint with this line instead of adding another banner.
void drawPurpose(Topic topic);

// buttonLabel is optional, already-localized visible text. Topic and caller
// ID scope own stable IDs, independent of locale. Callers showing the same
// topic twice in one window must provide separate ImGui::PushID scopes.
// Help is generic and read-only, including when invoked inside another popup.
void drawDetails(Topic topic, const char *buttonLabel = nullptr);
}
