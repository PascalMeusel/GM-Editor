#pragma once

#include "Controllers/Editor/Widgets/IWidgetController.hpp"
#include "Models/Campaign/Campaign.hpp"
#include "Models/Map/MapGeneration.hpp"
#include "Models/Map/MapCheckpointService.hpp"
#include "Models/Map/MapCrashJournalWorker.hpp"
#include "Models/Map/MapEditTransactions.hpp"
#include "Models/Map/MapImageImportService.hpp"
#include "Models/Map/MapPackageService.hpp"
#include "Models/Map/MapModels.hpp"
#include "Models/Map/MapBrushStrokePreview.hpp"
#include "Models/Map/MapBrushRenderSnapshot.hpp"
#include "Models/Map/MapStrokeWidths.hpp"
#include "Models/Map/MapPngExport.hpp"
#include "Models/Map/MapPngExportTask.hpp"
#include "Models/Map/MapWorldBrief.hpp"
#include "Models/Map/MapStructuredCreation.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorView.hpp"
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorPersistencePolicy.hpp"
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorControllerTypes.hpp"
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorState.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

class MapCreatorController : public IWidgetController
{
public:
    using Tool = map_creator_controller::Tool;

    explicit MapCreatorController(Campaign &currentCampaign, MapCrashJournalWorker::BeforePublish journalPublish = {});

    void update(std::shared_ptr<GLFWwindow> window) override;
    bool beginSettlementFromWorldPlace(const EntityId &placeId);
    bool openMapForEditing(const EntityId &mapId);
    void beginNewMap(MapCreationIntent intent = MapCreationIntent::WorldOverview);
    bool hasUnsavedDraft() const;
    bool flushDraft();
    // Revoke old work before replacing the referenced Campaign, then initialize
    // a clean draft after replacement. Recovery files remain explicitly recoverable.
    void prepareForCampaignReplacement();
    void onCampaignReplaced();
    void pollRetiredCampaignWork();
    const std::string &draftStatusMessage() const { return _persistence.saveFailed && !_persistence.saveError.empty() ? _persistence.saveError : _integration.statusMessage; }
    bool currentCameraForMap(const EntityId &mapId, LocalPlayerDisplayCameraState &camera) const;
    void setReviewSavedMapHandler(std::function<bool(const EntityId &)> handler);
    void setOpenSavedMapHandler(std::function<bool(const EntityId &)> handler);
    void setAttachSavedMapToEncounterHandler(std::function<bool(const EntityId &)> handler)
    { _integration.attachSavedMapToEncounterHandler = std::move(handler); }
    void setAttachSavedMapToSceneHandler(std::function<bool(const EntityId &)> handler);
    void setOpenEncounterPreparationHandler(std::function<bool()> handler);

private:
    friend class MapCreatorView;
    friend struct WorkspaceInteractionProbe;
    friend struct MapCampaignBoundaryProbe;

    using GeneratorPreset = map_creator_controller::GeneratorPreset;
    using SelectedObjectKind = map_creator_controller::SelectedObjectKind;
    using UndoSnapshot = map_creator_controller::UndoSnapshot;
    using GenerationProgressSnapshot = map_creator_controller::GenerationProgressSnapshot;
    using DraftGenerationResult = map_creator_controller::DraftGenerationResult;
    using PngExportVisibilitySettings = map_creator_controller::PngExportVisibilitySettings;

    std::unique_ptr<MapCreatorView> _view;
    Campaign &_currentCampaign;
    MapModel _draftMap;
    map_creator_controller::GenerationState _generation;
    map_creator_controller::EditingState _editing;
    map_creator_controller::HistoryState _history;
    map_creator_controller::TransferState _transfer;
    map_creator_controller::PersistenceState _persistence;
    map_creator_controller::IntegrationState _integration;
    std::uint64_t _campaignEpoch = 0;
    std::uint64_t _pngExportEpoch = 0;
    void restoreTravelSetupFromRecipe();
    void synchronizeStructuredSetup();

