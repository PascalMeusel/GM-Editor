#pragma once

#include "Controllers/Editor/Widgets/MapCreator/MapCreatorControllerTypes.hpp"
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorPersistencePolicy.hpp"
#include "Models/Map/MapBrushRenderSnapshot.hpp"
#include "Models/Map/MapBrushStrokePreview.hpp"
#include "Models/Map/MapCheckpointService.hpp"
#include "Models/Map/MapCrashJournalWorker.hpp"
#include "Models/Map/MapGeneration.hpp"
#include "Models/Map/MapImageImportService.hpp"
#include "Models/Map/MapPackageService.hpp"
#include "Models/Map/MapPngExportTask.hpp"
#include "Models/Map/MapStrokeWidths.hpp"
#include "Models/Map/MapStructuredCreation.hpp"
#include "Models/Map/MapWorldBrief.hpp"
#include "Models/UX/DraftHistoryState.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <utility>

namespace map_creator_controller
{
enum class Tool
{
    InspectSelect,
    PaintTile,
    EraseTile,
    PaintWay,
    EraseWay,
    PaintElevation,
    PaintTacticalEdge,
    PlaceToken,
    AddFeature,
    AddRegion,
    AddRoute,
    Fog
};

struct GenerationState
{
    int selectedCreationIntentIndex = static_cast<int>(MapCreationIntent::WorldOverview);
    int selectedSizePresetIndex = static_cast<int>(MapSizePreset::Standard);
    int selectedGeneratorIndex = 0;
    int worldEntryMode = 0;
    int worldIdeaIndex = 0;
    int selectedSymbolVariantModeIndex = 0;
    int generatorMountainPatternIndex = 0;
    int generatorRoadNetworkIndex = 2;
    int generatorQualityIndex = 1;
    int generatorPurposeIndex = static_cast<int>(MapGenerationPurpose::Region);
    int generatorLandscapeIndex = static_cast<int>(MapLandscapeProfile::RiverValley);
    bool structuredCreation = true;
    int travelPurposeIndex = 1;
    int travelBiomeIndex = 0;
    int travelDangerLevel = 1;
    bool travelCombatReady = false;
    std::vector<MapSetupEntity> setupEntities;
    std::string setupEntityName;
    EntityId setupEntityNationId;
    bool setupEntityIsNation = false;
    int generatorRequestedSettlements = 3;
    int generatorRequestedLandmarks = 4;
    int generatorEdgeConnections = 2;
    std::string worldBrief;
    std::vector<std::string> briefNationNames;
    std::vector<std::string> briefCityNames;
    std::vector<MapGenerationFact> briefFacts;
    std::string briefCapitalName;
    bool worldBriefParsed = false;
    int selectedGenerationCandidate = -1;
    int recommendedGenerationCandidate = -1;
    int rejectedGenerationCandidateCount = 0;
    std::uint32_t generatorSeed = 240529u;
    int generatorWaterLevel = 20;
    int generatorMountainDensity = 18;
    int generatorForestDensity = 25;
    int generatorSettlementDensity = 8;
    int generatorRouteDensity = 28;
    int generatorTacticalDensity = 18;
    bool queued = false;
    bool inProgress = false;
    bool previewCandidateStyleAssets = false;
    std::future<DraftGenerationResult> future;
    bool retired = false;
    std::uint64_t requestSerial = 0;
    std::uint64_t queuedSerial = 0;
    std::uint64_t activeSerial = 0;
    std::shared_ptr<std::atomic_bool> cancellation;
    MapModel previousDraft;
    MapGenerationOptions options;
    MapGenerationQualityReport lastQuality;
    MapGenerationValidationReport lastValidation;
    MapGenerationBatchFailureCode lastBatchFailure = MapGenerationBatchFailureCode::None;
    std::vector<MapGenerationResult> candidates;
    int presetIndex = 0;
    std::shared_ptr<GenerationProgressSnapshot> progress;
    std::chrono::steady_clock::time_point startedAt{};
    bool hasLastQuality = false;
    bool partialRequested = false;
    EntityId partialSourceMapId;
    std::uint64_t partialSourceRevision = 0;
    MapGenerationSelection partialSelection;
    MapGenerationPatchPreview pendingPatch;
    bool hasPendingPatch = false;
};

struct EditingState
{
    int selectedTileIndex = 0;
    int selectedWayIndex = 1;
    int selectedParentIndex = 0;
    int selectedLayerIndex = 0;
    int selectedTokenIndex = -1;
    int selectedRegionIndex = -1;
    int selectedFeatureIndex = -1;
    int selectedGeneratedFeatureIndex = -1;
    int selectedRouteIndex = -1;
    int selectedTokenKindIndex = 0;
    int selectedFeatureKindIndex = 0;
    int selectedLightingIndex = 0;
    int selectedTacticalPresetIndex = static_cast<int>(MapTacticalPreset::Wall);
    int selectedTacticalChannelIndex = 2;
    int selectedElevation = 0;
    int selectedLinkedEntityIndex = 0;
    int brushSize = 1;
    MapStrokeWidths strokeWidths;
    bool roundBrush = true;
    bool revealFog = false;
    int rotationQuarterTurns = 0;
    float zoom = 1.0f;
    EntityId viewCameraMapId;
    LocalPlayerDisplayCameraState viewCamera;
    bool viewCameraValid = false;
    bool paintMode = true;
    bool tacticalOneWay = false;
    bool autoRotateTiles = true;
    bool showEditorGrid = false;
    Tool activeTool = Tool::PaintTile;
    bool strokeInProgress = false;
    std::uint64_t brushStrokeRenderSerial = 0;
    MapBrushStrokePreview brushStrokePreview;
    MapBrushRenderSnapshot brushRenderSnapshot;
    std::vector<MapCellCoordinate> brushProtectedWayCells;
    std::vector<std::vector<MapFeaturePoint>> wayStrokeParts;
    int lastBrushX = -1;
    int lastBrushY = -1;
    std::uint64_t authoringSerial = 0;
    bool focusViewRequested = false;
};

struct HistoryState
{
    explicit HistoryState(MapCrashJournalWorker::BeforePublish publish = {}) : crashJournalWorker(std::move(publish)) {}
    static constexpr std::size_t maximumUndoEntries = 100;
    std::vector<UndoSnapshot> undo;
    std::vector<UndoSnapshot> redo;
    bool hasUndo = false;
    bool hasRedo = false;
    bool propertyEditUndoActive = false;
    std::string undoLabel;
    std::string redoLabel;
    bool crashJournalPending = false;
    MapCrashJournalWorker crashJournalWorker;
    std::optional<std::uint64_t> journalSubmittedSerial;
    std::chrono::steady_clock::time_point journalRetryAt{};
    bool crashJournalWriteFailed = false;
    bool hasRecoveryPreview = false;
    MapCheckpointRestorePreview recoveryPreview;
    std::string crashJournalLabel;
    std::string pendingNamedCheckpointLabel;
    std::vector<MapCheckpointInfo> namedCheckpoints;
    std::string newCheckpointName;
    int selectedNamedCheckpointIndex = -1;
    MapCheckpointRestorePreview namedCheckpointRestorePreview;
    bool hasNamedCheckpointRestorePreview = false;
    DraftHistoryState draftHistory;
};

struct TransferState
{
    bool exportPlayerSafe = true;
    bool exportCurrentViewport = false;
    bool exportViewportValid = false;
    MapPngExportRect exportViewportRect{};
    int exportPngSizePresetIndex = static_cast<int>(MapPngExportSizePreset::Screen);
    int exportPixelsPerCell = 48;
    PngExportVisibilitySettings exportGmSettings{};
    PngExportVisibilitySettings exportPlayerSettings{
        false, true, false, false, false, false, false, true};
    std::string importExportPath;
    std::string manualPngExportPath;
    std::string manualUvttExportPath;
    std::string manualTiledPngManifestPath;
    MapPackagePreview mapPackagePreview;
    bool hasMapPackagePreview = false;
    bool confirmMapPackageImport = false;
    std::string pendingPackageImportTransactionId;
    float importCropX = 0.0f;
    float importCropY = 0.0f;
    float importCropWidth = 1.0f;
    float importCropHeight = 1.0f;
    float importRotationDegrees = 0.0f;
    bool importSpatialCalibrated = false;
    float importExtentWidth = 0.0f;
    float importExtentHeight = 0.0f;
    int importDistanceUnitIndex = static_cast<int>(MapDistanceUnit::Unspecified);
    std::string lastExportPath;
    MapPngExportTask pngExportTask;
};

struct PersistenceState
{
    int editingMapIndex = -1;
    int selectedMapIndex = -1;
    int selectedScopeIndex = 0;
    std::uint64_t cleanAuthoringSerial = 0;
    MapCreatorPersistentSaveReceipt saveReceipt;
    MapCreatorLoadedDraftBaseline loadedBaseline;
    bool saveFailed = false;
    std::string saveError;
    MapCreatorPlayerSettingsBaseline playerSettingsBaseline;
};

struct IntegrationState
{
    std::function<bool(const EntityId &)> reviewSavedMapHandler;
    std::function<bool(const EntityId &)> openSavedMapHandler;
    std::function<bool(const EntityId &)> attachSavedMapToSceneHandler;
    std::function<bool(const EntityId &)> attachSavedMapToEncounterHandler;
    std::function<bool()> openEncounterPreparationHandler;
    std::string statusMessage;
};
}
