#include "BattleTrackerController.hpp"
#include "Models/Audio/AudioTriggerService.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <string_view>
#include <utility>
namespace {
struct DeferredCombatSave {
    bool &deferred;
    explicit DeferredCombatSave(bool &value) : deferred(value) { deferred = true; }
    ~DeferredCombatSave() { deferred = false; }
};
std::string combatText(const char *english)
{
    return gm::localization::trUi(english);
}

std::string combatFormat(const char *englishTemplate, std::initializer_list<std::string_view> arguments)
{
    std::string result = combatText(englishTemplate);
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

std::string localizedConditionLabel(const std::string &condition)
{
    constexpr std::string_view spellEffectPrefix = "Spell Effect: ";
    constexpr std::string_view concentratingPrefix = "Concentrating: ";
    constexpr std::string_view reinforcementPrefix = "Reinforcement: Round ";
    if (condition.rfind(spellEffectPrefix, 0) == 0)
        return combatFormat("Spell Effect: {0}", {std::string_view(condition).substr(spellEffectPrefix.size())});
    if (condition.rfind(concentratingPrefix, 0) == 0)
        return combatFormat("Concentrating: {0}", {std::string_view(condition).substr(concentratingPrefix.size())});
    if (condition.rfind(reinforcementPrefix, 0) == 0)
        return combatFormat("Reinforcement: Round {0}", {std::string_view(condition).substr(reinforcementPrefix.size())});
    constexpr std::string_view systemConditions[] = {
        "Blinded", "Charmed", "Deafened", "Exhaustion", "Frightened", "Grappled", "Incapacitated", "Invisible",
        "Paralyzed", "Petrified", "Poisoned", "Prone", "Restrained", "Stunned", "Unconscious", "Hidden"};
    for (const std::string_view systemCondition : systemConditions)
    {
        if (condition == systemCondition)
            return combatText(condition.c_str());
    }
    return condition;
}

std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool addUniqueCondition(std::vector<std::string> &conditions, const std::string &condition)
{
    if (condition.empty() || std::find(conditions.begin(), conditions.end(), condition) != conditions.end())
        return false;
    conditions.push_back(condition);
    return true;
}

bool rechargeRuleMatchesTurn(const std::string &rule)
{
    const std::string lowerRule = lowerCopy(rule);
    return lowerRule.find("turn") != std::string::npos || lowerRule.find("round") != std::string::npos;
}

void syncParticipantConditionsToSource(Campaign &campaign, const EncounterParticipantModel &participant)
{
    if (participant.kind == CombatantKind::Player)
    {
        auto characterIt = std::find_if(campaign.characters.begin(), campaign.characters.end(), [&](const CharacterModel &character) {
            return character.metadata.id == participant.sourceEntityId;
        });
        if (characterIt != campaign.characters.end())
        {
            characterIt->conditions = participant.conditions;
            characterIt->metadata.touch();
            campaign.metadata.touch();
        }
    }
    else if (participant.kind == CombatantKind::NPC)
    {
        auto npcIt = std::find_if(campaign.npcs.begin(), campaign.npcs.end(), [&](const NPCModel &npc) {
            return npc.metadata.id == participant.sourceEntityId;
        });
        if (npcIt != campaign.npcs.end())
        {
            npcIt->conditions = participant.conditions;
            npcIt->metadata.touch();
            campaign.metadata.touch();
        }
    }
}

void touchParticipantSpellcaster(Campaign &campaign, const EncounterParticipantModel &participant)
{
    if (participant.kind == CombatantKind::Player)
    {
        auto characterIt = std::find_if(campaign.characters.begin(), campaign.characters.end(), [&](const CharacterModel &character) {
            return character.metadata.id == participant.sourceEntityId;
        });
        if (characterIt != campaign.characters.end())
            characterIt->metadata.touch();
    }
    else if (participant.kind == CombatantKind::NPC)
    {
        auto npcIt = std::find_if(campaign.npcs.begin(), campaign.npcs.end(), [&](const NPCModel &npc) {
            return npc.metadata.id == participant.sourceEntityId;
        });
        if (npcIt != campaign.npcs.end())
            npcIt->metadata.touch();
    }
}
}

void BattleTrackerController::sortInitiative()
{
    if (!canLeaveForPreparation()) return;
    std::stable_sort(_draftEncounter.participants.begin(), _draftEncounter.participants.end(), [](const EncounterParticipantModel &left, const EncounterParticipantModel &right) {
        return left.initiative > right.initiative;
    });
    _draftEncounter.activeParticipantIndex = _draftEncounter.participants.empty() ? 0 : 0;
    _selectedParticipantIndex = _draftEncounter.participants.empty() ? -1 : 0;
    _initiativeLocked = !_draftEncounter.participants.empty();
    setCombatPhase(_draftEncounter.participants.empty() ? CombatPhase::Setup : CombatPhase::Initiative);
    _selectedActionIndex = 0;
    _lastAttackResolved = false;
    addCombatLogEntry("Initiative", combatText("Sorted initiative order."));
}

void BattleTrackerController::beginCombat()
{
    if (!canLeaveForPreparation() || _draftEncounter.participants.empty() ||
        (_draftEncounter.combatPhase != CombatPhase::Setup && _draftEncounter.combatPhase != CombatPhase::Initiative))
        return;
    {
    DeferredCombatSave transition(_deferCombatSave);
    for (EncounterParticipantModel &participant : _draftEncounter.participants)
    {
        participant.x = participant.startingX;
        participant.y = participant.startingY;
        participant.turnStartX = participant.startingX;
        participant.turnStartY = participant.startingY;
        if (participant.startsHidden)
            addUniqueCondition(participant.conditions, "Hidden");
        if (participant.reinforcementRound > 1)
            addUniqueCondition(participant.conditions, "Reinforcement: Round " + std::to_string(participant.reinforcementRound));
    }
    if (!_initiativeLocked)
        sortInitiative();
    setCombatPhase(CombatPhase::ActiveTurn);
    _freePlacementMode = false;
    startActiveTurn();
    addCombatLogEntry("Combat", combatText("Combat began."), _draftEncounter.participants[_draftEncounter.activeParticipantIndex].displayName);
    }
    saveEncounter();
    if (_encounterSaveFailed) return;
    std::string triggerError;
    AudioTriggers::playMatchingTriggers(_context.campaign, _context.playbackService, "CombatEvent", _draftEncounter.metadata.id, "Combat Started", triggerError);
    if (!triggerError.empty()) _combatErrorMessage = triggerError;
}

void BattleTrackerController::startActiveTurn()
{
    ++_correctionGeneration;
    if (_draftEncounter.participants.empty())
        return;

    normalizeEncounter(_draftEncounter);
    setCombatPhase(CombatPhase::ActiveTurn);
    EncounterParticipantModel &participant = _draftEncounter.participants[_draftEncounter.activeParticipantIndex];
    _selectedParticipantIndex = _draftEncounter.activeParticipantIndex;
    participant.turnStartX = participant.x;
    participant.turnStartY = participant.y;
    participant.movementUsedFeet = 0;
    if (std::vector<ResourceTrackerModel> *resources = getSelectedSourceResources())
    {
        bool changed = false;
        for (ResourceTrackerModel &tracker : *resources)
        {
            normalizeResourceTracker(tracker);
            if (tracker.used > 0 && rechargeRuleMatchesTurn(tracker.rechargeRule))
            {
                tracker.used = 0;
                tracker.metadata.touch();
                changed = true;
            }
        }
        if (changed)
        {
            touchParticipantSpellcaster(_context.campaign, participant);
            saveCampaign();
        }
    }
    selectActiveParticipant();
    processTurnBoundaryEffects(_draftEncounter.activeParticipantIndex, "Start");
}

void BattleTrackerController::selectActiveParticipant()
{
    if (_draftEncounter.participants.empty())
    {
        _selectedParticipantIndex = -1;
        return;
    }
    normalizeEncounter(_draftEncounter);
    _selectedParticipantIndex = _draftEncounter.activeParticipantIndex;
    _selectedActionIndex = 0;
    _lastAttackResolved = false;
    _actionAdjudicationReviewed = false;
    _selectedTargetIndices.clear();
    _rangeTargetIndex = _draftEncounter.participants.size() > 1 ? (_selectedParticipantIndex == 0 ? 1 : 0) : -1;
    if (_rangeTargetIndex >= 0)
        _selectedTargetIndices.push_back(_rangeTargetIndex);
}

void BattleTrackerController::advanceCombatTurn(bool skipped)
{
    if (!canLeaveForPreparation() || _draftEncounter.participants.empty() ||
        !isActiveCombatPhase() || _draftEncounter.combatPaused)
        return;

    normalizeEncounter(_draftEncounter);
    const std::string skippedName = _draftEncounter.participants[_draftEncounter.activeParticipantIndex].displayName;
    bool newRound = false;
    {
    DeferredCombatSave transition(_deferCombatSave);
    processTurnBoundaryEffects(_draftEncounter.activeParticipantIndex, "End");
    setCombatPhase(CombatPhase::EndTurn);
    _draftEncounter.activeParticipantIndex++;
    if (_draftEncounter.activeParticipantIndex >= static_cast<int>(_draftEncounter.participants.size()))
    {
        _draftEncounter.activeParticipantIndex = 0;
        _draftEncounter.round++;
        advanceTimedEffectsForNewRound();
        newRound = true;
    }

    startActiveTurn();
    addCombatLogEntry("Turn", combatText("Started turn."), _draftEncounter.participants[_draftEncounter.activeParticipantIndex].displayName);
    if (skipped)
    {
        _lastAttackMessage = combatFormat("{0} skipped their turn.", {skippedName});
        _lastAttackResolved = true;
        _lastAttackHit = false;
        addCombatLogEntry("Turn", _lastAttackMessage, skippedName);
    }
    }
    saveEncounter();
    if (_encounterSaveFailed || !newRound) return;
    std::string triggerError;
    AudioTriggers::playMatchingTriggers(_context.campaign, _context.playbackService, "CombatEvent", _draftEncounter.metadata.id, "Round Started", triggerError);
    if (!triggerError.empty()) _combatErrorMessage = triggerError;
}

void BattleTrackerController::nextTurn() { advanceCombatTurn(false); }

void BattleTrackerController::skipTurn() { advanceCombatTurn(true); }

void BattleTrackerController::advanceTimedEffectsForNewRound()
{
    std::vector<std::pair<std::string, std::string>> expiredEffects;
    for (EncounterParticipantModel &participant : _draftEncounter.participants)
    {
        for (ActiveEffectModel &effect : participant.activeEffects)
        {
            if (effect.remainingRounds > 0)
                --effect.remainingRounds;
        }

        auto expiredBegin = std::remove_if(participant.activeEffects.begin(), participant.activeEffects.end(), [&](const ActiveEffectModel &effect) {
            if (effect.remainingRounds != 0)
                return false;
            expiredEffects.emplace_back(participant.displayName, effect.name);
            const std::string conditionName = effect.concentration ? effect.name : "Spell Effect: " + effect.name;
            participant.conditions.erase(std::remove(participant.conditions.begin(), participant.conditions.end(), conditionName), participant.conditions.end());
            return true;
        });
        if (expiredBegin != participant.activeEffects.end())
        {
            participant.activeEffects.erase(expiredBegin, participant.activeEffects.end());
            syncParticipantConditionsToSource(_context.campaign, participant);
        }
    }

    if (!expiredEffects.empty())
        saveCampaign();
    for (const auto &expired : expiredEffects)
        addCombatLogEntry("Effect",
                          combatFormat("{0}'s {1} expires.", {expired.first, localizedConditionLabel(expired.second)}),
                          expired.first);
}

void BattleTrackerController::processTurnBoundaryEffects(int participantIndex, const std::string &boundary)
{
    if (participantIndex < 0 || participantIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return;

    EncounterParticipantModel &participant = _draftEncounter.participants[participantIndex];
    std::vector<std::string> effectNames;
    for (const ActiveEffectModel &effect : participant.activeEffects)
    {
        if (!effect.active)
            continue;
        std::string label = localizedConditionLabel(effect.name);
        if (effect.remainingRounds >= 0)
            label += combatFormat(" ({0} rounds)", {std::to_string(effect.remainingRounds)});
        if (!effect.notes.empty())
            label += ": " + combatText(effect.notes.c_str());
        effectNames.push_back(label);
    }

    if (effectNames.empty())
        return;

    const char *summaryTemplate = boundary == "Start"
                                      ? "Start of turn effects for {0}: "
                                      : (boundary == "End" ? "End of turn effects for {0}: "
                                                           : "Review of turn effects for {0}: ");
    std::string summary = combatFormat(summaryTemplate, {participant.displayName});
    for (std::size_t index = 0; index < effectNames.size(); ++index)
    {
        if (index > 0)
            summary += "; ";
        summary += effectNames[index];
    }
    addCombatLogEntry("Effect", summary, participant.displayName);
}

void BattleTrackerController::addCombatLogEntry(const std::string &eventType, const std::string &summary, const std::string &actorName)
{
    if (summary.empty())
        return;

    CombatLogEntryModel entry;
    entry.round = std::max(1, _draftEncounter.round);
    entry.actorName = actorName;
    entry.eventType = eventType.empty() ? "Event" : eventType;
    entry.summary = summary;
    entry.metadata.touch();
    _draftEncounter.combatLog.push_back(entry);
    _draftEncounter.metadata.touch();
    saveEncounter();
}

void BattleTrackerController::saveCampaign()
{
    // Resource/effect helpers may save between combat phases. Include the
    // matching encounter snapshot without refreshing action lists mid-action.
    persistEncounterDraft();
}

std::vector<ResourceTrackerModel> *BattleTrackerController::getSelectedSourceResources()
{
    if (_selectedParticipantIndex < 0 || _selectedParticipantIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return nullptr;
    const EncounterParticipantModel &participant = _draftEncounter.participants[_selectedParticipantIndex];
    if (participant.kind == CombatantKind::Player)
    {
        auto characterIt = std::find_if(_context.campaign.characters.begin(), _context.campaign.characters.end(), [&](const CharacterModel &character) {
            return character.metadata.id == participant.sourceEntityId;
        });
        return characterIt == _context.campaign.characters.end() ? nullptr : &characterIt->resourceTrackers;
    }
    if (participant.kind == CombatantKind::NPC)
    {
        auto npcIt = std::find_if(_context.campaign.npcs.begin(), _context.campaign.npcs.end(), [&](const NPCModel &npc) {
            return npc.metadata.id == participant.sourceEntityId;
        });
        return npcIt == _context.campaign.npcs.end() ? nullptr : &npcIt->resourceTrackers;
    }
    return nullptr;
}

bool BattleTrackerController::advanceTurnFromShortcut()
{
    if (!canLeaveForPreparation() || _draftEncounter.participants.empty() || _combatStep != 1 || _draftEncounter.combatPaused)
        return false;

    nextTurn();
    return !_encounterSaveFailed;
}
