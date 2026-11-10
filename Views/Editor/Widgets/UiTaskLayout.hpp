#pragma once

#include "imgui.h"
#include <algorithm>
#include <cstring>

// Layout decisions use the actual content region and current font, including
// application text scaling. A narrow inspector is not treated as a wide screen.
namespace TaskUi
{
inline bool hasRoomForColumns(float em = 58.0f)
{
    return ImGui::GetContentRegionAvail().x >= ImGui::GetFontSize() * em;
}

inline float listHeight(float fraction = 0.42f, float maximumEm = 22.0f)
{
    return std::max(ImGui::GetTextLineHeightWithSpacing() * 3.0f,
        std::min(ImGui::GetFontSize() * maximumEm,
                 ImGui::GetContentRegionAvail().y * fraction));
}

inline void sameLineIfFits(float nextWidth = 0.0f)
{
    const float width = nextWidth > 0.0f ? nextWidth : ImGui::GetFontSize() * 24.0f;
    const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    if (ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + width <= right)
        ImGui::SameLine();
}

inline float buttonWidth(const char *label)
{
    return ImGui::CalcTextSize(label, nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2.0f;
}

inline float equalColumnButtonsMinimumWidth(const char *firstLabel,
                                            const char *secondLabel,
                                            float horizontalFramePadding)
{
    const float firstWidth = ImGui::CalcTextSize(firstLabel, nullptr, true).x + horizontalFramePadding * 2.0f;
    const float secondWidth = ImGui::CalcTextSize(secondLabel, nullptr, true).x + horizontalFramePadding * 2.0f;
    const float cellContentWidth = std::max(firstWidth, secondWidth);
    return cellContentWidth * 2.0f + ImGui::GetStyle().CellPadding.x * 4.0f;
}

inline bool equalColumnButtonsFit(const char *firstLabel,
                                  const char *secondLabel,
                                  float horizontalFramePadding,
                                  float availableWidth = -1.0f)
{
    const float available = availableWidth >= 0.0f ? availableWidth : ImGui::GetContentRegionAvail().x;
    return available >= equalColumnButtonsMinimumWidth(firstLabel, secondLabel, horizontalFramePadding);
}

inline void sameLineForButton(const char *label)
{
    sameLineIfFits(buttonWidth(label));
}

// Keep the native button's ID, keyboard navigation and disabled behavior.
// Only its text rendering changes when the available width needs wrapping.
inline bool button(const char *label, ImVec2 requested = ImVec2(0, 0))
{
    const float available = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float natural = buttonWidth(label);
    const float width = requested.x < 0 ? available : std::min(available, requested.x > 0 ? requested.x : natural);
    if (natural <= width)
        return ImGui::Button(label, ImVec2(width, requested.y));
    const auto padding = ImGui::GetStyle().FramePadding;
    const float wrapWidth = std::max(1.0f, width - padding.x * 2.0f);
    const char *end = std::strstr(label, "##");
    if (!end) end = label + std::strlen(label);
    const auto textSize = ImGui::CalcTextSize(label, end, false, wrapWidth);
    const float height = std::max(requested.y, textSize.y + padding.y * 2.0f);
    const auto color = ImGui::GetColorU32(ImGuiCol_Text);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 0));
    const bool pressed = ImGui::Button(label, ImVec2(width, height));
    ImGui::PopStyleColor();
    const auto min = ImGui::GetItemRectMin();
    ImGui::GetWindowDrawList()->AddText(nullptr, 0.0f,
        ImVec2(min.x + padding.x, min.y + padding.y), color, label, end, wrapWidth);
    return pressed;
}

inline void fieldLabel(const char *label)
{
    ImGui::TextWrapped("%s", label);
    ImGui::SetNextItemWidth(-1.0f);
}

inline void preparePickerWindow()
{
    const auto *viewport = ImGui::GetWindowViewport();
    const float width = std::max(1.0f, std::min(ImGui::GetFontSize() * 32.0f, viewport->WorkSize.x * 0.9f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0f),
        ImVec2(width, std::max(1.0f, viewport->WorkSize.y * 0.85f)));
}
}
