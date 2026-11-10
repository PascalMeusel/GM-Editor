#include "Models/Project/ContextLinkMetadata.hpp"
#include "Models/Map/MapTravelCreation.hpp"
#include "MapCreatorController.hpp"
#include <random>
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorArtStylePolicy.hpp"
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorFilePathPolicy.hpp"
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorPersistencePolicy.hpp"
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorPlayerOutputPolicy.hpp"
#include "Models/Map/MapAtlasService.hpp"
#include "Models/Map/MapAuthoring.hpp"
#include "Models/Map/MapStrokeAuthoring.hpp"
#include "Models/Map/MapCheckpointService.hpp"
#include "Models/Map/MapGeneration.hpp"
#include "Models/Map/MapImageImportService.hpp"
#include "Models/Map/MapPackageDraftInstallService.hpp"
#include "Models/Map/MapPackageService.hpp"
#include "Models/Map/MapPngExport.hpp"
#include "Models/Map/MapRenderPlan.hpp"
#include "Models/Map/TacticalMapGenerator.hpp"
#include "Models/Map/MapWorldBrief.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Persistence/PersistenceSerialization.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Project/WorldInspectorService.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <Windows.h>
#include <commdlg.h>
#endif

using namespace PersistenceSerialization;

#include "MapCreatorInternals.hpp"
using namespace gm::map_creator_detail;

void MapCreatorController::captureUndoSnapshot(const std::string &label)
{
    _history.undo.push_back(UndoSnapshot{_draftMap, label, _persistence.playerSettingsBaseline, {}, _history.draftHistory.current()});
    markDraftEdited();
    while (_history.undo.size() > map_creator_controller::HistoryState::maximumUndoEntries)
        _history.undo.erase(_history.undo.begin());
    // Any new edit creates a new history branch. A redo would otherwise
    // resurrect state that no longer follows from the current map.
    _history.redo.clear();
    _history.crashJournalPending = true;
    _history.crashJournalLabel = label;
    refreshUndoState();
}

void MapCreatorController::markDraftEdited()
{
    ++_editing.authoringSerial;
    if (_editing.authoringSerial == 0)
        ++_editing.authoringSerial;
    _history.draftHistory.edited();
}

void MapCreatorController::beginPropertyEditUndo(const std::string &label)
{
    if (_generation.inProgress || _history.propertyEditUndoActive)
        return;

    captureUndoSnapshot(label);
    _history.propertyEditUndoActive = true;
}

void MapCreatorController::finishPropertyEditUndo()
{
    if (_history.propertyEditUndoActive)
    {
        markDraftEdited();
        // The journal may have flushed while the field was still active.
        // Queue the final draft as well, including the last typed characters.
        _history.crashJournalPending = true;
    }
    _history.propertyEditUndoActive = false;
}

void MapCreatorController::refreshUndoState()
{
    _history.hasUndo = !_history.undo.empty();
    _history.undoLabel = _history.hasUndo ? _history.undo.back().label : std::string{};
    _history.hasRedo = !_history.redo.empty();
    _history.redoLabel = _history.hasRedo ? _history.redo.back().label : std::string{};
}

void MapCreatorController::clearUndoSnapshot()
{
    _history.crashJournalWorker.cancel();
    _history.journalSubmittedSerial.reset();
    _editing.brushRenderSnapshot.clear();
    _history.undo.clear();
    _history.redo.clear();
    _history.hasUndo = false;
    _history.hasRedo = false;
    _editing.strokeInProgress = false;
    _history.propertyEditUndoActive = false;
    _editing.lastBrushX = -1;
    _editing.lastBrushY = -1;
    _history.undoLabel.clear();
    _history.redoLabel.clear();
    _history.crashJournalPending = false;
    _history.crashJournalWriteFailed = false;
    _history.crashJournalLabel.clear();
}

