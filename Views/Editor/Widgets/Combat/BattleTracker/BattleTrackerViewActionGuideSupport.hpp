#pragma once
#include "BattleTrackerViewStateSupport.hpp"

namespace
{
const char *spellRollStepLabel(SpellResolutionType resolution)
{
    switch (resolution)
    {
    case SpellResolutionType::SpellAttack:
        return localizedUiText("Spell attack");
    case SpellResolutionType::SavingThrow:
        return localizedUiText("Target saves");
    case SpellResolutionType::Healing:
        return localizedUiText("Healing dice");
    case SpellResolutionType::SummonOrZone:
        return localizedUiText("Zone/effect");
    case SpellResolutionType::Utility:
    default:
        return localizedUiText("Effect");
    }
}

void drawActionGuideStep(const char *label, const std::string &detail, bool complete, bool current)
{
    ImGui::TableNextColumn();
    const ImVec4 color = current ? ImVec4(0.95f, 0.78f, 0.42f, 1.0f)
                                 : (complete ? ImVec4(0.55f, 0.82f, 0.68f, 1.0f)
                                             : ImVec4(0.62f, 0.66f, 0.68f, 1.0f));
    const std::string localizedLabel = battleText(label);
    ImGui::TextColored(color, localizedUiLabel("%s").c_str(), localizedLabel.c_str());
    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), detail.empty() ? "-" : detail.c_str());
}

int actionResolutionCurrentStep(const MonsterActionModel &selectedAction,
                                const std::vector<int> &selectedTargets,
                                bool allTargetsInRange,
                                bool adjudicationReviewed,
                                bool lastAttackResolved)
{
    if (selectedAction.name.empty())
        return 0;
    if (selectedTargets.empty() || !allTargetsInRange)
        return 1;
    if (!adjudicationReviewed)
        return 2;
    if (!lastAttackResolved)
        return 3;
    return 4;
}

bool beginActionResolutionSection(const char *label, int stepIndex, int currentStep)
{
    if (stepIndex == currentStep)
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    const std::string localizedLabel = localizedUiLabel(label);
    return ImGui::CollapsingHeader(localizedLabel.c_str());
}

void drawActionResolutionGuide(
    const MonsterActionModel &selectedAction,
    const std::vector<int> &selectedTargets,
    bool selectedActionIsSpell,
    bool hasSpellSummary,
    const RuleSpellResolutionSummary &spellSummary,
    bool allTargetsInRange,
    int adjudicationCount,
    bool adjudicationReviewed,
    bool lastAttackResolved,
    bool lastAttackHit)
{
    const int currentStep = actionResolutionCurrentStep(selectedAction, selectedTargets, allTargetsInRange, adjudicationReviewed, lastAttackResolved);

    const std::string actionDetail = selectedAction.name.empty()
        ? battleText("Choose action")
        : selectedAction.name + (selectedAction.actionKind.empty() ? "" : " | " + battleText(selectedAction.actionKind.c_str()));
    const std::string targetDetail = selectedTargets.empty()
        ? battleText("Select targets")
        : battleFormat(allTargetsInRange ? "{0} ready" : "{0} blocked", {std::to_string(static_cast<int>(selectedTargets.size()))});
    const std::string adjudicationDetail = adjudicationReviewed
        ? (adjudicationCount == 0 ? battleText("Reviewed: optional") : battleFormat("Reviewed: {0} modifiers", {std::to_string(adjudicationCount)}))
        : (adjudicationCount == 0 ? battleText("Review optional") : battleFormat("Review {0} modifiers", {std::to_string(adjudicationCount)}));
    const std::string rollDetail = selectedActionIsSpell
        ? (hasSpellSummary ? spellRollStepLabel(spellSummary.resolution) : battleText("Missing spell details"))
        : (lastAttackResolved ? (lastAttackHit ? battleText("Hit checked") : battleText("Miss logged")) : battleText("D20 vs AC"));
    const std::string applyDetail = selectedActionIsSpell
        ? (hasSpellSummary && !selectedTargets.empty() && allTargetsInRange ? battleText("Resolve spell") : battleText("Blocked"))
        : (!selectedTargets.empty() && allTargetsInRange ? (lastAttackResolved ? (lastAttackHit ? battleText("Apply damage") : battleText("Miss logged")) : battleText("Check hit")) : battleText("Blocked"));

    if (ImGui::BeginTable("ActionResolutionGuide", battleActionColumns(5, {"1 Action", "2 Targets", "3 Adjudicate", "4 Roll/Save", "5 Apply/Log"}), ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
    {
        drawActionGuideStep("1 Action", actionDetail, !selectedAction.name.empty(), currentStep == 0);
        drawActionGuideStep("2 Targets", targetDetail, !selectedTargets.empty() && allTargetsInRange, currentStep == 1);
        drawActionGuideStep("3 Adjudicate", adjudicationDetail, adjudicationReviewed, currentStep == 2);
        drawActionGuideStep("4 Roll/Save", rollDetail, lastAttackResolved, currentStep == 3);
        drawActionGuideStep("5 Apply/Log", applyDetail, lastAttackResolved && (!lastAttackHit || !selectedActionIsSpell), currentStep == 4);
        ImGui::EndTable();
    }
}
}
