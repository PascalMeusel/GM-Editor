#pragma once

#include "Models/Settings/AccessibilitySettings.hpp"

#include <string>

namespace gm::settings
{
// The application applies these values at the start of every ImGui frame.
// Keeping this small bridge outside the View menu lets the Settings workspace
// expose the same controls without maintaining a second copy of the state.
AccessibilitySettings activeAccessibilitySettings();
bool applyAccessibilitySettings(const AccessibilitySettings &settings,
                                std::string *error = nullptr);
void requestKeyboardHelp();
} // namespace gm::settings
