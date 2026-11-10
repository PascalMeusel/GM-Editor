#pragma once
#include "Views/Editor/Widgets/UiAdaptiveFields.hpp"
#include "BattleTrackerView.hpp"
#include "Models/Combat/EncounterMapRenderSource.hpp"
#include "Models/Combat/EncounterMapCanvas.hpp"
#include "Controllers/Editor/Widgets/BattleTracker/BattleTrackerController.hpp"
#include "Views/Editor/Widgets/Map/MapTileVisuals.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"
#include "misc/cpp/imgui_stdlib.h"
#include "imgui.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <string>
#include <string_view>
#include <vector>


namespace
{
std::string battleText(const char *english)
{
    return localizedUiLabel(english);
}

int battleActionColumns(int maximum, std::initializer_list<const char *> labels)
{
    float minimumWidth = 1.0f;
    for (const char *label : labels)
        minimumWidth = std::max(minimumWidth, ImGui::CalcTextSize(localizedUiText(label)).x +
            2.0f * (ImGui::GetStyle().FramePadding.x + ImGui::GetStyle().CellPadding.x));
    return std::clamp(static_cast<int>(ImGui::GetContentRegionAvail().x / minimumWidth), 1, maximum);
}

void battleSameLineIfFits(float nextWidth)
{
    const float rightEdge = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    if (ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + nextWidth <= rightEdge)
        ImGui::SameLine();
}

std::string battleFormat(const char *englishTemplate, std::initializer_list<std::string_view> arguments)
{
    std::string result = battleText(englishTemplate);
    std::size_t index = 0;
    for (const std::string_view argument : arguments)
    {
        const std::string placeholder = "{" + std::to_string(index++) + "}";
        std::size_t offset = 0;
        while ((offset = result.find(placeholder, offset)) != std::string::npos)
        {
            result.replace(offset, placeholder.size(), argument.data(), argument.size());
            offset += argument.size();
        }
    }
    return result;
}

std::string localizedNarrativeValue(const std::string &value)
{
    const auto withSuffix = [&](std::string_view prefix, const char *englishTemplate) -> std::string {
        if (value.rfind(prefix, 0) != 0 || value.size() <= prefix.size())
            return {};
        return battleFormat(englishTemplate, {std::string_view(value).substr(prefix.size())});
    };

    if (std::string localized = withSuffix("Engaged with ", "Engaged with {0}"); !localized.empty())
        return localized;
    if (std::string localized = withSuffix("Far from ", "Far from {0}"); !localized.empty())
        return localized;
    if (std::string localized = withSuffix("Distant from ", "Distant from {0}"); !localized.empty())
        return localized;
    if (std::string localized = withSuffix("Away from ", "Away from {0}"); !localized.empty())
        return localized;
    if (std::string localized = withSuffix("Near ", "Near {0}"); !localized.empty())
        return localized;
    return battleText(value.c_str());
}

std::string localizedConditionText(const std::string &condition)
{
    constexpr std::string_view spellEffectPrefix = "Spell Effect: ";
    constexpr std::string_view concentratingPrefix = "Concentrating: ";
    constexpr std::string_view reinforcementPrefix = "Reinforcement: Round ";
    if (condition.rfind(spellEffectPrefix, 0) == 0)
        return battleFormat("Spell Effect: {0}", {std::string_view(condition).substr(spellEffectPrefix.size())});
    if (condition.rfind(concentratingPrefix, 0) == 0)
        return battleFormat("Concentrating: {0}", {std::string_view(condition).substr(concentratingPrefix.size())});
    if (condition.rfind(reinforcementPrefix, 0) == 0)
        return battleFormat("Reinforcement: Round {0}", {std::string_view(condition).substr(reinforcementPrefix.size())});
    constexpr std::string_view systemConditions[] = {
        "Blinded", "Charmed", "Deafened", "Exhaustion", "Frightened", "Grappled", "Incapacitated", "Invisible",
        "Paralyzed", "Petrified", "Poisoned", "Prone", "Restrained", "Stunned", "Unconscious", "Hidden"};
    for (const std::string_view systemCondition : systemConditions)
    {
        if (condition == systemCondition)
            return battleText(condition.c_str());
    }
    return condition;
}

std::string tokenLabel(const std::string &name)
{
    if (name.empty())
        return "?";
    const unsigned char first = static_cast<unsigned char>(name.front());
    return std::string(1, static_cast<char>(std::toupper(first)));
}

std::string signedNumber(int value)
{
    return value >= 0 ? "+" + std::to_string(value) : std::to_string(value);
}

bool pointInsideCircle(const ImVec2 &point, const ImVec2 &center, float radius)
{
    const float dx = point.x - center.x;
    const float dy = point.y - center.y;
    return dx * dx + dy * dy <= radius * radius;
}

ImVec2 cellCenter(const ImVec2 &origin, float cellSize, int x, int y)
{
    return ImVec2(origin.x + (static_cast<float>(x) + 0.5f) * cellSize, origin.y + (static_cast<float>(y) + 0.5f) * cellSize);
}

void drawAreaTemplate(ImDrawList *drawList, const RuleAreaTemplate &area, const ImVec2 &origin, float cellSize, int cellSizeFeet, const EncounterParticipantModel &caster, const EncounterParticipantModel *target)
{
    if (area.shape.empty() || area.sizeFeet <= 0)
        return;

    const ImVec2 casterCenter = cellCenter(origin, cellSize, caster.x, caster.y);
    const ImVec2 targetCenter = target ? cellCenter(origin, cellSize, target->x, target->y) : casterCenter;
    const float feetToPixels = cellSize / static_cast<float>(std::max(1, cellSizeFeet));
    const float sizePixels = static_cast<float>(area.sizeFeet) * feetToPixels;
    const std::string shape = area.shape;
    const ImU32 fill = IM_COL32(255, 116, 96, 48);
    const ImU32 stroke = IM_COL32(255, 132, 96, 205);

    if (shape == "sphere" || shape == "cylinder" || shape == "emanation" || shape == "area")
    {
        const ImVec2 center = shape == "emanation" ? casterCenter : targetCenter;
        drawList->AddCircleFilled(center, sizePixels, fill, 96);
        drawList->AddCircle(center, sizePixels, stroke, 96, 2.5f);
    }
    else if (shape == "cube")
    {
        const float half = sizePixels * 0.5f;
        drawList->AddRectFilled(ImVec2(targetCenter.x - half, targetCenter.y - half), ImVec2(targetCenter.x + half, targetCenter.y + half), fill);
        drawList->AddRect(ImVec2(targetCenter.x - half, targetCenter.y - half), ImVec2(targetCenter.x + half, targetCenter.y + half), stroke, 0.0f, 0, 2.5f);
    }
    else if (shape == "cone")
    {
        float dx = targetCenter.x - casterCenter.x;
        float dy = targetCenter.y - casterCenter.y;
        const float length = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
        dx /= length;
        dy /= length;
        const ImVec2 tip(casterCenter.x + dx * sizePixels, casterCenter.y + dy * sizePixels);
        const float halfWidth = sizePixels * 0.5f;
        const ImVec2 left(tip.x + -dy * halfWidth, tip.y + dx * halfWidth);
        const ImVec2 right(tip.x - -dy * halfWidth, tip.y - dx * halfWidth);
        drawList->AddTriangleFilled(casterCenter, left, right, fill);
        drawList->AddTriangle(casterCenter, left, right, stroke, 2.5f);
    }
    else if (shape == "line" || shape == "wall")
    {
        float dx = targetCenter.x - casterCenter.x;
        float dy = targetCenter.y - casterCenter.y;
        const float length = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
        dx /= length;
        dy /= length;
        const ImVec2 end(casterCenter.x + dx * sizePixels, casterCenter.y + dy * sizePixels);
        const float halfWidth = std::max(cellSize * (shape == "wall" ? 0.35f : 0.18f), 4.0f);
        const ImVec2 leftA(casterCenter.x + -dy * halfWidth, casterCenter.y + dx * halfWidth);
        const ImVec2 rightA(casterCenter.x - -dy * halfWidth, casterCenter.y - dx * halfWidth);
        const ImVec2 leftB(end.x + -dy * halfWidth, end.y + dx * halfWidth);
        const ImVec2 rightB(end.x - -dy * halfWidth, end.y - dx * halfWidth);
        drawList->AddQuadFilled(leftA, leftB, rightB, rightA, fill);
        drawList->AddQuad(leftA, leftB, rightB, rightA, stroke, 2.5f);
    }
    else
    {
        drawList->AddCircleFilled(targetCenter, sizePixels, fill, 8);
        drawList->AddCircle(targetCenter, sizePixels, stroke, 8, 2.5f);
    }
}

const char *combatPhaseNextAction(const EncounterModel &encounter)
{
    switch (encounter.combatPhase)
    {
    case CombatPhase::Setup:
        return localizedUiText(encounter.participants.empty() ? "Load a battlefield, then add combatants." : "Roll or sort initiative, then begin combat.");
    case CombatPhase::Initiative:
        return localizedUiText("Confirm initiative order, then begin combat.");
    case CombatPhase::ActiveTurn:
        return localizedUiText("Move the active combatant, choose targets, then resolve an action.");
    case CombatPhase::Movement:
        return localizedUiText("Move or confirm the active combatant's position.");
    case CombatPhase::ActionResolution:
        return localizedUiText("Choose targets, adjudicate modifiers, then apply damage, healing, or effects.");
    case CombatPhase::ReactionsEffects:
        return localizedUiText("Resolve reactions, conditions, and expiring effects.");
    case CombatPhase::EndTurn:
        return localizedUiText("Review turn-end effects, then advance to the next combatant.");
    case CombatPhase::PausedBreak:
        return localizedUiText("Resume combat or resume the table break when ready.");
    case CombatPhase::WrapUp:
        return localizedUiText("Open Wrap-up / After Combat for summary, loot, and session notes.");
    }
    return localizedUiText("Choose the next combat step.");
}

std::string movementContextText(const EncounterModel &encounter, const EncounterParticipantModel &participant, int targetX, int targetY)
{
    int projected = encounterMovementCostFeet(encounter, participant.turnStartX, participant.turnStartY, targetX, targetY);
    if (projected >= std::numeric_limits<int>::max() / 8)
        return battleText("movement blocked");
    const int remaining = std::max(0, participant.speedFeet - projected);
    return battleFormat("movement {0}/{1} ft, {2} ft left",
                        {std::to_string(projected), std::to_string(participant.speedFeet), std::to_string(remaining)});
}
}
