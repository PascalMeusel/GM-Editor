#pragma once
#include "Models/Character/PortraitArtwork.hpp"
#include "Views/Editor/Widgets/ImageTextureCache.hpp"
#include "Views/Editor/Widgets/UiAdaptiveFields.hpp"
#include <array>
#include <cstdio>

namespace PortraitUi
{
// The picker consumes the same atlas crop as the character sheet. Selecting a
// thumbnail changes only the existing identity index, never a saved species ID.
inline bool identityPicker(int &identity, const CachedImageTexture &art, bool german)
{
    const auto label = [german](int index) {
        std::array<char, 32> value{};
        std::snprintf(value.data(), value.size(), german ? "Porträt %d" : "Portrait %d", index + 1);
        return value;
    };
    const float width = std::max(1.f, ImGui::GetContentRegionAvail().x);
    const float unit = ImGui::GetFontSize() / 18.f;
    const float maximumHeight = ImGui::GetMainViewport()->WorkSize.y * .78f;
    ImGui::SetNextItemWidth(width);
    ImGui::SetNextWindowSizeConstraints({width, 0.f},
        {width, maximumHeight});
    const auto current = label(identity);
    if (!FormUi::BeginCombo(german ? "Person###PortraitIdentity" : "Character###PortraitIdentity", current.data())) return false;
    bool changed = false;
    if (!art.loaded)
    {
        // A missing texture must not make existing portrait choices unusable.
        for (int i = 0; i < 8; ++i)
        {
            const auto name = label(i);
            if (ImGui::Selectable(name.data(), identity == i)) { identity = i; changed = true; }
            if (identity == i) ImGui::SetItemDefaultFocus();
        }
    }
    else
    {
        const float available = std::max(1.f, ImGui::GetContentRegionAvail().x);
        const float gap = ImGui::GetStyle().ItemSpacing.x;
        const float padding = 3.f * unit;
        const int columns = std::clamp(int((available + gap) / (54.f * unit + gap)), 1, 4);
        const int rows = (8 + columns - 1) / columns;
        const float cellWidth = std::max(1.f, (available - gap * (columns - 1)) / columns);
        const float rowChrome = padding * 2 + ImGui::GetTextLineHeight() + ImGui::GetStyle().ItemSpacing.y * 2;
        const float heightBudget = (maximumHeight - ImGui::GetStyle().WindowPadding.y * 2) / rows;
        const float pictureWidth = std::max(1.f, std::min({88.f * unit, cellWidth - padding * 2,
            std::max(32.f, (heightBudget - rowChrome) / 1.5f)}));
        const float firstColumn = ImGui::GetCursorPosX();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {padding, padding});
        for (int i = 0; i < 8; ++i)
        {
            if (i % columns) ImGui::SameLine(firstColumn + (i % columns) * (cellWidth + gap));
            ImGui::PushID(i);
            ImGui::BeginGroup();
            const auto name = label(i);
            const auto crop = gm::portraits::artworkRegion(i, art.width, art.height);
            const bool selected = identity == i;
            if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
            const bool clicked = ImGui::ImageButton(name.data(), art.textureId, {pictureWidth, pictureWidth * 1.5f},
                                                   {crop.u0, crop.v0}, {crop.u1, crop.v1});
#ifdef IMGUI_ENABLE_TEST_ENGINE
            // ImGui's image button has no built-in label hook. Preserve the
            // ordinary widget identity for the application's input observer.
            auto &g = *ImGui::GetCurrentContext();
            IMGUI_TEST_ENGINE_ITEM_INFO(g.LastItemData.ID, name.data(), g.LastItemData.StatusFlags);
#endif
            if (clicked)
            {
                identity = i;
                changed = true;
                ImGui::CloseCurrentPopup();
            }
            if (selected) { ImGui::SetItemDefaultFocus(); ImGui::PopStyleColor(); }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", name.data());
            // The number stays readable even in a narrow inspector at 150%.
            ImGui::Text("%s%d", selected ? "• " : "", i + 1);
            ImGui::EndGroup();
            ImGui::PopID();
        }
        ImGui::PopStyleVar();
    }
    ImGui::EndCombo();
    return changed;
}
}
