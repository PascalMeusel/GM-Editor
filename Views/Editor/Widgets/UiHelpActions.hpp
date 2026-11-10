#pragma once
#include "Views/Editor/Widgets/UiHelpLayout.hpp"
#include "Views/Editor/Widgets/UiTooltipPolicy.hpp"

inline void showHelpTooltip(const char *text, ImGuiHoveredFlags hoverFlags = ImGuiHoveredFlags_DelayShort)
{
    if (!text || text[0] == '\0')
        return;

    const ImGuiIO &io = ImGui::GetIO();
    const bool tracksHoveredViewport =
        (io.BackendFlags & ImGuiBackendFlags_HasMouseHoveredViewport) != 0;
    const bool pointerInsideApplication =
        ImGui::IsMousePosValid(&io.MousePos) &&
        (!tracksHoveredViewport || io.MouseHoveredViewport != 0);
    const bool itemHovered = pointerInsideApplication && ImGui::IsItemHovered(hoverFlags);
    if (!UiTooltipPolicy::shouldShow(
            ImGui::IsItemActive(),
            pointerInsideApplication,
            itemHovered))
        return;

    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);
    ImGui::TextUnformatted(localizedUiText(text));
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

inline bool drawUiButton(
    const char *label,
    UiButtonRole role = UiButtonRole::Secondary,
    ImVec2 size = ImVec2(0.0f, 0.0f),
    const char *tooltip = nullptr)
{
    UiButtonStyleScope style(role);
    const std::string localizedLabel = localizedUiLabel(label);
    const bool pressed = TaskUi::button(localizedLabel.c_str(), fitUiButtonSize(localizedLabel.c_str(), size));
    showHelpTooltip(localizedUiText(tooltip));
    return pressed;
}

inline bool drawSmallUiButton(
    const char *label,
    UiButtonRole role = UiButtonRole::Secondary,
    const char *tooltip = nullptr)
{
    UiButtonStyleScope style(role);
    const std::string localizedLabel = localizedUiLabel(label);
    const bool pressed = ImGui::SmallButton(localizedLabel.c_str());
    showHelpTooltip(localizedUiText(tooltip));
    return pressed;
}

inline bool drawDisabledUiButton(
    const char *label,
    const char *disabledReason,
    UiButtonRole role = UiButtonRole::Secondary,
    ImVec2 size = ImVec2(0.0f, 0.0f))
{
    UiButtonStyleScope style(role);
    ImGui::BeginDisabled(true);
    const std::string localizedLabel = localizedUiLabel(label);
    TaskUi::button(localizedLabel.c_str(), fitUiButtonSize(localizedLabel.c_str(), size));
    ImGui::EndDisabled();
    showHelpTooltip(
        localizedUiText(uiTextOrFallback(disabledReason, "This action is not available right now.")),
        ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled);
    return false;
}

inline bool drawUiButton(
    const char *label,
    bool enabled,
    const char *disabledReason,
    UiButtonRole role = UiButtonRole::Secondary,
    ImVec2 size = ImVec2(0.0f, 0.0f),
    const char *tooltip = nullptr)
{
    if (!enabled)
        return drawDisabledUiButton(label, disabledReason, role, size);
    return drawUiButton(label, role, size, tooltip);
}

inline void drawHelpMarker(const char *tooltip, const char *label = "?", const char *id = nullptr)
{
    if (!tooltip || tooltip[0] == '\0')
        return;

    std::string markerId;
    if (id && id[0] != '\0')
        markerId = std::string(id) + "::" + tooltip;

    ImGui::PushID(markerId.empty() ? tooltip : markerId.c_str());
    drawUiButton(label, UiButtonRole::IconOnly, ImVec2(ImGui::GetFrameHeight(), 0.0f), tooltip);
    ImGui::PopID();
}

inline void drawInlineHelpMarker(const char *tooltip, const char *label = "?", const char *id = nullptr)
{
    if (!tooltip || tooltip[0] == '\0')
        return;

    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    drawHelpMarker(tooltip, label, id);
}