void MapCreatorController::undoLastAction()
{
    if (!_history.hasUndo)
        return;

    const UndoSnapshot &pendingSnapshot = _history.undo.back();
    if (pendingSnapshot.worldPlaceTransition)
    {
        if (!applyWorldPlaceHistoryTransition(
                *pendingSnapshot.worldPlaceTransition,
                "undo generated place transfer",
                "Could not undo the campaign-world transfer:"))
            return;

        const UndoSnapshot snapshot = pendingSnapshot;
        _history.undo.pop_back();
        UndoSnapshot redoSnapshot{_draftMap, snapshot.label, _persistence.playerSettingsBaseline, {}, _history.draftHistory.current()};
        redoSnapshot.worldPlaceTransition = UndoSnapshot::WorldPlaceTransition{
            snapshot.worldPlaceTransition->place,
            !snapshot.worldPlaceTransition->shouldExist};
        _history.redo.push_back(std::move(redoSnapshot));
        while (_history.redo.size() > map_creator_controller::HistoryState::maximumUndoEntries)
            _history.redo.erase(_history.redo.begin());
        _history.propertyEditUndoActive = false;
        _integration.statusMessage = std::string(gm::localization::trUi("Undid:")) + " " +
                         gm::localization::trUi(snapshot.label.c_str());
        markDraftEdited();
        refreshUndoState();
        return;
    }

    if (_history.undo.back().label == "Import Map Package" &&
        !finishPendingMapPackageImportRecovery(false))
        return;

    const UndoSnapshot snapshot = _history.undo.back();
    _history.undo.pop_back();
    _history.redo.push_back(UndoSnapshot{_draftMap, snapshot.label, _persistence.playerSettingsBaseline, {}, _history.draftHistory.current()});
    while (_history.redo.size() > map_creator_controller::HistoryState::maximumUndoEntries)
        _history.redo.erase(_history.redo.begin());
    restoreMapCreatorDraftHistory(snapshot.map, snapshot.playerSettingsBaseline, _currentCampaign.maps,
                                  _draftMap, _persistence.playerSettingsBaseline, _persistence.editingMapIndex, _persistence.selectedMapIndex,
                                  MapAtlasService::effectiveParentId(snapshot.map), _editing.selectedParentIndex);
    if (snapshot.label == "Edit world notes")
    {
        _generation.worldBrief = _draftMap.description;
        _generation.worldBriefParsed = false;
    }
    _history.crashJournalPending = true;
    _history.crashJournalLabel = "Undo " + snapshot.label;
    _history.propertyEditUndoActive = false;
    _persistence.selectedScopeIndex = std::clamp(static_cast<int>(_draftMap.scope), 0, 2);
    _generation.selectedCreationIntentIndex = static_cast<int>(_draftMap.creationIntent);
    restoreTravelSetupFromRecipe();
    _generation.selectedSizePresetIndex = static_cast<int>(_draftMap.sizePreset);
    resizeMapCells(_draftMap);
    ensureDefaultMapLayer(_draftMap);
    normalizeObjectSelection();
    syncReferenceCalibrationDraft();
    _integration.statusMessage = snapshot.label.empty()
        ? gm::localization::trUi("Undid last map edit.")
        : std::string(gm::localization::trUi("Undid:")) + " " + gm::localization::trUi(snapshot.label.c_str());
    markDraftEdited();
    _history.draftHistory.restore(snapshot.historyState);
    if (!_history.draftHistory.dirty())
    {
        _persistence.cleanAuthoringSerial = _editing.authoringSerial;
        // Saving increments metadata/revision independently from authoring.
        // Restore their committed values when returning to the saved content.
        if (_persistence.editingMapIndex >= 0)
        {
            _draftMap.metadata = _currentCampaign.maps[_persistence.editingMapIndex].metadata;
            _draftMap.revision = _currentCampaign.maps[_persistence.editingMapIndex].revision;
        }
    }
    refreshUndoState();
}

