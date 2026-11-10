#pragma once
#include "BattleTrackerViewGeometrySupport.hpp"

namespace
{
std::string viewLowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool textContainsNoCase(const std::string &text, const std::string &needle)
{
    return viewLowerCopy(text).find(viewLowerCopy(needle)) != std::string::npos;
}

bool participantHasConcentrationMarker(const EncounterParticipantModel &participant)
{
    for (const std::string &condition : participant.conditions)
    {
        if (textContainsNoCase(condition, "concentrat"))
            return true;
    }
    for (const ActiveEffectModel &effect : participant.activeEffects)
    {
        if (effect.concentration || textContainsNoCase(effect.name, "concentrat"))
            return true;
    }
    return false;
}

std::string summarizeConditions(const EncounterParticipantModel &participant)
{
    if (participant.conditions.empty())
        return "-";

    std::string summary;
    const std::size_t visibleCount = std::min<std::size_t>(participant.conditions.size(), 2);
    for (std::size_t index = 0; index < visibleCount; ++index)
    {
        if (!summary.empty())
            summary += ", ";
        summary += localizedConditionText(participant.conditions[index]);
    }
    if (participant.conditions.size() > visibleCount)
        summary += " +" + std::to_string(participant.conditions.size() - visibleCount);
    return summary;
}

std::string summarizeActiveEffects(const EncounterParticipantModel &participant)
{
    if (participant.activeEffects.empty())
        return "-";

    std::string summary;
    int visibleCount = 0;
    for (const ActiveEffectModel &effect : participant.activeEffects)
    {
        if (visibleCount >= 2)
            break;
        if (!summary.empty())
            summary += ", ";
        summary += effect.name.empty() ? battleText(activeEffectKindLabel(effect.kind))
                                       : (effect.kind == ActiveEffectKind::Condition ? localizedConditionText(effect.name) : effect.name);
        if (effect.remainingRounds >= 0)
            summary += battleFormat(" ({0}r)", {std::to_string(effect.remainingRounds)});
        ++visibleCount;
    }
    if (participant.activeEffects.size() > static_cast<std::size_t>(visibleCount))
        summary += " +" + std::to_string(participant.activeEffects.size() - static_cast<std::size_t>(visibleCount));
    return summary;
}

std::string effectReviewReason(const EncounterModel &encounter, const std::vector<int> &selectedTargets, int participantIndex)
{
    if (participantIndex < 0 || participantIndex >= static_cast<int>(encounter.participants.size()))
        return "-";

    const EncounterParticipantModel &participant = encounter.participants[static_cast<std::size_t>(participantIndex)];
    std::string reason;
    const auto appendReason = [&](const char *label) {
        if (!reason.empty())
            reason += ", ";
        reason += battleText(label);
    };

    if (participantIndex == encounter.activeParticipantIndex)
        appendReason("Active turn");
    if (std::find(selectedTargets.begin(), selectedTargets.end(), participantIndex) != selectedTargets.end())
        appendReason("Target");
    if (participantHasConcentrationMarker(participant))
        appendReason("Concentration");
    if (!participant.conditions.empty())
        appendReason("Conditions");
    if (!participant.activeEffects.empty())
        appendReason("Effects");

    return reason.empty() ? battleText("Review") : reason;
}

bool isReactionAction(const MonsterActionModel &action)
{
    return textContainsNoCase(action.actionKind, "reaction") || textContainsNoCase(action.name, "reaction");
}

std::string summarizeReactionActions(const EncounterParticipantModel &participant)
{
    std::string summary;
    int listed = 0;
    int total = 0;
    for (const MonsterActionModel &action : participant.actions)
    {
        if (!isReactionAction(action))
            continue;
        ++total;
        if (listed >= 2)
            continue;
        if (!summary.empty())
            summary += ", ";
        summary += action.name.empty() ? battleText("Reaction") : action.name;
        ++listed;
    }
    if (total > listed)
        summary += " +" + std::to_string(total - listed);
    return summary.empty() ? "-" : summary;
}

bool isEffectsReviewLogType(const std::string &eventType)
{
    return eventType == "Reaction" || eventType == "Effect" || eventType == "Concentration";
}

int optionIndexForValue(const std::string &value, const char *const *options, int optionCount, int fallbackIndex)
{
    for (int index = 0; index < optionCount; ++index)
    {
        if (value == options[index])
            return index;
    }
    return std::clamp(fallbackIndex, 0, std::max(0, optionCount - 1));
}
}