inline void drawHelpText(const char *label, const char *tooltip)
{
    ImGui::TextUnformatted(localizedUiText(label));
    showHelpTooltip(localizedUiText(tooltip));
    drawInlineHelpMarker(tooltip, "?", label);
}

inline void drawMutedHelpText(const char *label, const char *tooltip)
{
    ImGui::TextDisabled(localizedUiLabel("%s").c_str(), localizedUiText(label));
    showHelpTooltip(localizedUiText(tooltip));
    drawInlineHelpMarker(tooltip, "?", label);
}

inline void drawStatusActionStrip(const char *id, const UiStatusActionStrip &strip)
{
    ImGui::PushID(uiTextOrFallback(id, "Status"));
    std::string heading = localizedUiText(uiTextOrFallback(strip.mode, ""));
    if (strip.selection && strip.selection[0])
    {
        if (!heading.empty()) heading += " · ";
        heading += localizedUiText(strip.selection);
    }
    // One readable context line; detailed guidance is available on demand.
    // Tables with repeated Mode/Selection/Next labels consumed the working area.
    ImGui::TextWrapped("%s", heading.c_str());
    if (strip.nextAction && strip.nextAction[0])
    {
        ImGui::TextDisabled("%s", localizedUiText("Help"));
        ImGui::SameLine();
        std::string guidance = localizedUiText(strip.nextAction);
        if (strip.help && strip.help[0]) guidance += "\n\n" + std::string(localizedUiText(strip.help));
        drawHelpMarker(guidance.c_str(), "?", "Guidance");
    }
    ImGui::PopID();
}

inline void drawStatusActionStrip(
    const char *mode,
    const char *selection,
    const char *nextAction,
    const char *help = nullptr)
{
    UiStatusActionStrip strip;
    strip.mode = mode;
    strip.selection = selection;
    strip.nextAction = nextAction;
    strip.help = help;
    drawStatusActionStrip("##UiStatusActionStrip", strip);
}

inline UiModalAction drawModalActionButtonsEx(
    const char *confirmLabel,
    UiButtonRole confirmRole = UiButtonRole::Primary,
    const char *cancelLabel = "Cancel",
    float buttonWidth = 128.0f)
{
    UiModalAction action = UiModalAction::None;
    ImGui::Separator();
    const std::string localizedConfirm = localizedUiLabel(confirmLabel);
    const std::string localizedCancel = localizedUiLabel(cancelLabel);
    const float horizontalPadding = ImGui::GetStyle().FramePadding.x * 2.0f;
    const gm::ux::ResponsiveTwoActionLayout layout = gm::ux::responsiveTwoActionLayout(
        ImGui::GetContentRegionAvail().x,
        ImGui::GetStyle().ItemSpacing.x,
        std::max(buttonWidth, ImGui::CalcTextSize(localizedConfirm.c_str()).x + horizontalPadding),
        std::max(buttonWidth, ImGui::CalcTextSize(localizedCancel.c_str()).x + horizontalPadding));
    if (drawUiButton(confirmLabel, confirmRole, ImVec2(layout.firstWidth, 0.0f)))
    {
        action = UiModalAction::Confirm;
        ImGui::CloseCurrentPopup();
    }
    if (confirmRole != UiButtonRole::Destructive)
        ImGui::SetItemDefaultFocus();
    if (layout.inlineActions)
        ImGui::SameLine();
    if (drawUiButton(cancelLabel, UiButtonRole::Secondary, ImVec2(layout.secondWidth, 0.0f)))
    {
        action = UiModalAction::Cancel;
        ImGui::CloseCurrentPopup();
    }
    // Destructive dialogs default to the reversible choice when opened.
    if (confirmRole == UiButtonRole::Destructive)
        ImGui::SetItemDefaultFocus();
    return action;
}

inline bool drawModalActionButtons(
    const char *confirmLabel,
    UiButtonRole confirmRole = UiButtonRole::Primary,
    const char *cancelLabel = "Cancel",
    float buttonWidth = 128.0f)
{
    return drawModalActionButtonsEx(confirmLabel, confirmRole, cancelLabel, buttonWidth) == UiModalAction::Confirm;
}