void MapCreatorController::redoLastAction()
{
    if (!_history.hasRedo)
        return;

    const UndoSnapshot &pendingSnapshot = _history.redo.back();
    if (pendingSnapshot.worldPlaceTransition)
    {
        if (!applyWorldPlaceHistoryTransition(
                *pendingSnapshot.worldPlaceTransition,
                "redo generated place transfer",
                "Could not redo the campaign-world transfer:"))
            return;

        const UndoSnapshot snapshot = pendingSnapshot;
        _history.redo.pop_back();
        UndoSnapshot undoSnapshot{_draftMap, snapshot.label, _persistence.playerSettingsBaseline, {}, _history.draftHistory.current()};
        undoSnapshot.worldPlaceTransition = UndoSnapshot::WorldPlaceTransition{
            snapshot.worldPlaceTransition->place,
            !snapshot.worldPlaceTransition->shouldExist};
        _history.undo.push_back(std::move(undoSnapshot));
        while (_history.undo.size() > map_creator_controller::HistoryState::maximumUndoEntries)
            _history.undo.erase(_history.undo.begin());
        _history.propertyEditUndoActive = false;
        _integration.statusMessage = std::string(gm::localization::trUi("Redid:")) + " " +
                         gm::localization::trUi(snapshot.label.c_str());
        markDraftEdited();
        refreshUndoState();
        return;
    }

    const UndoSnapshot snapshot = _history.redo.back();
    _history.redo.pop_back();
    _history.undo.push_back(UndoSnapshot{_draftMap, snapshot.label, _persistence.playerSettingsBaseline, {}, _history.draftHistory.current()});
    while (_history.undo.size() > map_creator_controller::HistoryState::maximumUndoEntries)
        _history.undo.erase(_history.undo.begin());
    restoreMapCreatorDraftHistory(snapshot.map, snapshot.playerSettingsBaseline, _currentCampaign.maps,
                                  _draftMap, _persistence.playerSettingsBaseline, _persistence.editingMapIndex, _persistence.selectedMapIndex,
                                  MapAtlasService::effectiveParentId(snapshot.map), _editing.selectedParentIndex);
    if (snapshot.label == "Edit world notes")
    {
        _generation.worldBrief = _draftMap.description;
        _generation.worldBriefParsed = false;
    }
    _history.crashJournalPending = true;
    _history.crashJournalLabel = "Redo " + snapshot.label;
    _history.propertyEditUndoActive = false;
    _persistence.selectedScopeIndex = std::clamp(static_cast<int>(_draftMap.scope), 0, 2);
    _generation.selectedCreationIntentIndex = static_cast<int>(_draftMap.creationIntent);
    restoreTravelSetupFromRecipe();
    _generation.selectedSizePresetIndex = static_cast<int>(_draftMap.sizePreset);
    resizeMapCells(_draftMap);
    ensureDefaultMapLayer(_draftMap);
    normalizeObjectSelection();
    syncReferenceCalibrationDraft();
    _integration.statusMessage = snapshot.label.empty()
        ? gm::localization::trUi("Redid last map edit.")
        : std::string(gm::localization::trUi("Redid:")) + " " + gm::localization::trUi(snapshot.label.c_str());
    markDraftEdited();
    _history.draftHistory.restore(snapshot.historyState);
    if (!_history.draftHistory.dirty())
    {
        _persistence.cleanAuthoringSerial = _editing.authoringSerial;
        // Saving increments metadata/revision independently from authoring.
        // Restore their committed values when returning to the saved content.
        if (_persistence.editingMapIndex >= 0)
        {
            _draftMap.metadata = _currentCampaign.maps[_persistence.editingMapIndex].metadata;
            _draftMap.revision = _currentCampaign.maps[_persistence.editingMapIndex].revision;
        }
    }
    refreshUndoState();
}

