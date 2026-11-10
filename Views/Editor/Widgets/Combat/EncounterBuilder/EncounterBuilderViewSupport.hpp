#pragma once
#include "Views/Editor/Widgets/UiAdaptiveFields.hpp"
#include "EncounterBuilderView.hpp"
#include "Controllers/Editor/Widgets/EncounterBuilder/EncounterBuilderController.hpp"
#include "Views/Editor/Widgets/Map/MapTileVisuals.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"
#include "misc/cpp/imgui_stdlib.h"
#include "imgui.h"
#include "Views/Editor/Widgets/UiTaskLayout.hpp"
#include "Models/Combat/EncounterMapEligibility.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <string_view>
#include <string>
#include <vector>

namespace
{
template <typename... Args>
std::string formatLocalizedUi(const char *format, Args... args)
{
    const std::string localizedFormat = localizedUiLabel(format);
    const int required = std::snprintf(nullptr, 0, localizedFormat.c_str(), args...);
    if (required <= 0)
        return localizedFormat;

    std::vector<char> buffer(static_cast<std::size_t>(required) + 1);
    std::snprintf(buffer.data(), buffer.size(), localizedFormat.c_str(), args...);
    return std::string(buffer.data(), static_cast<std::size_t>(required));
}

void drawFieldLabel(const char *label)
{
    ImGui::TextDisabled("%s", localizedUiLabel(label).c_str());
}

bool needsWrappedPreview(const std::string &value, float availableWidth)
{
    if (value.empty())
        return false;

    const float previewWidth = std::max(80.0f, availableWidth - ImGui::GetStyle().FramePadding.x * 2.0f);
    std::string_view remaining(value);
    while (!remaining.empty())
    {
        const std::size_t lineEnd = remaining.find('\n');
        const std::string_view line = remaining.substr(0, lineEnd);
        if (ImGui::CalcTextSize(line.data(), line.data() + line.size()).x > previewWidth)
            return true;
        if (lineEnd == std::string_view::npos)
            break;
        remaining.remove_prefix(lineEnd + 1);
    }
    return false;
}

void drawWrappedPreview(const std::string &value)
{
    if (!needsWrappedPreview(value, ImGui::GetContentRegionAvail().x))
        return;

    ImGui::TextDisabled("%s", localizedUiLabel("Preview").c_str());
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
    ImGui::TextUnformatted(value.c_str());
    ImGui::PopTextWrapPos();
}

bool drawTacticalCandidatePreview(const MapModel &map, MapRenderPlanCache &cache, const char *id, bool selected)
{
    const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float height = std::clamp(ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 3.0f, 220.0f, 620.0f);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const bool activated = ImGui::InvisibleButton(id, ImVec2(width, height));
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height), IM_COL32(29, 33, 31, 255), 5.0f);
    const float cell = std::min((width - 6.0f) / std::max(1, map.width), (height - 6.0f) / std::max(1, map.height));
    const ImVec2 mapOrigin(origin.x + (width - map.width * cell) * 0.5f, origin.y + (height - map.height * cell) * 0.5f);
    MapRenderPlanRequest request;
    request.audience = MapRenderAudience::GameMaster;
    request.applyFog = false;
    request.showGrid = false;
    request.locale = std::string(gm::localization::localeCode(gm::localization::LocalizationService::instance().locale()));
    request.layers = MapRenderPlanAll & ~static_cast<std::uint32_t>(MapRenderPlanGrid | MapRenderPlanLabels);
    const auto plan = cache.getOrCompile(map, request, map.generation.recipe.seed, cell);
    drawList->PushClipRect(origin, ImVec2(origin.x + width, origin.y + height), true);
    renderMap(drawList, mapOrigin, map, *plan, cell, false);
    drawList->PopClipRect();

    int startZoneIndex = 0;
    int objectiveIndex = 0;
    for (const MapRegionModel &region : map.regions)
    {
        const bool startZone = region.description == "Encounter starting zone";
        const bool objective = region.description == "Visible encounter objective";
        if (!startZone && !objective)
            continue;

        const ImVec2 minimum(mapOrigin.x + region.x * cell, mapOrigin.y + region.y * cell);
        const ImVec2 maximum(mapOrigin.x + (region.x + region.width) * cell,
                             mapOrigin.y + (region.y + region.height) * cell);
        if (startZone)
        {
            const ImU32 color = startZoneIndex == 0 ? IM_COL32(57, 192, 175, 235)
                                                    : IM_COL32(98, 153, 224, 235);
            const ImU32 fillColor = startZoneIndex == 0 ? IM_COL32(57, 192, 175, 52)
                                                        : IM_COL32(98, 153, 224, 52);
            drawList->AddRectFilled(minimum, maximum, fillColor, 2.0f);
            drawList->AddRect(minimum, maximum, color, 2.0f, 0, 2.0f);
            const char label[] = {static_cast<char>('A' + std::min(startZoneIndex, 25)), '\0'};
            drawList->AddText(ImVec2(minimum.x + 3.0f, minimum.y + 1.0f), color, label);
            ++startZoneIndex;
        }
        else
        {
            const ImVec2 center((minimum.x + maximum.x) * 0.5f, (minimum.y + maximum.y) * 0.5f);
            const float radius = std::max(4.0f, std::min(maximum.x - minimum.x, maximum.y - minimum.y) * 0.42f);
            const ImU32 color = IM_COL32(232, 184, 70, 245);
            drawList->AddQuadFilled(ImVec2(center.x, center.y - radius),
                                    ImVec2(center.x + radius, center.y),
                                    ImVec2(center.x, center.y + radius),
                                    ImVec2(center.x - radius, center.y),
                                    IM_COL32(232, 184, 70, 70));
            drawList->AddQuad(ImVec2(center.x, center.y - radius),
                              ImVec2(center.x + radius, center.y),
                              ImVec2(center.x, center.y + radius),
                              ImVec2(center.x - radius, center.y), color, 2.0f);
            const std::string label = std::to_string(++objectiveIndex);
            drawList->AddText(ImVec2(center.x - ImGui::CalcTextSize(label.c_str()).x * 0.5f,
                                     center.y - ImGui::GetFontSize() * 0.5f), color, label.c_str());
        }
    }

    const ImU32 border = (selected || activated) ? IM_COL32(224, 169, 64, 255)
                         : hovered                ? IM_COL32(89, 188, 177, 245)
                                                  : IM_COL32(108, 116, 109, 210);
    drawList->AddRect(origin, ImVec2(origin.x + width, origin.y + height), border, 5.0f, 0,
                      (selected || activated) ? 3.0f : 1.0f);
    if (hovered)
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    return activated;
}
}
