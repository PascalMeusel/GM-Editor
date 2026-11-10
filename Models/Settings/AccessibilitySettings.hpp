#pragma once

#include <filesystem>
#include <string>

namespace gm::settings
{
struct AccessibilitySettings
{
    float textScale = 1.0f;
    bool highContrast = false;
    bool compactTables = false;
    bool alwaysShowFocus = true;
};

struct AccessibilitySettingsLoadResult
{
    AccessibilitySettings settings;
    bool fileFound = false;
    bool textScaleFound = false;
    bool highContrastFound = false;
    bool compactTablesFound = false;
    bool alwaysShowFocusFound = false;
    std::string error;

    bool succeeded() const { return error.empty(); }
};

// Reads and updates only the accessibility keys in GMEditor.settings. Language,
// comments, blank lines, and settings owned by other subsystems are preserved.
class AccessibilitySettingsStore
{
public:
    static std::filesystem::path defaultPath();
    static AccessibilitySettingsLoadResult load(const std::filesystem::path &path);
    static bool save(const std::filesystem::path &path,
                     const AccessibilitySettings &settings,
                     std::string *error = nullptr);
};
} // namespace gm::settings
