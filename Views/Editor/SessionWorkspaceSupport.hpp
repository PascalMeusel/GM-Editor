#pragma once

#include "Models/UX/CampaignNavigationResolver.hpp"
#include "Views/Editor/Widgets/UiAdaptiveFields.hpp"
#include "Models/UX/PreparationText.hpp"
#include "SessionWorkspace.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Project/EntityRegistry.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Session/SessionControlService.hpp"
#include "Views/Editor/WorkspaceUi.hpp"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include <algorithm>
#include <cctype>
#include <iterator>

namespace
{
const char *st(const char *en, const char *de)
{
    return gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German ? de : en;
}
bool action(const char *en, const char *de, bool primary = false)
{
    return drawUiButton(st(en, de), primary ? UiButtonRole::Primary : UiButtonRole::Secondary);
}
void sameLineIfFits(const char *text)
{
    const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    const float buttonWidth = ImGui::CalcTextSize(text).x + 2.0f * ImGui::GetStyle().FramePadding.x;
    if (ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + buttonWidth <= right)
        ImGui::SameLine();
}
bool contains(std::string haystack, std::string needle)
{
    const auto lower = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };
    std::transform(haystack.begin(), haystack.end(), haystack.begin(), lower);
    std::transform(needle.begin(), needle.end(), needle.begin(), lower);
    return haystack.find(needle) != std::string::npos;
}
void label(const char *en, const char *de) { ImGui::TextUnformatted(st(en, de)); ImGui::SetNextItemWidth(-1); }
}
