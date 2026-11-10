#include "BattleTrackerControllerEffectSupport.hpp"

std::vector<int> BattleTrackerController::getSelectedTargetIndices() const
{
    std::vector<int> targets;
    for (int targetIndex : _selectedTargetIndices)
    {
        if (targetIndex < 0 || targetIndex >= static_cast<int>(_draftEncounter.participants.size()))
            continue;
        if (targetIndex == _selectedParticipantIndex)
            continue;
        if (std::find(targets.begin(), targets.end(), targetIndex) == targets.end())
            targets.push_back(targetIndex);
    }

    if (targets.empty() && _rangeTargetIndex >= 0 && _rangeTargetIndex < static_cast<int>(_draftEncounter.participants.size()) && _rangeTargetIndex != _selectedParticipantIndex)
        targets.push_back(_rangeTargetIndex);
    return targets;
}

void BattleTrackerController::addConditionToSelectedParticipant()
{
    if (_selectedParticipantIndex < 0 || _selectedParticipantIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return;

    if (_conditionDraft.empty())
        return;

    EncounterParticipantModel &participant = _draftEncounter.participants[_selectedParticipantIndex];
    if (std::find(participant.conditions.begin(), participant.conditions.end(), _conditionDraft) == participant.conditions.end())
        participant.conditions.push_back(_conditionDraft);
    const std::string addedCondition = _conditionDraft;
    _conditionDraft.clear();
    syncSelectedParticipantConditionsToSource();
    addCombatLogEntry("Condition",
                      combatFormat("{0} gains {1}.", {participant.displayName, localizedConditionLabel(addedCondition)}),
                      participant.displayName);
}

void BattleTrackerController::removeConditionFromSelectedParticipant(int conditionIndex)
{
    if (_selectedParticipantIndex < 0 || _selectedParticipantIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return;

    EncounterParticipantModel &participant = _draftEncounter.participants[_selectedParticipantIndex];
    if (conditionIndex < 0 || conditionIndex >= static_cast<int>(participant.conditions.size()))
        return;

    const std::string removedCondition = participant.conditions[static_cast<std::size_t>(conditionIndex)];
    participant.conditions.erase(participant.conditions.begin() + conditionIndex);
    syncSelectedParticipantConditionsToSource();
    addCombatLogEntry("Condition",
                      combatFormat("{0} loses {1}.", {participant.displayName, localizedConditionLabel(removedCondition)}),
                      participant.displayName);
}

void BattleTrackerController::removeActiveEffectFromSelectedParticipant(int effectIndex)
{
    if (_selectedParticipantIndex < 0 || _selectedParticipantIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return;

    EncounterParticipantModel &participant = _draftEncounter.participants[_selectedParticipantIndex];
    if (effectIndex < 0 || effectIndex >= static_cast<int>(participant.activeEffects.size()))
        return;

    const std::string removedEffect = participant.activeEffects[static_cast<std::size_t>(effectIndex)].name;
    participant.activeEffects.erase(participant.activeEffects.begin() + effectIndex);
    addCombatLogEntry("Effect", combatFormat("{0} loses {1}.", {participant.displayName, removedEffect}), participant.displayName);
}

void BattleTrackerController::syncSelectedParticipantConditionsToSource()
{
    if (_selectedParticipantIndex < 0 || _selectedParticipantIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return;

    EncounterParticipantModel &participant = _draftEncounter.participants[_selectedParticipantIndex];
    if (participant.kind == CombatantKind::Player)
    {
        auto characterIt = std::find_if(_context.campaign.characters.begin(), _context.campaign.characters.end(), [&](const CharacterModel &character) {
            return character.metadata.id == participant.sourceEntityId;
        });
        if (characterIt != _context.campaign.characters.end())
        {
            characterIt->conditions = participant.conditions;
            characterIt->metadata.touch();
        }
    }
    else if (participant.kind == CombatantKind::NPC)
    {
        auto npcIt = std::find_if(_context.campaign.npcs.begin(), _context.campaign.npcs.end(), [&](const NPCModel &npc) {
            return npc.metadata.id == participant.sourceEntityId;
        });
        if (npcIt != _context.campaign.npcs.end())
        {
            npcIt->conditions = participant.conditions;
            npcIt->metadata.touch();
        }
    }

    _draftEncounter.metadata.touch();
    saveEncounter();
}



void BattleTrackerController::addCombatLogNote()
{
    if (_combatLogNote.empty())
        return;

    addCombatLogEntry("Note", _combatLogNote);
    _combatLogNote.clear();
}

void BattleTrackerController::completeEffectsReview()
{
    if (_draftEncounter.participants.empty() || _combatStep != 1 || _draftEncounter.combatPaused)
        return;

    normalizeEncounter(_draftEncounter);
    const int activeIndex = std::clamp(_draftEncounter.activeParticipantIndex, 0, static_cast<int>(_draftEncounter.participants.size()) - 1);
    processTurnBoundaryEffects(activeIndex, "Review");
    addCombatLogEntry("Review", combatText("Reactions and active effects reviewed."), _draftEncounter.participants[static_cast<std::size_t>(activeIndex)].displayName);
    changeCombatPhase(CombatPhase::EndTurn);
}

void BattleTrackerController::setNarrativePosition(int participantIndex, const std::string &position)
{
    if (participantIndex < 0 || participantIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return;
    EncounterParticipantModel &participant = _draftEncounter.participants[static_cast<std::size_t>(participantIndex)];
    if (participant.narrativePosition == position)
        return;

    participant.narrativePosition = position.empty() ? "In the scene" : position;
    participant.metadata.touch();
    _combatStatusMessage = combatFormat("{0} narrative position set to {1}.",
                                        {participant.displayName, combatText(participant.narrativePosition.c_str())});
    _combatErrorMessage.clear();
    saveEncounter();
}

void BattleTrackerController::setNarrativeRangeBand(int participantIndex, const std::string &rangeBand)
{
    if (participantIndex < 0 || participantIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return;
    EncounterParticipantModel &participant = _draftEncounter.participants[static_cast<std::size_t>(participantIndex)];
    if (participant.narrativeRangeBand == rangeBand)
        return;

    participant.narrativeRangeBand = rangeBand.empty() ? "Near" : rangeBand;
    participant.metadata.touch();
    _combatStatusMessage = combatFormat("{0} range band set to {1}.",
                                        {participant.displayName, combatText(participant.narrativeRangeBand.c_str())});
    _combatErrorMessage.clear();
    saveEncounter();
}

void BattleTrackerController::setNarrativeSpotlight(int participantIndex)
{
    if (participantIndex < 0 || participantIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return;

    for (int index = 0; index < static_cast<int>(_draftEncounter.participants.size()); ++index)
    {
        EncounterParticipantModel &participant = _draftEncounter.participants[static_cast<std::size_t>(index)];
        const bool shouldSpotlight = index == participantIndex;
        if (participant.narrativeSpotlight != shouldSpotlight)
        {
            participant.narrativeSpotlight = shouldSpotlight;
            participant.metadata.touch();
        }
    }

    const std::string actorName = _draftEncounter.participants[static_cast<std::size_t>(participantIndex)].displayName;
    _combatStatusMessage = combatFormat("{0} has the narrative spotlight.", {actorName});
    _combatErrorMessage.clear();
    addCombatLogEntry("Spotlight", combatFormat("{0} takes the Theater of the Mind spotlight.", {actorName}), actorName);
}

void BattleTrackerController::setNarrativeRelativeTo(int participantIndex, int anchorIndex, const std::string &rangeBand, const std::string &relationship)
{
    if (participantIndex < 0 || participantIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return;
    if (anchorIndex < 0 || anchorIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return;

    EncounterParticipantModel &participant = _draftEncounter.participants[static_cast<std::size_t>(participantIndex)];
    const EncounterParticipantModel &anchor = _draftEncounter.participants[static_cast<std::size_t>(anchorIndex)];
    const std::string resolvedRange = rangeBand.empty() ? "Near" : rangeBand;
    std::string resolvedPosition;
    if (participantIndex == anchorIndex)
    {
        resolvedPosition = "Holding the spotlight";
    }
    else if (relationship.empty())
    {
        resolvedPosition = resolvedRange + " from " + anchor.displayName;
    }
    else
    {
        resolvedPosition = relationship + " " + anchor.displayName;
    }

    if (participant.narrativePosition == resolvedPosition && participant.narrativeRangeBand == resolvedRange)
        return;

    participant.narrativePosition = resolvedPosition;
    participant.narrativeRangeBand = resolvedRange;
    participant.metadata.touch();
    std::string localizedPosition;
    if (participantIndex == anchorIndex)
        localizedPosition = combatText("Holding the spotlight");
    else if (relationship.empty())
        localizedPosition = combatFormat("{0} from {1}", {combatText(resolvedRange.c_str()), anchor.displayName});
    else
        localizedPosition = combatFormat("{0} {1}", {combatText(relationship.c_str()), anchor.displayName});
    _combatStatusMessage = combatFormat("{0} is now {1} ({2}).",
                                        {participant.displayName,
                                         localizedPosition,
                                         combatText(participant.narrativeRangeBand.c_str())});
    _combatErrorMessage.clear();
    addCombatLogEntry("Position", _combatStatusMessage, participant.displayName);
}