inline bool beginProgressiveHeader(const char *label, bool defaultOpen = false, const char *tooltip = nullptr)
{
    ImGui::SetNextItemOpen(defaultOpen, ImGuiCond_Once);
    const bool open = ImGui::CollapsingHeader(localizedUiText(label));
    showHelpTooltip(localizedUiText(tooltip));
    return open;
}

inline bool beginContextHeader(const char *label, bool contextOpen, const char *tooltip = nullptr)
{
    return beginProgressiveHeader(label, contextOpen, tooltip);
}

inline bool beginAdvancedHeader(const char *label, const char *tooltip = nullptr)
{
    return beginProgressiveHeader(label, false, tooltip);
}

inline bool beginProgressiveTreeNode(const char *label, bool defaultOpen = false, const char *tooltip = nullptr)
{
    ImGui::SetNextItemOpen(defaultOpen, ImGuiCond_Once);
    const bool open = ImGui::TreeNodeEx(localizedUiText(label));
    showHelpTooltip(localizedUiText(tooltip));
    return open;
}

inline bool beginContextTreeNode(const char *label, bool contextOpen, const char *tooltip = nullptr)
{
    return beginProgressiveTreeNode(label, contextOpen, tooltip);
}

inline bool beginAdvancedTreeNode(const char *label, const char *tooltip = nullptr)
{
    return beginProgressiveTreeNode(label, false, tooltip);
}

inline void drawActiveMarker(bool active, const char *tooltip = nullptr)
{
    const float size = ImGui::GetTextLineHeight();
    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    if (active)
    {
        const ImVec2 center(cursor.x + size * 0.5f, cursor.y + size * 0.5f);
        ImGui::GetWindowDrawList()->AddCircleFilled(center, size * 0.24f, ImGui::GetColorU32(ImVec4(0.26f, 0.74f, 0.78f, 1.0f)));
    }
    ImGui::Dummy(ImVec2(size, size));
    if (active)
        showHelpTooltip(localizedUiText(uiTextOrFallback(tooltip, "Active item")));
}

inline bool drawTriangleIconButton(
    const char *id,
    bool pointingUp,
    UiButtonRole role = UiButtonRole::IconOnly,
    const char *tooltip = nullptr)
{
    const float size = ImGui::GetFrameHeight();
    UiButtonStyleScope style(role);
    const bool pressed = ImGui::Button(id, ImVec2(size, size));
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const float cx = (min.x + max.x) * 0.5f;
    const float cy = (min.y + max.y) * 0.5f;
    const float r = size * 0.22f;
    const ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
    if (pointingUp)
        ImGui::GetWindowDrawList()->AddTriangleFilled(ImVec2(cx, cy - r), ImVec2(cx - r, cy + r), ImVec2(cx + r, cy + r), color);
    else
        ImGui::GetWindowDrawList()->AddTriangleFilled(ImVec2(cx - r, cy - r), ImVec2(cx + r, cy - r), ImVec2(cx, cy + r), color);
    showHelpTooltip(localizedUiText(tooltip));
    return pressed;
}

inline bool drawConfirmationModal(const char *popupId, const char *message, const char *confirmLabel = "Delete")
{
    bool confirmed = false;
    if (beginBoundedConfirmationModal(popupId))
    {
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), localizedUiText(message));
        if (drawModalActionButtons(confirmLabel, UiButtonRole::Destructive, "Cancel", 110.0f))
            confirmed = true;
        ImGui::EndPopup();
    }
    return confirmed;
}

inline void tableTextWrapped(const std::string &text)
{
    const float wrapWidth = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
    ImGui::PushTextWrapPos(wrapWidth);
    ImGui::TextUnformatted(text.empty() ? "-" : text.c_str());
    ImGui::PopTextWrapPos();
    if (!text.empty())
        showHelpTooltip(text.c_str());
}

inline void tableTextWrapped(const char *text)
{
    tableTextWrapped(std::string(text ? text : ""));
}
