#pragma once

#include "Application.hpp"

#include "Models/Core/ProductInfo.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Project/ApplicationPaths.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/ProjectCommand.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Session/PlayerDisplayBlackoutShortcut.hpp"
#include "Models/Settings/AccessibilityRuntime.hpp"
#include "Models/Settings/AccessibilitySettings.hpp"
#include "Views/Editor/ApprovedUi/ApprovedUiTheme.hpp"
#include "Views/Editor/EditorLayoutProfileService.hpp"
#include "Views/Editor/Widgets/DockableWidget.hpp"
#include "Views/Editor/Widgets/ImageTextureCache.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace gm::application_detail
{
extern std::atomic<StartupDiagnostics *> activeStartupDiagnostics;
extern std::atomic<unsigned int> recordedGlfwErrors;

void recordGlfwError(int code, const char *description);
void ensureMainContext(GLFWwindow *window, StartupDiagnostics &diagnostics);
std::string windowPresentationState(GLFWwindow *window);

struct AccessibilityPreferences
{
    float textScale = 1.0f;
    bool highContrast = false;
    bool alwaysShowFocus = true;
    bool showKeyboardHelp = false;
    bool compactTables = false;
};

AccessibilityPreferences &accessibilityPreferences();
std::filesystem::path applicationSettingsPath();
void loadAccessibilityPreferences(const std::filesystem::path &path);
void persistAccessibilityPreferences();
void setAccessibleTextScale(float scale);
void toggleCompactTables();

inline constexpr const char *kEditorDockSpaceName = "EditorSessionWorkspaceV2";
inline constexpr ImWchar kEditorGlyphRanges[] = {
    0x0020, 0x00FF,
    0x2000, 0x206F,
    0x20AC, 0x20AC,
    0x2190, 0x21FF,
    0x2212, 0x2212,
    0,
};
inline constexpr ImGuiDockNodeFlags kEditorShellRootDockFlags = ImGuiDockNodeFlags_NoDockingSplit |
                                                                ImGuiDockNodeFlags_NoDockingOverMe;
inline constexpr ImGuiDockNodeFlags kLockedShellDockFlags = ImGuiDockNodeFlags_NoResize |
                                                            ImGuiDockNodeFlags_NoUndocking |
                                                            ImGuiDockNodeFlags_NoDockingSplit |
                                                            ImGuiDockNodeFlags_NoDockingOverMe |
                                                            ImGuiDockNodeFlags_NoWindowMenuButton |
                                                            ImGuiDockNodeFlags_NoCloseButton |
                                                            ImGuiDockNodeFlags_NoTabBar;

void applyLockedShellDockFlags(ImGuiDockNode *node);
void applyEditorShellRootDockFlags(ImGuiID dockspaceId);
std::vector<ImGuiDockNode *> utilityRailLeaves(ImGuiID dockspaceId);
ImGuiDockNode *sidebarShellLeaf(ImGuiID dockspaceId);
void dockUtilityShellWindows(ImGuiID gameTimeDockId, ImGuiID audioDockId, ImGuiID miniMapDockId);
ImGuiID buildEditorShellDockLayout(ImGuiID dockspaceId, const ImVec2 &dockspaceSize, bool includeUtilityRail);

void LoadEditorFont(ImGuiIO &io);
void ApplyEditorPalette(bool highContrast);
void ApplyEditorSizing(bool compactTables);
void ApplyEditorTheme(ImGuiIO &io);
void ApplyAccessibilityPreferences(ImGuiIO &io);
void adjustAccessibleTextScale(float delta);
const char *accessibilityScaleLabel(float scale, bool german = false);
void constrainAutoSizedWindowToViewport(float preferredWidth);
void RenderKeyboardHelpOverlay();
void RenderAccessibilityMenu();
} // namespace gm::application_detail
