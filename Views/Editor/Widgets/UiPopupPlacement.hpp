#pragma once

#include "imgui_internal.h"
#include <algorithm>

namespace PopupUi
{
// Match BeginPopup's current stack level and ID. A deeper combo, or a newly
// opened popup reusing an old window, must keep its own native opening anchor.
inline ImGuiWindow *establishedPopup(const char *label)
{
    auto &context = *ImGui::GetCurrentContext();
    const int level = context.BeginPopupStack.Size;
    if (level >= context.OpenPopupStack.Size) return nullptr;
    const auto &popup = context.OpenPopupStack[level];
    auto *window = popup.Window;
    if (popup.PopupId != ImGui::GetID(label) || !window || !window->WasActive ||
        window->Appearing || popup.OpenFrameCount == context.FrameCount) return nullptr;
    return window;
}

// Apply before Begin computes the popup's decorations and clipping. The caller
// supplies either its constrained next auto-fit size or its fixed form size.
inline void contain(ImGuiWindow *window, const ImGuiViewport *owner, ImVec2 size)
{
    if (!window) return;
    const ImVec2 minimum = owner->WorkPos;
    const ImVec2 maximum(std::max(minimum.x, minimum.x + owner->WorkSize.x - size.x),
                         std::max(minimum.y, minimum.y + owner->WorkSize.y - size.y));
    const ImVec2 position(std::clamp(window->Pos.x, minimum.x, maximum.x),
                          std::clamp(window->Pos.y, minimum.y, maximum.y));
    if (position.x != window->Pos.x || position.y != window->Pos.y)
        ImGui::SetNextWindowPos(position, ImGuiCond_Always);
}

inline void prepareFixed(const char *label, ImVec2 preferred)
{
    const auto *owner = ImGui::GetWindowViewport();
    const ImVec2 size(std::max(1.0f, std::min(preferred.x, owner->WorkSize.x * .9f)),
                      std::max(1.0f, std::min(preferred.y, owner->WorkSize.y * .9f)));
    ImGui::SetNextWindowViewport(owner->ID);
    // BeginPopup adds AlwaysAutoResize; reserve the body/footer on every frame,
    // including short history-only frames and later validation-error growth.
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(size, size);
    contain(establishedPopup(label), owner, size);
}
}