void MapCreatorController::updateCrashJournal()
{
    if (auto result = _history.crashJournalWorker.takeCompletion())
    {
        if (result->mapId == _draftMap.metadata.id && result->authoringSerial == _editing.authoringSerial &&
            result->projectDirectory == ProjectPackageLayout::layoutForCampaign(_currentCampaign).rootDirectory)
        {
            _history.journalSubmittedSerial.reset();
            if (result->success)
            {
                _history.crashJournalPending = false;
                if (_history.crashJournalWriteFailed)
                    _integration.statusMessage = gm::localization::trUi("Map recovery snapshot saved after retry.");
                _history.crashJournalWriteFailed = false;
            }
            else
            {
                _history.crashJournalWriteFailed = true;
                _history.journalRetryAt = std::chrono::steady_clock::now() + std::chrono::seconds(2);
                _integration.statusMessage = std::string(gm::localization::trUi("Map recovery snapshot failed:")) + " " +
                    gm::localization::trSystemMessage(result->error) + " " +
                    gm::localization::trUi("The latest recovery snapshot will be retried automatically.");
            }
        }
    }
    if (!_history.crashJournalPending || _draftMap.metadata.id.empty() ||
        (_history.journalSubmittedSerial && *_history.journalSubmittedSerial == _editing.authoringSerial) ||
        std::chrono::steady_clock::now() < _history.journalRetryAt)
        return;
    try
    {
        _history.crashJournalWorker.submit(ProjectPackageLayout::layoutForCampaign(_currentCampaign).rootDirectory,
            _draftMap, _history.crashJournalLabel, _editing.authoringSerial, !_history.hasRecoveryPreview && !hasUnsavedDraft());
        _history.journalSubmittedSerial = _editing.authoringSerial;
    }
    catch (const std::exception &error)
    {
        _history.crashJournalWriteFailed = true;
        _history.journalRetryAt = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        _integration.statusMessage = std::string(gm::localization::trUi("Map recovery snapshot failed:")) + " " + error.what();
    }
}

void MapCreatorController::flushCrashJournal()
{
    _history.crashJournalWorker.cancel();
    _history.journalSubmittedSerial.reset();
    if (!_history.crashJournalPending || _draftMap.metadata.id.empty())
        return;
    try
    {
        ProjectPackageLayout::ensureCampaignPackageLayout(_currentCampaign);
        MapCheckpointService service(ProjectPackageLayout::layoutForCampaign(_currentCampaign).rootDirectory);
        std::string journalError;
        // Returning to the committed state must not advertise an unsaved recovery
        // draft on next launch. Keep untouched recovery previews until reviewed.
        if (!_history.hasRecoveryPreview && !_editing.strokeInProgress && !_history.propertyEditUndoActive && !hasUnsavedDraft())
        {
            if (service.discardRecovery(_draftMap.metadata.id, &journalError))
            {
                _history.crashJournalPending = false;
                _history.crashJournalWriteFailed = false;
                return;
            }
            _history.crashJournalWriteFailed = true;
            _integration.statusMessage = std::string(gm::localization::trUi("Could not discard map recovery:")) + " " + journalError;
            return;
        }
        const MapCreatorCrashJournalFlushOutcome outcome =
            flushMapCreatorCrashJournalRequest(
                _history.crashJournalPending, _history.crashJournalLabel,
                [&](const std::string &label) {
                    const MapCheckpointWriteResult result =
                        service.writeCrashJournal(_draftMap, label);
                    journalError = result.error;
                    return result.success;
                });
        if (outcome == MapCreatorCrashJournalFlushOutcome::RetryRequired)
        {
            _history.crashJournalWriteFailed = true;
            _integration.statusMessage = std::string(gm::localization::trUi("Map recovery snapshot failed:")) + " " +
                             gm::localization::trSystemMessage(journalError) + " " +
                             gm::localization::trUi("The latest recovery snapshot will be retried automatically.");
        }
        else if (outcome == MapCreatorCrashJournalFlushOutcome::Written &&
                 _history.crashJournalWriteFailed)
        {
            _history.crashJournalWriteFailed = false;
            _integration.statusMessage = gm::localization::trUi(
                "Map recovery snapshot saved after retry.");
        }
    }
    catch (const std::exception &error)
    {
        _history.crashJournalWriteFailed = true;
        _integration.statusMessage = std::string(gm::localization::trUi("Map recovery snapshot failed:")) + " " +
                         gm::localization::trSystemMessage(error.what()) + " " +
                         gm::localization::trUi("The latest recovery snapshot will be retried automatically.");
    }
}
