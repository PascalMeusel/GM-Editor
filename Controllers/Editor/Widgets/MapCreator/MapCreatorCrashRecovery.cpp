#include "MapCreatorController.hpp"

#include "MapCreatorInternals.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Map/MapCheckpointService.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"

#include <exception>
#include <string>
#include <utility>

using namespace gm::map_creator_detail;

void MapCreatorController::restoreCrashRecovery()
{
    if (!_history.hasRecoveryPreview || !_history.recoveryPreview.valid)
        return;
    try
    {
        ProjectPackageLayout::ensureCampaignPackageLayout(_currentCampaign);
        MapCheckpointService service(ProjectPackageLayout::layoutForCampaign(_currentCampaign).rootDirectory);
        MapModel recovered = _draftMap;
        std::string error;
        if (!service.applyRestore(_history.recoveryPreview, recovered, true, &error))
        {
            _integration.statusMessage = std::string(gm::localization::trUi("Map recovery failed:")) + " " +
                             gm::localization::trSystemMessage(error);
            return;
        }
        captureUndoSnapshot("Restore Map Recovery");
        _draftMap = std::move(recovered);
        repairCollapsedMapSetupFeatureReferences(_currentCampaign, _draftMap);
        _generation.worldBrief = _draftMap.creationIntent == MapCreationIntent::WorldOverview
            ? _draftMap.description : std::string{};
        _generation.worldBriefParsed = false;
        resizeMapCells(_draftMap);
        ensureDefaultMapLayer(_draftMap);
        normalizeObjectSelection();
        syncReferenceCalibrationDraft();
        service.discardRecovery(_draftMap.metadata.id, nullptr);
        _history.hasRecoveryPreview = false;
        _history.recoveryPreview = {};
        _integration.statusMessage = gm::localization::trUi("Recovered the unsaved map changes.");
    }
    catch (const std::exception &error)
    {
        _integration.statusMessage = std::string(gm::localization::trUi("Map recovery failed:")) + " " +
                         gm::localization::trSystemMessage(error.what());
    }
}

void MapCreatorController::discardCrashRecovery()
{
    if (!_history.hasRecoveryPreview)
        return;
    try
    {
        ProjectPackageLayout::ensureCampaignPackageLayout(_currentCampaign);
        MapCheckpointService service(ProjectPackageLayout::layoutForCampaign(_currentCampaign).rootDirectory);
        std::string error;
        if (!service.discardRecovery(_draftMap.metadata.id, &error))
        {
            _integration.statusMessage = std::string(gm::localization::trUi("Could not discard map recovery:")) + " " +
                             gm::localization::trSystemMessage(error);
            return;
        }
        _history.hasRecoveryPreview = false;
        _history.recoveryPreview = {};
        _integration.statusMessage = gm::localization::trUi("Discarded the unsaved recovery copy.");
    }
    catch (const std::exception &error)
    {
        _integration.statusMessage = std::string(gm::localization::trUi("Could not discard map recovery:")) + " " +
                         gm::localization::trSystemMessage(error.what());
    }
}
