#include "EncounterBuilderController.hpp"
#include "Models/Localization/LocalizationService.hpp"

bool EncounterBuilderController::beginNewEncounterDraft()
{
    if (_saveFailed) return false;
    startNewEncounter();
    return true;
}

void EncounterBuilderController::cancelTacticalGeneration()
{
    if (_tacticalCancellation)
        _tacticalCancellation->store(true);
    _tacticalGenerationEncounterId.clear();
}

void EncounterBuilderController::resetTacticalDraftContext()
{
    cancelTacticalGeneration();
    _tacticalCandidates.clear();
    _selectedTacticalCandidate = -1;
    _tacticalStatus.clear();
    _saveFailed = false;
    _saveSucceeded = false;
    _saveBlockedByCombat = false;
}

void EncounterBuilderController::startNewEncounter()
{
    if (_saveFailed) return;
    retainCurrentEncounterDraft();
    _explicitNewEncounterDraft = true;
    resetTacticalDraftContext();
    _draftEncounter = EncounterModel{};
    _persistedDraftId.clear();
    _draftEdited = false;
    _draftEncounter.name = gm::localization::trUi("New Encounter");
    _draftEncounter.cellSizeFeet = 5;
    _draftEncounter.battlefieldWidth = 16;
    _draftEncounter.battlefieldHeight = 12;
    _draftEncounter.round = 1;
    _draftEncounter.activeParticipantIndex = 0;
    _selectedEncounterIndex = -1;
    _selectedParticipantIndex = -1;
    normalizeEncounter(_draftEncounter);
}

void EncounterBuilderController::onShow()
{
    if (!_saveFailed && !_explicitNewEncounterDraft && _selectedEncounterIndex < 0 && !_currentCampaign.encounters.empty())
    {
        _selectedEncounterIndex = 0;
        loadSelectedEncounter();
    }
    if (_view)
    {
        _view->show();
        _view->requestDockOnNextUpdate();
    }
}


void EncounterBuilderController::retainCurrentEncounterDraft()
{
    if (!_draftEncounter.metadata.id.empty())
        _retainedEncounterDrafts[_draftEncounter.metadata.id] = {_draftEncounter, _persistedDraftId, _draftEdited};
}

void EncounterBuilderController::resetDrafts()
{
    _saveFailed = false;
    startNewEncounter();
    _retainedEncounterDrafts.clear();
    _explicitNewEncounterDraft = false;
}

bool EncounterBuilderController::restoreRetainedEncounterDraft(const EntityId &id)
{
    if (_saveFailed) return false;
    const auto found = _retainedEncounterDrafts.find(id);
    if (found == _retainedEncounterDrafts.end()) return false;
    const auto retained = found->second;
    retainCurrentEncounterDraft();
    resetTacticalDraftContext();
    _draftEncounter = retained.encounter;
    _persistedDraftId = retained.persistedId;
    _draftEdited = retained.dirty;
    _explicitNewEncounterDraft = _persistedDraftId.empty();
    _selectedEncounterIndex = -1;
    for (int index = 0; index < static_cast<int>(_currentCampaign.encounters.size()); ++index)
        if (_currentCampaign.encounters[index].metadata.id == id) _selectedEncounterIndex = index;
    return true;
}

bool EncounterBuilderController::hasUnsavedDrafts() const
{
    if (_draftEdited || _saveFailed) return true;
    for (const auto &entry : _retainedEncounterDrafts) if (entry.second.dirty) return true;
    return false;
}

bool EncounterBuilderController::flushDrafts()
{
    if (!hasUnsavedDrafts()) return true;
    const EntityId original = _draftEncounter.metadata.id;
    if ((_draftEdited || _saveFailed) && !saveEncounter()) return false;
    retainCurrentEncounterDraft();
    std::vector<EntityId> pending;
    for (const auto &entry : _retainedEncounterDrafts) if (entry.second.dirty) pending.push_back(entry.first);
    for (const auto &id : pending)
        if (!restoreRetainedEncounterDraft(id) || !saveEncounter()) return false;
    restoreRetainedEncounterDraft(original);
    _retainedEncounterDrafts.erase(original);
    return true;
}
