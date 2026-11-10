#pragma once
#include "imgui.h"
#include <algorithm>

// Wrapped modal text needs a width before auto-fit measures its height.
inline bool beginBoundedConfirmationModal(const char *popupId)
{
    if (!ImGui::IsPopupOpen(popupId)) return false;
    const ImGuiViewport *viewport = ImGui::GetWindowViewport();
    const float width = std::max(1.0f, std::min(ImGui::GetFontSize() * 38.0f, viewport->WorkSize.x * 0.90f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0f),
        ImVec2(width, std::max(1.0f, viewport->WorkSize.y * 0.90f)));
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
                                viewport->WorkPos.y + viewport->WorkSize.y * 0.5f),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    return ImGui::BeginPopupModal(popupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize);
}

// Reserve one line even after a transient success notice expires. Persistent
// errors remain separate wrapped text and are never clipped by this strip.
inline void drawStableTransientStatus(const char *id, const char *message, bool visible)
{
    ImGui::BeginChild(id, ImVec2(0.0f, ImGui::GetTextLineHeight()), ImGuiChildFlags_None,
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if (visible && message && *message)
    {
        ImGui::TextUnformatted(message);
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::TextWrapped("%s", message);
            ImGui::EndTooltip();
        }
    }
    ImGui::EndChild();
}
