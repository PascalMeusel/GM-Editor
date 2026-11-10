#include "ApplicationInternal.hpp"

using namespace gm::application_detail;

namespace gm::application_detail
{
AccessibilityPreferences &accessibilityPreferences()
{
    static AccessibilityPreferences preferences;
    return preferences;
}

std::filesystem::path applicationSettingsPath()
{
    const std::filesystem::path localizationPath =
        gm::localization::LocalizationService::instance().settingsPath();
    return localizationPath.empty() ? gm::settings::AccessibilitySettingsStore::defaultPath()
                                    : localizationPath;
}

void loadAccessibilityPreferences(const std::filesystem::path &path)
{
    const gm::settings::AccessibilitySettingsLoadResult loaded =
        gm::settings::AccessibilitySettingsStore::load(path);
    AccessibilityPreferences &preferences = accessibilityPreferences();
    preferences.textScale = loaded.settings.textScale;
    preferences.highContrast = loaded.settings.highContrast;
    preferences.compactTables = loaded.settings.compactTables;
    preferences.alwaysShowFocus = loaded.settings.alwaysShowFocus;
    if (!loaded.succeeded())
        std::cerr << loaded.error << std::endl;
}

void persistAccessibilityPreferences()
{
    const AccessibilityPreferences &preferences = accessibilityPreferences();
    gm::settings::AccessibilitySettings stored;
    stored.textScale = preferences.textScale;
    stored.highContrast = preferences.highContrast;
    stored.compactTables = preferences.compactTables;
    stored.alwaysShowFocus = preferences.alwaysShowFocus;

    std::string error;
    if (!gm::settings::AccessibilitySettingsStore::save(applicationSettingsPath(), stored, &error) &&
        !error.empty())
        std::cerr << error << std::endl;
}

void setAccessibleTextScale(float scale)
{
    AccessibilityPreferences &preferences = accessibilityPreferences();
    const float clamped = std::clamp(scale, 0.90f, 1.50f);
    if (preferences.textScale == clamped)
        return;
    preferences.textScale = clamped;
    persistAccessibilityPreferences();
}

void toggleCompactTables()
{
    AccessibilityPreferences &preferences = accessibilityPreferences();
    preferences.compactTables = !preferences.compactTables;
    persistAccessibilityPreferences();
}
void ApplyAccessibilityPreferences(ImGuiIO &io)
{
    const AccessibilityPreferences &preferences = accessibilityPreferences();
    io.FontGlobalScale = 1.0f;
    io.ConfigNavCursorVisibleAlways = preferences.alwaysShowFocus;
    io.ConfigNavEscapeClearFocusItem = !preferences.alwaysShowFocus;
    ImGui::GetStyle().FontScaleMain = preferences.textScale;
    ApplyEditorSizing(preferences.compactTables);
    ApplyEditorPalette(preferences.highContrast);
}

void adjustAccessibleTextScale(float delta)
{
    setAccessibleTextScale(accessibilityPreferences().textScale + delta);
}

const char *accessibilityScaleLabel(float scale, bool german)
{
    if (scale >= 1.40f)
        return german ? "Maximal (150 %)" : "Maximum (150%)";
    if (scale >= 1.24f)
        return german ? "Sehr groß" : "Extra Large";
    if (scale >= 1.09f)
        return german ? "Groß" : "Large";
    if (scale <= 0.95f)
        return german ? "Kompakt" : "Compact";
    return german ? "Standard" : "Default";
}

void constrainAutoSizedWindowToViewport(float preferredWidth)
{
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    const float maximumWidth = std::max(320.0f, viewport->WorkSize.x * 0.90f);
    const float maximumHeight = std::max(240.0f, viewport->WorkSize.y * 0.90f);
    ImGui::SetNextWindowSize(ImVec2(std::min(preferredWidth, maximumWidth), 0.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(std::min(320.0f, maximumWidth), 0.0f),
                                        ImVec2(maximumWidth, maximumHeight));
}

void RenderKeyboardHelpOverlay()
{
    AccessibilityPreferences &preferences = accessibilityPreferences();
    if (!preferences.showKeyboardHelp)
        return;

    const bool german = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
    const auto text = [german](const char *english, const char *germanText) { return german ? germanText : english; };
    constrainAutoSizedWindowToViewport(500.0f);
    if (ImGui::Begin(text("Keyboard Help", "Tastaturhilfe"), &preferences.showKeyboardHelp, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted(text("Navigation", "Navigation"));
        ImGui::Separator();
        ImGui::BulletText("%s", text("Tab / Shift+Tab: move focus", "Tab / Umschalt+Tab: Fokus bewegen"));
        ImGui::BulletText("%s", text("Arrow keys: move within menus, lists, tabs, and sliders", "Pfeiltasten: in Menüs, Listen, Reitern und Reglern bewegen"));
        ImGui::BulletText("%s", text("Enter or Space: activate the focused control", "Eingabe oder Leertaste: fokussiertes Element aktivieren"));
        ImGui::BulletText("%s", text("Ctrl+1…8: open Home through Settings", "Strg+1…8: Start bis Einstellungen öffnen"));
        ImGui::BulletText("%s", text("Esc: close popups or clear the focused item", "Esc: Dialog schließen oder Fokus lösen"));
        ImGui::Spacing();
        ImGui::TextUnformatted(text("Editor Shortcuts", "Editor-Tastenkürzel"));
        ImGui::Separator();
        ImGui::BulletText("%s", text("Ctrl+S: save campaign", "Strg+S: Kampagne speichern"));
        ImGui::BulletText("%s", text("Ctrl+O: open campaign", "Strg+O: Kampagne öffnen"));
        ImGui::BulletText("%s", text("Ctrl+Alt+M: back to Campaigns", "Strg+Alt+M: zurück zu Kampagnen"));
        ImGui::BulletText("%s", text("Ctrl+Q: exit program", "Strg+Q: Programm beenden"));
        ImGui::BulletText("%s", text("Ctrl+K: search and commands", "Strg+K: Suche und Befehle"));
        ImGui::BulletText("%s", text("Ctrl+R: quick d20 roll", "Strg+R: schneller W20-Wurf"));
        ImGui::BulletText("%s", text("Ctrl+Enter: next combat turn", "Strg+Eingabe: nächster Kampfzug"));
        ImGui::BulletText("%s", text("Ctrl+Shift+N: campaign notes", "Strg+Umschalt+N: Kampagnennotizen"));
        ImGui::BulletText("%s", text("Ctrl+M: mute or unmute audio", "Strg+M: Audio stummschalten"));
        ImGui::Spacing();
        ImGui::TextUnformatted(text("Accessibility", "Barrierefreiheit"));
        ImGui::Separator();
        ImGui::BulletText("%s", text("F1: show or hide this help", "F1: diese Hilfe ein- oder ausblenden"));
        ImGui::BulletText("%s", text("Ctrl+= / Ctrl+-: adjust text size", "Strg+= / Strg+-: Textgröße anpassen"));
        ImGui::BulletText("%s", text("Ctrl+0: reset text size", "Strg+0: Textgröße zurücksetzen"));
        ImGui::BulletText("%s", text("Ctrl+Shift+C: compact table mode", "Strg+Umschalt+C: kompakte Tabellen"));
        ImGui::Spacing();
        if (ImGui::Button(text("Close", "Schließen"), ImVec2(120.0f, 0.0f)))
            preferences.showKeyboardHelp = false;
        ImGui::SetItemDefaultFocus();
    }
    ImGui::End();
}

void RenderAccessibilityMenu()
{
    AccessibilityPreferences &preferences = accessibilityPreferences();
    const bool german = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
    const auto text = [german](const char *english, const char *germanText) { return german ? germanText : english; };
    if (!ImGui::BeginMenu(text("Accessibility", "Barrierefreiheit")))
        return;

    ImGui::Text("%s: %s", text("Text Size", "Textgröße"), accessibilityScaleLabel(preferences.textScale, german));
    if (ImGui::MenuItem(text("Compact", "Kompakt"), nullptr, preferences.textScale <= 0.95f))
        setAccessibleTextScale(0.92f);
    if (ImGui::MenuItem(text("Default", "Standard"), "Ctrl+0", preferences.textScale > 0.95f && preferences.textScale < 1.09f))
        setAccessibleTextScale(1.0f);
    if (ImGui::MenuItem(text("Large", "Groß"), nullptr, preferences.textScale >= 1.09f && preferences.textScale < 1.24f))
        setAccessibleTextScale(1.15f);
    if (ImGui::MenuItem(text("Extra Large", "Sehr groß"), nullptr, preferences.textScale >= 1.24f && preferences.textScale < 1.40f))
        setAccessibleTextScale(1.30f);
    if (ImGui::MenuItem(text("Maximum (150%)", "Maximal (150 %)"), nullptr, preferences.textScale >= 1.40f))
        setAccessibleTextScale(1.50f);
    ImGui::Separator();
    if (ImGui::MenuItem(text("Increase Text Size", "Text vergrößern"), "Ctrl+=", false, preferences.textScale < 1.50f))
        adjustAccessibleTextScale(0.05f);
    if (ImGui::MenuItem(text("Decrease Text Size", "Text verkleinern"), "Ctrl+-", false, preferences.textScale > 0.90f))
        adjustAccessibleTextScale(-0.05f);
    ImGui::Separator();
    if (ImGui::MenuItem(text("High Contrast", "Hoher Kontrast"), nullptr, &preferences.highContrast))
        persistAccessibilityPreferences();
    if (ImGui::MenuItem(text("Always Show Focus", "Fokus immer anzeigen"), nullptr, &preferences.alwaysShowFocus))
        persistAccessibilityPreferences();
    if (ImGui::MenuItem(text("Compact Table Mode", "Kompakte Tabellen"), "Ctrl+Shift+C", &preferences.compactTables))
        persistAccessibilityPreferences();
    if (ImGui::MenuItem(text("Keyboard Help", "Tastaturhilfe"), "F1"))
        preferences.showKeyboardHelp = true;
    ImGui::EndMenu();
}
} // namespace gm::application_detail

namespace gm::settings
{
AccessibilitySettings activeAccessibilitySettings()
{
    const AccessibilityPreferences &preferences = accessibilityPreferences();
    AccessibilitySettings settings;
    settings.textScale = preferences.textScale;
    settings.highContrast = preferences.highContrast;
    settings.compactTables = preferences.compactTables;
    settings.alwaysShowFocus = preferences.alwaysShowFocus;
    return settings;
}

bool applyAccessibilitySettings(const AccessibilitySettings &settings, std::string *error)
{
    if (error)
        error->clear();

    AccessibilitySettings normalized = settings;
    normalized.textScale = std::clamp(normalized.textScale, 0.90f, 1.50f);

    AccessibilityPreferences &preferences = accessibilityPreferences();
    preferences.textScale = normalized.textScale;
    preferences.highContrast = normalized.highContrast;
    preferences.compactTables = normalized.compactTables;
    preferences.alwaysShowFocus = normalized.alwaysShowFocus;
    return AccessibilitySettingsStore::save(applicationSettingsPath(), normalized, error);
}

void requestKeyboardHelp()
{
    accessibilityPreferences().showKeyboardHelp = true;
}
} // namespace gm::settings