    void setActiveTool(Tool tool);
    void cancelActiveTool();
    void selectTileFromLegend(int tileIndex);
    void applyActiveToolAt(int x, int y, const MapFeaturePoint *strokePoint = nullptr);
    bool activeToolUsesBrush() const;
    int activeBrushSize() const;
    const char *activeToolLabel() const;
    const char *nextActionHint() const;
    const char *toolHelpText() const;
    std::string activeSelectionSummary() const;
    std::string selectedObjectSummary() const;
    SelectedObjectKind selectedObjectKind() const;
    bool hasSelectedObject() const;
    MapTileKind activeTileKind() const;
    MapWayKind activeWayKind() const;
    MapSymbolVariantMode activeSymbolVariantMode() const;
    void selectObjectAt(int x, int y);
    void selectTokenObject(int index);
    void selectFeatureObject(int index);
    void selectGeneratedFeatureObject(int index);
    void selectRegionObject(int index);
    void selectRouteObject(int index);
    void normalizeObjectSelection();
    void clearObjectSelection();
    void applyBrushStampAt(int x, int y);
    void beginBrushStroke();
    void endBrushStroke();
    void captureUndoSnapshot(const std::string &label);
    void beginPropertyEditUndo(const std::string &label);
    void finishPropertyEditUndo();
    void refreshUndoState();
    void clearUndoSnapshot();
    void undoLastAction();
    void redoLastAction();
    void flushCrashJournal();
    void updateCrashJournal();
    void markDraftEdited();
    void restoreCrashRecovery();
    void discardCrashRecovery();
    void startNewMap(MapCreationIntent intent = MapCreationIntent::WorldOverview);
    void loadSelectedMap();
    void updateDraftArtwork(const std::string &undoLabel);
    void setAllDraftFog(bool hidden);
    bool saveDraftMap();
    bool savedDraftReadyForPlayerReview() const;
    bool reviewSavedMapForPlayers();
    bool openSavedMapInWorld();
    bool attachDraftMapToScene();
    bool attachDraftMapToEncounter();
    bool openEncounterPreparation();
    bool pendingAtlasSelectionMatchesDraft() const;
    void interpretWorldBrief();
    void synchronizeBriefFactDrafts();
    void setBriefFactStrength(std::size_t factIndex, MapGenerationFactStrength strength);
    void setBriefFactConfirmed(std::size_t factIndex, bool confirmed);
    void makeUnrecognizedBriefNotesOptional();
    MapGenerationValidationReport validateDraftGenerationRequest();
    void synchronizeWorldBriefEntities();
    void syncCreationIntentFromIndex(bool resizeToPreset);
    bool generationCandidateEligible(const MapGenerationResult &candidate) const;
    void queueDraftGeneration();
    void dispatchQueuedGeneration();
    void cancelDraftGeneration();
    void invalidateGenerationRequest();
    bool generationBusy() const;
    std::string generationProgressLabel() const;
    std::string generationDetailLabel() const;
    float generationProgressFraction() const;
    MapGenerationProgress currentGenerationProgress() const;
    MapGenerationOptions buildGenerationOptions();
    void finishDraftGeneration(const DraftGenerationResult &result);
    void applyGenerationCandidate(int candidateIndex);
    void queuePartialRegeneration(const MapGenerationSelection &selection);
    void discardPartialRegeneration();
    void applyPartialRegeneration();
    void setGeneratedFeatureLocked(const EntityId &featureId, bool locked);
    bool promoteGeneratedPlace(const EntityId &featureId);
    bool applyWorldPlaceHistoryTransition(const UndoSnapshot::WorldPlaceTransition &transition,
                                          const char *saveReason,
                                          const char *failurePrefix);
    void refreshNamedCheckpoints();
    void createNamedCheckpoint(const std::string &name);
    void previewNamedCheckpointRestore(int checkpointIndex);
    void restoreNamedCheckpoint();
    void discardNamedCheckpointRestorePreview();
    void generateDraftMap();
    void paintCell(int x, int y);
    void paintWayCell(int x, int y);
    void paintElevationCell(int x, int y);
    void paintTacticalEdge(int x, int y);
    void fogCell(int x, int y);
    void configureCell(int x, int y);
    void addLayer();
    void removeSelectedLayer();
    void addTokenAt(int x, int y);
    void removeSelectedToken();
    void addRegionAt(int x, int y);
    void removeSelectedRegion();
    void addFeatureAt(int x, int y);
    void removeSelectedFeature();
    void addRouteAt(int x, int y);
    void addRoute();
    void removeSelectedRoute();
    void setTokenLinkFromSelection(MapTokenModel &token);
    void exportDraftMap();
    MapModel currentPlayerOutputMap() const;
    void exportDraftPng(bool playerSafe);
    void exportDraftUvtt();
    void startPngExport(MapPngExportJob job);
    void updatePngExport();
    void exportDraftPngTiled(bool playerSafe, bool overwriteExisting = false);
    int selectedPngExportPixelsPerCell() const;
    MapPngExportOptions selectedPngExportOptions(bool playerSafe) const;
    MapPngExportSizing selectedPngExportSizing() const;
    void importDraftMap();
    void exportDraftMapLegacy();
    void importDraftMapLegacy();
    void browseForMapPackageExport();
    void browseForMapPackageImport();
    void previewMapPackageImport();
    void confirmMapPackageImport();
    void recoverInterruptedMapPackageImport();
    bool finishPendingMapPackageImportRecovery(bool discardAssets);
    void browseForReferenceImage(bool preserveExistingMapContract = false);
    void adoptLegacyReferenceImage();
    bool verifyReferenceImageForPlayerPublishing();
    bool setReferenceImagePlayerVisible(bool visible);
    void syncReferenceCalibrationDraft();
    void applyReferenceCalibration();
    bool commitImageTrace(const MapImageImport::TraceRequest &request);
    void removeReferenceImage();
    void resizeDraftCells();
    void syncScopeFromIndex();
    void updateViewCameraSnapshot(const EntityId &mapId,
                                  const LocalPlayerDisplayCameraState &camera);
    std::vector<MapTileKind> getPalette() const;
    std::vector<const char *> getParentMapLabels() const;
    std::vector<const char *> getLinkedEntityLabels() const;
    void onShow() override;
};
