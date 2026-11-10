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

bool MapCreatorController::pendingAtlasSelectionMatchesDraft() const
{
    EntityId selectedParentId;
    if (!_draftMap.isMainMap && _editing.selectedParentIndex > 0 &&
        _editing.selectedParentIndex - 1 < static_cast<int>(_currentCampaign.maps.size()))
    {
        selectedParentId = _currentCampaign.maps[
            static_cast<std::size_t>(_editing.selectedParentIndex - 1)].metadata.id;
    }
    return selectedParentId == MapAtlasService::effectiveParentId(_draftMap);
}

bool MapCreatorController::savedDraftReadyForPlayerReview() const
{
    return mapCreatorDraftReadyForPlayerReview(_persistence.saveReceipt, _persistence.loadedBaseline,
        _draftMap, _editing.authoringSerial, pendingAtlasSelectionMatchesDraft(), hasUnsavedDraft(),
        _currentCampaign.maps);
}

bool MapCreatorController::reviewSavedMapForPlayers()
{
    if (!savedDraftReadyForPlayerReview())
    {
        _integration.statusMessage = gm::localization::trUi(
            "Save the current map before reviewing the player view.");
        return false;
    }
    const EntityId reviewedMapId = _draftMap.metadata.id;
    if (!_integration.reviewSavedMapHandler ||
        !_integration.reviewSavedMapHandler(reviewedMapId))
    {
        _integration.statusMessage = gm::localization::trUi(
            "The player view could not be prepared. Nothing was shown to players.");
        return false;
    }
    const auto savedMap = std::find_if(
        _currentCampaign.maps.begin(), _currentCampaign.maps.end(),
        [&](const MapModel &candidate) { return candidate.metadata.id == reviewedMapId; });
    if (savedMap != _currentCampaign.maps.end() && _draftMap.metadata.id == reviewedMapId)
    {
        if (_persistence.playerSettingsBaseline.valid && _persistence.playerSettingsBaseline.mapId == reviewedMapId)
            _persistence.playerSettingsBaseline.visibility = savedMap->visibility;
        // The review handler has committed the visibility change. Synchronize
        // only that field: replacing the draft would discard authoring edits.
        if (_draftMap.visibility != savedMap->visibility)
        {
            const bool draftWasClean = _editing.authoringSerial == _persistence.cleanAuthoringSerial;
            const bool receiptWasCurrent = savedDraftReadyForPlayerReview();
            _draftMap.visibility = savedMap->visibility;
            markDraftEdited(); // Invalidate the creator's player-preview cache.
            if (draftWasClean)
            {
                _persistence.cleanAuthoringSerial = _editing.authoringSerial;
                _history.draftHistory.saved();
            }
            if (receiptWasCurrent)
                _persistence.saveReceipt.authoringSerial = _editing.authoringSerial;
        }
        // Undoing terrain or notes must not undo a separately committed player
        // visibility decision. Preserve every other field in both histories.
        for (auto &snapshot : _history.undo)
            if (snapshot.map.metadata.id == reviewedMapId)
            {
                snapshot.map.visibility = savedMap->visibility;
                if (snapshot.playerSettingsBaseline.valid && snapshot.playerSettingsBaseline.mapId == reviewedMapId)
                    snapshot.playerSettingsBaseline.visibility = savedMap->visibility;
            }
        // The review callback has now committed the loaded map together with
        // its blackout selection. This is the first durable receipt on load.
        _persistence.saveReceipt = {true, reviewedMapId, _draftMap.revision, _editing.authoringSerial};
        for (auto &snapshot : _history.redo)
            if (snapshot.map.metadata.id == reviewedMapId)
            {
                snapshot.map.visibility = savedMap->visibility;
                if (snapshot.playerSettingsBaseline.valid && snapshot.playerSettingsBaseline.mapId == reviewedMapId)
                    snapshot.playerSettingsBaseline.visibility = savedMap->visibility;
            }
    }
    _integration.statusMessage = gm::localization::trUi(
        "Player view prepared in blackout. Review it before arming the display.");
    return true;
}

bool MapCreatorController::openSavedMapInWorld()
{
    if (!savedDraftReadyForPlayerReview())
    {
        _integration.statusMessage = gm::localization::trUi(
            "Save the current map before leaving this workflow.");
        return false;
    }
    if (!_integration.openSavedMapHandler ||
        !_integration.openSavedMapHandler(_draftMap.metadata.id))
    {
        _integration.statusMessage = gm::localization::trUi(
            "The saved map could not be opened in World.");
        return false;
    }
    return true;
}

bool MapCreatorController::openEncounterPreparation()
{
    if (!_integration.openEncounterPreparationHandler || !_integration.openEncounterPreparationHandler())
    {
        _integration.statusMessage = gm::localization::trUi(
            "Encounter preparation could not be opened.");
        return false;
    }
    _integration.statusMessage = gm::localization::trUi(
        "Encounter preparation opened. Build the battle map around its objectives and participants.");
    return true;
}

bool MapCreatorController::attachDraftMapToScene()
{
    if (!_integration.attachSavedMapToSceneHandler || generationBusy())
        return false;
    // Only a successful persistent save can supply a scene link. Do not issue
    // another save from the view or infer an ID from the map-list selection.
    if (!saveDraftMap())
        return false;
    const EntityId savedMapId = _persistence.saveReceipt.mapId;
    if (!_persistence.saveReceipt.committed || savedMapId.empty())
        return false;
    const bool linked = _integration.attachSavedMapToSceneHandler(savedMapId);
    const bool german = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
    _integration.statusMessage = linked
        ? (german ? "Karte gespeichert und mit der Szene verknüpft." : "Map saved and attached to the scene.")
        : (german ? "Karte gespeichert, aber nicht verknüpft. Wähle eine Szene unter Sitzung und versuche es erneut."
                  : "Map saved, but could not attach it. Select a scene in Session and retry.");
    return linked;
}
