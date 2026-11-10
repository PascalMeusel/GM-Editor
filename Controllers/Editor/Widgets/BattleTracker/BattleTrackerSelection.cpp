#include "BattleTrackerController.hpp"
#include "Models/Combat/EncounterDraftPersistence.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include <algorithm>
#include <iterator>

namespace { const char *combatText(const char *english) { return gm::localization::trUi(english); } }

bool BattleTrackerController::canLeaveForPreparation()
{
    if (!_encounterSaveFailed)
        return true;
    _combatErrorMessage = combatText("Combat changes could not be saved. Your edits are retained. Retry saving.");
    return false;
}

bool BattleTrackerController::refreshSavedPreparationById(const EntityId &encounterId)
{
    // Explicit handoff only: per-frame selection must never discard a draft.
    if (encounterId.empty() || !canLeaveForPreparation())
        return false;
    const auto found = std::find_if(_context.campaign.encounters.begin(), _context.campaign.encounters.end(),
        [&](const EncounterModel &encounter) { return encounter.metadata.id == encounterId; });
    if (found == _context.campaign.encounters.end())
        return false;
    EntityId selectedActor;
    if (_draftEncounter.metadata.id == encounterId && _selectedParticipantIndex >= 0 &&
        _selectedParticipantIndex < static_cast<int>(_draftEncounter.participants.size()))
        selectedActor = _draftEncounter.participants[_selectedParticipantIndex].metadata.id;
    _selectedEncounterIndex = static_cast<int>(std::distance(_context.campaign.encounters.begin(), found));
    loadSelectedEncounter();
    for (int index = 0; index < static_cast<int>(_draftEncounter.participants.size()); ++index)
        if (_draftEncounter.participants[index].metadata.id == selectedActor)
            _selectedParticipantIndex = index;
    _dragParticipantIndex = -1;
    _pendingHitPointDelta = 0;
    return true;
}

bool BattleTrackerController::selectEncounterById(const EntityId &encounterId)
{
    if (encounterId.empty())
        return false;

    const auto found = std::find_if(_context.campaign.encounters.begin(), _context.campaign.encounters.end(), [&](const EncounterModel &encounter) {
        return encounter.metadata.id == encounterId;
    });
    if (found == _context.campaign.encounters.end())
        return false;

    const int encounterIndex = static_cast<int>(std::distance(_context.campaign.encounters.begin(), found));
    if (_selectedEncounterIndex == encounterIndex && _draftEncounter.metadata.id == encounterId)
        return true;

    if (!canLeaveForPreparation())
        return false;
    _selectedEncounterIndex = encounterIndex;
    loadSelectedEncounter();
    return true;
}

void BattleTrackerController::loadSelectedEncounter()
{
    if (!canLeaveForPreparation())
    {
        // The legacy chooser changes its index before asking to load. Restore
        // the retained draft's identity instead of showing a mismatched row.
        const auto retained = std::find_if(_context.campaign.encounters.begin(), _context.campaign.encounters.end(),
            [&](const EncounterModel &entry) { return entry.metadata.id == _draftEncounter.metadata.id; });
        _selectedEncounterIndex = retained == _context.campaign.encounters.end() ? -1
            : static_cast<int>(std::distance(_context.campaign.encounters.begin(), retained));
        return;
    }
    if (_selectedEncounterIndex < 0 || _selectedEncounterIndex >= static_cast<int>(_context.campaign.encounters.size()))
        return;

    ++_correctionGeneration; _dragCorrection.reset();
    _draftEncounter = _context.campaign.encounters[_selectedEncounterIndex];
    normalizeEncounter(_draftEncounter);
    synchronizeParticipantsWithSources();
    _selectedParticipantIndex = _draftEncounter.participants.empty() ? -1 : std::min(_draftEncounter.activeParticipantIndex, static_cast<int>(_draftEncounter.participants.size()) - 1);
    _rangeTargetIndex = _draftEncounter.participants.size() > 1 ? 0 : -1;
    _selectedTargetIndices.clear();
    if (_rangeTargetIndex >= 0)
        _selectedTargetIndices.push_back(_rangeTargetIndex);
    _selectedActionIndex = 0;
    _selectedDiceRollPresetIndex = 0;
    _combatStep = isActiveCombatPhase() && !_draftEncounter.combatPaused ? 1 : 0;
    _freePlacementMode = false;
    _initiativeLocked = !_draftEncounter.participants.empty();
    _lastAttackResolved = false;
    _actionAdjudicationReviewed = false;
    if (_draftEncounter.diceRollPresets.empty())
    {
        _diceRollPresetName = combatText("Quick Roll");
        _diceRollExpression = "1d20";
        _diceRollPresetNotes.clear();
    }
    else
    {
        loadSelectedDiceRollPreset();
    }

    for (int i = 0; i < static_cast<int>(_context.campaign.maps.size()); ++i)
    {
        if (_context.campaign.maps[i].metadata.id == _draftEncounter.mapId)
        {
            _selectedMapIndex = i;
            break;
        }
    }
}

