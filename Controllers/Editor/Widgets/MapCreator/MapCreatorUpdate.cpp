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

void MapCreatorController::update(std::shared_ptr<GLFWwindow> window)
{
    pollRetiredCampaignWork();
    updatePngExport();
    if (!m_isOpen)
        return;

    dispatchQueuedGeneration();

    if (_generation.inProgress && _generation.future.valid())
    {
        if (_generation.future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
        {
            const bool requestIsCurrent = _generation.activeSerial == _generation.requestSerial;
            try
            {
                const DraftGenerationResult result = _generation.future.get();
                if (requestIsCurrent && result.requestSerial == _generation.requestSerial)
                    finishDraftGeneration(result);
                else
                {
                    _generation.inProgress = false;
                    _generation.previousDraft = {};
                    _generation.progress.reset();
                    _generation.cancellation.reset();
                    _generation.activeSerial = 0;
                    _integration.statusMessage = gm::localization::trUi("Discarded an outdated map generation result.");
                }
            }
            catch (const std::exception &e)
            {
                _generation.inProgress = false;
                _generation.previousDraft = {};
                _generation.progress.reset();
                _generation.cancellation.reset();
                _generation.activeSerial = 0;
                if (requestIsCurrent)
                    _integration.statusMessage = std::string(gm::localization::trUi("Map generation failed after")) + " " +
                                     elapsedGenerationLabel(_generation.startedAt) + ": " +
                                     gm::localization::trSystemMessage(e.what());
            }
            catch (...)
            {
                _generation.inProgress = false;
                _generation.previousDraft = {};
                _generation.progress.reset();
                _generation.cancellation.reset();
                _generation.activeSerial = 0;
                if (requestIsCurrent)
                    _integration.statusMessage = std::string(gm::localization::trUi("Map generation failed after")) + " " + elapsedGenerationLabel(_generation.startedAt) + ".";
            }
        }
        else
        {
            if (_generation.activeSerial != _generation.requestSerial)
                _integration.statusMessage = gm::localization::trUi("Finishing a cancelled generator task. Its result will be discarded.");
            else
                _integration.statusMessage = generationDetailLabel() + " " + gm::localization::trUi("Running in the background for") + " " + elapsedGenerationLabel(_generation.startedAt) + ".";
        }
    }

    // The editor canvas is only a valid follow source in frames where it was
    // actually drawn. This prevents a hidden creation step or stale draft from
    // continuing to steer the player display.
    _editing.viewCameraValid = false;
    if (_editing.focusViewRequested)
    {
        ImGui::SetNextWindowFocus();
        _editing.focusViewRequested = false;
    }
    _view->update();
    // A geometry drag has the same transaction boundary as a paint stroke.
    // Write recovery after release, not the first intermediate mouse position.
    if (!_editing.strokeInProgress && !_history.propertyEditUndoActive && !_view->hasPendingAuthoringInteraction())
        updateCrashJournal();
    if (!_view->isVisible())
        m_isOpen = false;
}

bool MapCreatorController::currentCameraForMap(
    const EntityId &mapId,
    LocalPlayerDisplayCameraState &camera) const
{
    if (!_editing.viewCameraValid || mapId.empty() || _editing.viewCameraMapId != mapId)
        return false;
    camera = _editing.viewCamera;
    return true;
}

void MapCreatorController::updateViewCameraSnapshot(
    const EntityId &mapId,
    const LocalPlayerDisplayCameraState &camera)
{
    if (mapId.empty())
    {
        _editing.viewCameraValid = false;
        return;
    }
    _editing.viewCameraMapId = mapId;
    _editing.viewCamera = camera;
    normalizeLocalPlayerDisplayCamera(_editing.viewCamera);
    _editing.viewCamera.followGmCamera = true;
    _editing.viewCameraValid = true;
}
