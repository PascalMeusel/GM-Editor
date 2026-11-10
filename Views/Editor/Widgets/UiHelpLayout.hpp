#pragma once

#include "Models/Localization/LocalizationService.hpp"
#include "Models/UX/ResponsiveLayout.hpp"
#include "imgui.h"
#include "Views/Editor/Widgets/UiModalLayout.hpp"
#include "Views/Editor/Widgets/UiTaskLayout.hpp"
#include <algorithm>
#include <string>
#include <string_view>

enum class UiButtonRole
{
    Primary,
    Secondary,
    Destructive,
    Advanced,
    IconOnly
};

struct UiStatusActionStrip
{
    const char *mode = nullptr;
    const char *selection = nullptr;
    const char *nextAction = nullptr;
    const char *help = nullptr;
};

enum class UiModalAction
{
    None,
    Confirm,
    Cancel
};

namespace
{
inline const char *uiTextOrFallback(const char *text, const char *fallback = "-")
{
    return text && text[0] != '\0' ? text : fallback;
}

inline const char *localizedUiText(const char *applicationText)
{
    return gm::localization::trUi(applicationText);
}

inline const char *uiTextForLocale(const char *english, const char *german)
{
    return gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
               ? german
               : english;
}

inline std::string localizedUiLabel(const char *applicationLabel)
{
    if (!applicationLabel)
        return {};
    const std::string_view label(applicationLabel);
    const std::size_t idOffset = label.find("##");
    if (idOffset == std::string_view::npos)
        return localizedUiText(applicationLabel);

    const std::string visible(label.substr(0, idOffset));
    return std::string(localizedUiText(visible.c_str())) + std::string(label.substr(idOffset));
}

inline std::string localizedComboItems(const char *applicationItems)
{
    std::string localized;
    if (!applicationItems)
        return localized;
    const char *item = applicationItems;
    while (*item != '\0')
    {
        localized.append(localizedUiText(item));
        localized.push_back('\0');
        item += std::char_traits<char>::length(item) + 1;
    }
    localized.push_back('\0');
    return localized;
}

inline void setResponsiveAutoSizeWindow(float preferredWidth, float minimumWidth = 320.0f)
{
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    const float maximumWidth = std::max(1.0f, viewport->WorkSize.x * 0.90f);
    const float maximumHeight = std::max(1.0f, viewport->WorkSize.y * 0.90f);
    ImGui::SetNextWindowSize(ImVec2(std::min(preferredWidth, maximumWidth), 0.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(std::min(minimumWidth, maximumWidth), 0.0f),
                                         ImVec2(maximumWidth, maximumHeight));
}

inline void setResponsiveWindowSize(ImVec2 preferredSize,
                                    ImVec2 minimumSize = ImVec2(320.0f, 240.0f))
{
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    const gm::ux::ResponsiveWindowLayout layout = gm::ux::responsiveWindowLayout(
        viewport->WorkSize.x,
        viewport->WorkSize.y,
        preferredSize.x,
        preferredSize.y,
        minimumSize.x,
        minimumSize.y);
    ImGui::SetNextWindowSize(ImVec2(layout.width, layout.height), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(layout.minimumWidth, layout.minimumHeight),
                                        ImVec2(layout.maximumWidth, layout.maximumHeight));
}

inline ImVec2 fitUiButtonSize(const char *label, ImVec2 requested)
{
    if (requested.x <= 0.0f)
        return requested;

    const float contentWidth = ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    requested.x = std::max(requested.x, contentWidth);
    if (availableWidth > 0.0f)
        requested.x = std::min(requested.x, availableWidth);
    return requested;
}

struct CompactUiDensityScope
{
    CompactUiDensityScope()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 4.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(6.0f, 4.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(6.0f, 4.0f));
    }

    ~CompactUiDensityScope()
    {
        ImGui::PopStyleVar(5);
    }
};

struct UiButtonStyleScope
{
    explicit UiButtonStyleScope(UiButtonRole role)
    {
        switch (role)
        {
        case UiButtonRole::Primary:
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.43f, 0.47f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.55f, 0.60f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.36f, 0.40f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.98f, 0.98f, 1.0f));
            _colorCount = 4;
            break;
        case UiButtonRole::Destructive:
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.20f, 0.18f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.68f, 0.26f, 0.22f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.44f, 0.15f, 0.14f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.98f, 0.94f, 0.92f, 1.0f));
            _colorCount = 4;
            break;
        case UiButtonRole::Advanced:
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.34f, 0.29f, 0.20f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.45f, 0.38f, 0.25f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.28f, 0.24f, 0.17f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.88f, 0.70f, 1.0f));
            _colorCount = 4;
            break;
        case UiButtonRole::IconOnly:
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 3.0f));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.31f, 0.34f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.42f, 0.46f, 1.0f));
            _styleVarCount = 1;
            _colorCount = 3;
            break;
        case UiButtonRole::Secondary:
        default:
            break;
        }
    }

    ~UiButtonStyleScope()
    {
        if (_colorCount > 0)
            ImGui::PopStyleColor(_colorCount);
        if (_styleVarCount > 0)
            ImGui::PopStyleVar(_styleVarCount);
    }

    int _colorCount = 0;
    int _styleVarCount = 0;
};

inline void drawSectionTitle(const char *label, ImVec4 color = ImVec4(0.88f, 0.68f, 0.36f, 1.0f))
{
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    ImGui::TextColored(color, localizedUiLabel("%s").c_str(), localizedUiText(label));
    ImGui::Separator();
}

inline void drawInlineLabelValue(const char *label, const char *value)
{
    const char *localizedLabel = localizedUiText(label);
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float labelWidth = ImGui::CalcTextSize(localizedLabel).x;
    const float minimumValueWidth = ImGui::GetFontSize() * 8.0f;
    const bool fitsInline = labelWidth + ImGui::GetStyle().ItemSpacing.x + minimumValueWidth <= availableWidth;
    ImGui::TextDisabled(localizedUiLabel("%s").c_str(), localizedLabel);
    if (fitsInline)
        ImGui::SameLine();
    else
        ImGui::Indent();
    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), uiTextOrFallback(value));
    if (!fitsInline)
        ImGui::Unindent();
}

inline void drawStatusActionStripCell(const char *label, const char *value)
{
    ImGui::TableNextColumn();
    ImGui::TextDisabled(localizedUiLabel("%s").c_str(), uiTextOrFallback(label));
    ImGui::SameLine();
    // Keep the regular desktop layout consistent with the stacked layout:
    // application-owned mode, selection, and next-action copy must be
    // localized in both variants. Unknown (including campaign-authored)
    // values still pass through the bounded literal catalog unchanged.
    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), localizedUiText(uiTextOrFallback(value)));
}
}