void BattleTrackerController::persistEncounterDraft()
{
    if (_deferCombatSave) return;
    if (_pendingTableBreakResume.has_value()) { persistPendingTableBreak(); return; }
    const bool retryingSave = _encounterSaveFailed;
    _encounterSaveFailed = !persistEncounterDraftSnapshot(
        _context.campaign, _draftEncounter, _selectedEncounterIndex,
        [](Campaign &campaign) { return ProjectSaveService::getInstance().saveCampaign(campaign, "combat edit"); });
    if (_encounterSaveFailed)
        _combatErrorMessage = combatText("Combat changes could not be saved. Your edits are retained. Retry saving.");
    else if (retryingSave)
        _combatErrorMessage.clear();
}

bool BattleTrackerController::isActiveCombatPhase() const
{
    return combatPhaseRunsActiveTurn(_draftEncounter.combatPhase);
}

void BattleTrackerController::loadSelectedDiceRollPreset()
{
    if (_draftEncounter.diceRollPresets.empty())
        return;
    _selectedDiceRollPresetIndex = std::clamp(_selectedDiceRollPresetIndex, 0, static_cast<int>(_draftEncounter.diceRollPresets.size()) - 1);
    const DiceRollPresetModel &preset = _draftEncounter.diceRollPresets[_selectedDiceRollPresetIndex];
    _diceRollPresetName = preset.name;
    _diceRollExpression = preset.expression;
    _diceRollPresetNotes = preset.notes;
}

bool BattleTrackerController::combatPhaseRunsActiveTurn(CombatPhase phase)
{
    return phase == CombatPhase::ActiveTurn ||
           phase == CombatPhase::Movement ||
           phase == CombatPhase::ActionResolution ||
           phase == CombatPhase::ReactionsEffects ||
           phase == CombatPhase::EndTurn;
}

void BattleTrackerController::onPreparationSaved(const EntityId &encounterId)
{
    // Saving another encounter must not change the active combat selection.
    // The same retained encounter is refreshed only after durable prep save.
    if (_draftEncounter.metadata.id == encounterId)
        refreshSavedPreparationById(encounterId);
}

void BattleTrackerController::saveEncounter()
{
    if (_deferCombatSave) return;
    if (_draftEncounter.name.empty())
        _draftEncounter.name = combatText("Untitled Encounter");

    synchronizeParticipantsWithSources();
    normalizeEncounter(_draftEncounter);
    _draftEncounter.metadata.touch();

    persistEncounterDraft();
}

void BattleTrackerController::setCombatPhase(CombatPhase phase)
{
    _draftEncounter.combatPhase = phase;
    _draftEncounter.combatPaused = combatPhaseIsPaused(phase);
    _combatStep = combatPhaseRunsActiveTurn(phase) ? 1 : 0;
}
