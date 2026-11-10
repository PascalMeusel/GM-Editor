#pragma once

#include "Models/Map/MapRenderPlan.hpp"
#include "imgui.h"
#include <algorithm>
#include <cstddef>
#include <vector>

namespace gm::mapui
{
inline std::vector<const MapTokenModel *> orderedLegendTokens(const MapModel &filtered)
{
    std::vector<const MapTokenModel *> result;
    for (const auto &token : filtered.tokens) result.push_back(&token);
    // Persistence may return the collection alphabetically. A rename or
    // restart must not change the numbers used to discuss this scene.
    std::stable_sort(result.begin(), result.end(), [](const auto *left, const auto *right) {
        if (left->metadata.createdAtUtc != right->metadata.createdAtUtc)
            return left->metadata.createdAtUtc < right->metadata.createdAtUtc;
        return left->metadata.id < right->metadata.id;
    });
    return result;
}
// Only accept the already-filtered projection. Names follow the same token
// and label permissions as the map, including in the private review window.
inline void drawMapTokenLegend(const MapModel &filtered, const MapRenderPlan &plan)
{
    if ((plan.layers & MapRenderPlanTokens) == 0 || (plan.layers & MapRenderPlanLabels) == 0 ||
        filtered.tokens.empty()) return;
    const int columns = std::clamp(static_cast<int>(ImGui::GetContentRegionAvail().x /
        (ImGui::GetFontSize() * 18.0f)), 1, 3);
    if (!ImGui::BeginTable("ReviewedTokenLegend", columns,
        ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings)) return;
    const auto tokens = orderedLegendTokens(filtered);
    for (std::size_t index = 0; index < tokens.size(); ++index)
    {
        ImGui::TableNextColumn();
        ImGui::TextWrapped("%zu  %s", index + 1, tokens[index]->name.c_str());
    }
    ImGui::EndTable();
}
}
