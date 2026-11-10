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

bool MapCreatorController::saveDraftMap()
{
    // Tool selection may change the ordinary status; retain the last failed
    // save explanation until another save attempt resolves it.
    struct SaveFeedbackScope
    {
        bool &failed;
        const std::string &status;
        std::string &error;
        ~SaveFeedbackScope() { error = failed ? status : std::string{}; }
    } saveFeedback{_persistence.saveFailed, _integration.statusMessage, _persistence.saveError};
    // Older recovery copies may predate positional reference preservation.
    repairCollapsedMapSetupFeatureReferences(_currentCampaign, _draftMap);
    // A new receipt is issued only after the database commit below succeeds.
    // The campaign receives the in-memory draft before that commit, so keeping
    // an older receipt here could otherwise make a failed re-save reviewable.
    if (!mapSetupReferencesValid(_currentCampaign, _draftMap.generation.recipe.facts))
    {
        _persistence.saveFailed = true;
        _integration.statusMessage = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
            ? "Ein ausgewählter Ort oder eine Nation wurde entfernt. Prüfe die Auswahl. Dein Entwurf bleibt erhalten."
            : "A selected place or nation was removed. Check the selection. Your draft has been kept.";
        return false;
    }
    _persistence.saveReceipt = {};
    _persistence.loadedBaseline = {};
    _persistence.saveFailed = true;
    if (!_draftMap.isMainMap &&
        !mapCreatorParentSelectionIsResolved(_editing.selectedParentIndex, _currentCampaign.maps.size()))
    {
        _integration.statusMessage = gm::localization::trUi(
            "This map's parent is no longer available. Choose another parent or explicitly select No parent map before saving. Your draft has been kept.");
        return false;
    }
    const auto currentMap = std::find_if(
        _currentCampaign.maps.begin(), _currentCampaign.maps.end(),
        [&](const MapModel &map) { return map.metadata.id == _draftMap.metadata.id; });
    const bool mapAlreadyExists = currentMap != _currentCampaign.maps.end();
    const auto latestRevision = mapAlreadyExists ? currentMap->revision : _draftMap.revision;
    const auto mergedPlayerSettings = mapAlreadyExists
                                          ? mergeMapCreatorPlayerSettings(_persistence.playerSettingsBaseline, _draftMap, *currentMap)
                                          : captureMapCreatorPlayerSettings(_draftMap);
    const bool draftWasPreviouslySaved = _persistence.playerSettingsBaseline.valid &&
                                         _persistence.playerSettingsBaseline.mapId == _draftMap.metadata.id;
    if (!mergedPlayerSettings.valid ||
        (!mapAlreadyExists && (_persistence.editingMapIndex >= 0 || draftWasPreviouslySaved)))
    {
        _integration.statusMessage = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
                             ? "Die Karte oder ihre Spielerfreigaben wurden außerhalb dieses Entwurfs geändert. Lade die Karte neu, bevor du speicherst. Dein Entwurf bleibt erhalten."
                             : "The map or its player sharing settings changed outside this draft. Reload the map before saving. Your draft has been kept.";
        return false;
    }
    MapCreatorCampaignSaveTransaction<Campaign> saveTransaction(
        _currentCampaign, _draftMap, _persistence.editingMapIndex, _persistence.selectedMapIndex);
    if (mapAlreadyExists)
        _persistence.editingMapIndex = static_cast<int>(std::distance(_currentCampaign.maps.begin(), currentMap));
    if (_draftMap.name.empty())
        _draftMap.name = gm::localization::trUi("Untitled Map");

    syncScopeFromIndex();
    const EntityId previousParentId = MapAtlasService::effectiveParentId(_draftMap);
    if (_editing.selectedParentIndex > 0 && _editing.selectedParentIndex - 1 < static_cast<int>(_currentCampaign.maps.size()))
    {
        const MapModel &parent = _currentCampaign.maps[_editing.selectedParentIndex - 1];
        _draftMap.parentMapId = parent.metadata.id;
        if (previousParentId != parent.metadata.id || _draftMap.atlasPlacement.parentId.empty())
        {
            _draftMap.atlasPlacement = {};
            _draftMap.atlasStatus = MapAtlasStatus::Unpositioned;
            const int desiredDepth = std::min(MapAtlasService::levelDepth(parent.atlasLevel) + 1, MapAtlasService::levelDepth(MapLevel::Tactical));
            _draftMap.atlasLevel = static_cast<MapLevel>(desiredDepth);
        }
    }
    else
    {
        _draftMap.parentMapId.clear();
        _draftMap.atlasPlacement = {};
        _draftMap.atlasStatus = MapAtlasStatus::Standalone;
    }
    if (_draftMap.parentMapId == _draftMap.metadata.id)
    {
        _draftMap.parentMapId.clear();
        _draftMap.atlasPlacement = {};
        _draftMap.atlasStatus = MapAtlasStatus::Standalone;
        _draftMap.atlasLevel = MapLevel::World;
    }

    if (_draftMap.isMainMap)
    {
        for (auto &map : _currentCampaign.maps)
            map.isMainMap = false;
        _draftMap.parentMapId.clear();
        _draftMap.atlasPlacement = {};
        _draftMap.atlasStatus = MapAtlasStatus::Standalone;
        _draftMap.atlasLevel = MapLevel::World;
    }

    resizeMapCells(_draftMap);
    ensureDefaultMapLayer(_draftMap);
    _draftMap.metadata.touch();
    _draftMap.revision = std::max(_draftMap.revision, latestRevision) + 1;
    synchronizeWorldBriefEntities();

    MapModel mapToSave = _draftMap;
    applyMapCreatorPlayerSettings(mapToSave, mergedPlayerSettings);
    if (_persistence.editingMapIndex >= 0 && _persistence.editingMapIndex < static_cast<int>(_currentCampaign.maps.size()))
    {
        preserveCurrentContextLinks(mapToSave.metadata, _currentCampaign.maps[_persistence.editingMapIndex].metadata);
        _currentCampaign.maps[_persistence.editingMapIndex] = std::move(mapToSave);
        _persistence.selectedMapIndex = _persistence.editingMapIndex;
    }
    else
    {
        _currentCampaign.maps.push_back(std::move(mapToSave));
        _persistence.editingMapIndex = static_cast<int>(_currentCampaign.maps.size()) - 1;
        _persistence.selectedMapIndex = _persistence.editingMapIndex;
    }

    if (_currentCampaign.partyLocation.mapId.empty())
    {
        _currentCampaign.partyLocation.mapId = _draftMap.metadata.id;
        _currentCampaign.partyLocation.x = std::min(_draftMap.width - 1, _draftMap.width / 2);
        _currentCampaign.partyLocation.y = std::min(_draftMap.height - 1, _draftMap.height / 2);
    }

    _currentCampaign.metadata.touch();
    const bool campaignSaved = saveTransaction.finish(
        ProjectSaveService::getInstance().saveCampaign(_currentCampaign));
    if (finalizeMapCreatorPlayerSettingsSave(
            campaignSaved, mergedPlayerSettings, _draftMap, _persistence.playerSettingsBaseline))
    {
        // Terrain/notes undo history must not resurrect a sharing decision
        // superseded in the presentation panel. Keep all authoring content.
        for (auto &snapshot : _history.undo)
            finalizeMapCreatorPlayerSettingsSave(true, mergedPlayerSettings, snapshot.map, snapshot.playerSettingsBaseline);
        for (auto &snapshot : _history.redo)
            finalizeMapCreatorPlayerSettingsSave(true, mergedPlayerSettings, snapshot.map, snapshot.playerSettingsBaseline);
    }
    if (campaignSaved)
    {
        _generation.setupEntities = mapSetupEntities(_draftMap.generation.recipe.facts);
        _generation.briefFacts = _draftMap.generation.recipe.facts;
    }
    if (campaignSaved)
        _integration.statusMessage = gm::localization::trUi(
            "Map saved. Review the player view before showing anything at the table.");
    if (campaignSaved && !_transfer.pendingPackageImportTransactionId.empty())
        finishPendingMapPackageImportRecovery(false);
    if (campaignSaved && !_history.pendingNamedCheckpointLabel.empty())
    {
        try
        {
            ProjectPackageLayout::ensureCampaignPackageLayout(_currentCampaign);
            MapCheckpointService service(ProjectPackageLayout::layoutForCampaign(_currentCampaign).rootDirectory);
            const MapCheckpointWriteResult checkpoint =
                service.createNamedCheckpoint(_draftMap, _history.pendingNamedCheckpointLabel);
            if (checkpoint.success)
                _history.pendingNamedCheckpointLabel.clear();
            else
                _integration.statusMessage = std::string(gm::localization::trUi("Map checkpoint failed:")) + " " +
                                 gm::localization::trSystemMessage(checkpoint.error);
        }
        catch (const std::exception &error)
        {
            _integration.statusMessage = std::string(gm::localization::trUi("Map checkpoint failed:")) + " " +
                             gm::localization::trSystemMessage(error.what());
        }
    }
    if (!campaignSaved)
        _integration.statusMessage = gm::localization::trUi(
            "The campaign could not be saved. Open Settings > Project Health for details.");
    try
    {
        std::string recoveryError;
        const MapCreatorRecoveryCleanupOutcome cleanup =
            finalizeMapCreatorRecoveryAfterPersistentSave(campaignSaved, [&]() {
                _history.crashJournalWorker.cancel();
                _history.journalSubmittedSerial.reset();
                ProjectPackageLayout::ensureCampaignPackageLayout(_currentCampaign);
                MapCheckpointService service(ProjectPackageLayout::layoutForCampaign(_currentCampaign).rootDirectory);
                return service.discardRecovery(_draftMap.metadata.id, &recoveryError);
            });
        if (cleanup == MapCreatorRecoveryCleanupOutcome::ClearFailed && !recoveryError.empty())
            _integration.statusMessage = std::string(gm::localization::trUi("Saved, but the recovery journal could not be cleared:")) +
                             " " + gm::localization::trSystemMessage(recoveryError);
        else if (cleanup == MapCreatorRecoveryCleanupOutcome::Cleared)
        {
            _history.hasRecoveryPreview = false;
            _history.recoveryPreview = {};
            _history.crashJournalPending = false;
            _history.crashJournalWriteFailed = false;
            _history.crashJournalLabel.clear();
        }
    }
    catch (const std::exception &error)
    {
        _integration.statusMessage = std::string(gm::localization::trUi("Saved, but the recovery journal could not be cleared:")) +
                         " " + gm::localization::trSystemMessage(error.what());
    }
    refreshNamedCheckpoints();
    if (campaignSaved)
    {
        _persistence.saveFailed = false;
        _persistence.cleanAuthoringSerial = _editing.authoringSerial;
        _history.draftHistory.saved();
        _persistence.saveReceipt = {
            true,
            _draftMap.metadata.id,
            _draftMap.revision,
            _editing.authoringSerial};
    }
    return campaignSaved;
}
