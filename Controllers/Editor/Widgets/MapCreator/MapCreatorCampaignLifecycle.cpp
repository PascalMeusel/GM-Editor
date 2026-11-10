#include "MapCreatorController.hpp"

#include <utility>

void MapCreatorController::prepareForCampaignReplacement()
{
    ++_campaignEpoch;
    invalidateGenerationRequest();
    // Keep the async future alive: destroying a std::async future can join its
    // worker. It owns immutable inputs and will be drained without publication.
    auto pendingGeneration = std::move(_generation.future);
    const auto requestSerial = _generation.requestSerial;
    _generation = map_creator_controller::GenerationState{};
    _generation.future = std::move(pendingGeneration);
    _generation.retired = _generation.future.valid();
    _generation.requestSerial = requestSerial;

    clearUndoSnapshot(); // Revokes the journal epoch without destroying its worker.
    _history.journalRetryAt = {};
    _history.hasRecoveryPreview = false;
    _history.recoveryPreview = {};
    _history.pendingNamedCheckpointLabel.clear();
    _history.namedCheckpoints.clear();
    _history.newCheckpointName.clear();
    _history.selectedNamedCheckpointIndex = -1;
    _history.namedCheckpointRestorePreview = {};
    _history.hasNamedCheckpointRestorePreview = false;
    _history.draftHistory = {};

    auto pendingExport = std::move(_transfer.pngExportTask);
    _transfer = map_creator_controller::TransferState{};
    _transfer.pngExportTask = std::move(pendingExport);
    // Pending import recovery belongs to the old campaign's on-disk library.
    // Forget its UI binding, but never acknowledge/delete it through a new root.
    const auto authoringSerial = _editing.authoringSerial;
    _editing = map_creator_controller::EditingState{};
    _editing.authoringSerial = authoringSerial + 1;
    _persistence = map_creator_controller::PersistenceState{};
    _draftMap = {};
    _integration.statusMessage.clear();
    _integration.attachSavedMapToEncounterHandler = {};
    if (_view) _view->resetForCampaign();
    hide();
}

void MapCreatorController::onCampaignReplaced()
{
    // startNewMap is safe only after the old import transaction binding has
    // been revoked. No recovery is silently reapplied at this boundary.
    startNewMap();
}

void MapCreatorController::pollRetiredCampaignWork()
{
    if (_generation.retired && _generation.future.valid() &&
        _generation.future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
        try { (void)_generation.future.get(); } catch (...) {}
        _generation.retired = false;
    }
    if (_view) _view->pollRetiredCampaignWork();
    if (_pngExportEpoch != _campaignEpoch) updatePngExport();
}
