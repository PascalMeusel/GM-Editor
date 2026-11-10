#include "BattleTrackerController.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include <exception>

std::optional<CombatEditHistory::Snapshot> BattleTrackerController::captureCorrection(CombatEditHistory::Kind kind, const EntityId &resource)
{
    if (_encounterSaveFailed || _deferCombatSave || _pendingTableBreakResume || _selectedParticipantIndex < 0 ||
        _selectedParticipantIndex >= static_cast<int>(_draftEncounter.participants.size())) return {};
    try
    {
        return CombatEditHistory::capture(_currentCampaign, _draftEncounter,
            _draftEncounter.participants[static_cast<std::size_t>(_selectedParticipantIndex)].metadata.id, kind, resource);
    }
    catch (const std::exception &error) { _combatErrorMessage = error.what(); return {}; }
}

void BattleTrackerController::recordCorrection(const std::optional<CombatEditHistory::Snapshot> &before, const std::string &label)
{
    // Retain history for a failed original save; the command's context guard
    // blocks correction until retry, then scoped values reject later conflicts.
    if (!before || _deferCombatSave || _pendingTableBreakResume) return;
    try
    {
        const auto generation = _correctionGeneration;
        CombatEditHistory::recordApplied(_currentCampaign, _draftEncounter, *before,
            [](Campaign &staged) { return ProjectSaveService::getInstance().saveCampaign(staged, "combat correction"); },
            [this, generation] { return generation == _correctionGeneration && !_encounterSaveFailed &&
                !_deferCombatSave && !_pendingTableBreakResume && _dragParticipantIndex < 0; }, label);
    }
    catch (const std::exception &error) { _combatErrorMessage = error.what(); }
}
