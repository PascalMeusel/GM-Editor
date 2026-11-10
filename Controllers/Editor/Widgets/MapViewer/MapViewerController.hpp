#pragma once

#include "Controllers/Editor/Widgets/IWidgetController.hpp"
#include "Controllers/Editor/Widgets/MapViewer/MapViewerFogStroke.hpp"
#include "Models/Audio/AudioPlaybackService.hpp"
#include "Models/Campaign/Campaign.hpp"
#include "Models/Map/MapAtlasService.hpp"
#include "Models/Map/MapGeneration.hpp"
#include "Models/Map/TravelSceneGenerator.hpp"
#include "Views/Editor/Widgets/Map/MapViewer/MapViewerView.hpp"

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>

enum class MapViewerInteractionMode
{
    InspectSelect,
    MoveParty,
    SelectDetailArea,
    RevealFog,
    HideFog,
    PingPlayers
};

struct MapDetailGenerationProgressState
{
    std::atomic<int> step{0};
    std::atomic<int> totalSteps{24};
    std::atomic<bool> discardResult{false};
};

enum class MapDetailGenerationRejectionReason
{
    None,
    LockedAnchors,
    UsableTerrain,
    RoadContinuity,
    RiverOrCrossingContinuity,
    SettlementAccess,
    QualityFloor,
    Count
};

struct MapDetailGenerationJobResult
{
    std::uint64_t requestSerial = 0;
    EntityId parentMapId;
    std::uint64_t parentRevision = 0;
    MapNormalizedRect parentSelection;
    MapLevel targetLevel = MapLevel::Region;
    std::vector<MapGenerationResult> candidates;
    MapGenerationBatchFailureCode failure = MapGenerationBatchFailureCode::None;
    int checkedCandidateCount = 0;
    int inheritedAnchorCount = 0;
    int rejectedCandidateCount = 0;
    std::array<int, static_cast<std::size_t>(MapDetailGenerationRejectionReason::Count)> rejectionReasonCounts{};
    MapDetailGenerationRejectionReason primaryRejectionReason = MapDetailGenerationRejectionReason::None;
    bool discarded = false;
};

class MapViewerController : public IWidgetController
{
    friend struct UiGuidanceTestProbe;
public:
    MapViewerController(Campaign &currentCampaign, std::shared_ptr<AudioPlaybackService> playbackService = nullptr);
    ~MapViewerController() override;

    void update(std::shared_ptr<GLFWwindow> window) override;
    bool selectMapById(const EntityId &mapId);
    void openTravelWorkflow();
    bool currentCameraForMap(const EntityId &mapId, LocalPlayerDisplayCameraState &camera) const;
    bool transientFogEditActive() const { return _fogStroke.active(); }
    void cancelTransientFogEdit() { cancelFogStroke(); }

private:
    friend class MapViewerView;

    std::unique_ptr<MapViewerView> _view;
    bool _focusViewRequested = false;
    Campaign &_currentCampaign;
    std::shared_ptr<AudioPlaybackService> _playbackService;
    int _selectedMapIndex = 0;
    EntityId _selectedCampaignId;
    bool _mapSelectionExplicit = false;
    int _selectedRouteIndex = 0;
    int _selectedTravelPaceIndex = 1;
    int _selectedCellX = -1;
    int _selectedCellY = -1;
    int _pendingPartyMoveX = -1;
    int _pendingPartyMoveY = -1;
    bool _hasPendingPartyMove = false;
    bool _previewCandidateStyleAssets = false;
    bool _showPoliticalOverlay = false;
    MapViewerInteractionMode _interactionMode = MapViewerInteractionMode::InspectSelect;
    MapViewerFogStroke _fogStroke;
    int _fogStrokeLastX = -1;
    int _fogStrokeLastY = -1;
    std::string _mapInteractionStatusMessage;
    float _zoom = 1.0f;
    EntityId _viewCameraMapId;
    LocalPlayerDisplayCameraState _viewCamera;
    bool _viewCameraValid = false;
    bool _fitZoomRequested = true;
    int _detailStartX = -1;
    int _detailStartY = -1;
    int _detailEndX = -1;
    int _detailEndY = -1;
    bool _detailSelectionDragging = false;
    MapLevel _detailTargetLevel = MapLevel::Region;
    bool _detailAdvancedLevelOverride = false;
    std::string _detailMapName;
    std::vector<MapGenerationResult> _detailCandidates;
    int _selectedDetailCandidate = -1;
    std::future<MapDetailGenerationJobResult> _detailGenerationFuture;
    std::shared_ptr<MapDetailGenerationProgressState> _detailGenerationProgress;
    std::uint64_t _detailRequestSerial = 0;
    // Invalidates renderer-independent plans after any in-memory atlas/viewer
    // mutation, including a change whose disk save failed.
    std::uint64_t _mapRenderContentVersion = 1;
    bool _detailGenerationRunning = false;
    std::string _atlasStatusMessage;
    MapAtlasPromotionPreview _promotionPreview;
    bool _hasPromotionPreview = false;
    MapAtlasRebasePreview _rebasePreview;
    bool _hasRebasePreview = false;
    std::vector<int> _rebaseResolutionIndices;
    int _travelScenePurposeIndex = static_cast<int>(TravelScenePurpose::Discovery);
    int _travelDangerLevel = 1;
    int _selectedTravelSceneCandidate = -1;
    bool _travelSceneCombatReady = false;
    std::uint32_t _travelSceneSeed = 240529u;
    std::string _travelRequiredElements;
    std::string _travelExcludedElements;
    std::string _travelSceneStatus;
    std::vector<TravelSceneCandidate> _travelSceneCandidates;
    EntityId _travelSceneSessionId, _travelSceneCampaignId;

    MapModel *getSelectedMap();
    const MapModel *getSelectedMap() const;
    MapModel *findMapById(const EntityId &mapId);
    const MapModel *findMapById(const EntityId &mapId) const;
    std::vector<const MapModel *> selectedMapBreadcrumbs() const;
    std::vector<int> childMapIndices(const EntityId &parentMapId) const;
    bool selectParentMap();
    bool hasDetailSelection() const;
    MapNormalizedRect selectedDetailRect() const;
    void beginDetailSelection(int x, int y);
    void updateDetailSelection(int x, int y);
    void finishDetailSelection(int x, int y);
    void clearDetailSelection();
    void setDetailTargetLevel(MapLevel level, bool advancedOverride);
    void generateDetailCandidates();
    void cancelDetailGeneration();
    void pollDetailGeneration();
    bool createSelectedDetailMap();
    bool placeExistingMap(const EntityId &childId);
    bool expandWorldMap(int eastCells, int southCells);
    void preparePromotionPreview();
    bool applyPromotionPreview();
    void prepareRebasePreview();
    bool rebasePreviewReadyToApply() const;
    bool applyRebasePreview();
    void setInteractionMode(MapViewerInteractionMode mode);
    bool playerPingModeAvailable() const;
    void beginFogStroke(int x, int y);
    void continueFogStroke(int x, int y);
    void finishFogStroke();
    void cancelFogStroke();
    bool pingPlayersAtCell(int x, int y);
    void selectCell(int x, int y);
    void clearSelectedCell();
    void previewPartyMoveToCell(int x, int y);
    void clearPendingPartyMove();
    void applyPendingPartyMove();
    void movePartyToCell(int x, int y);
    void startTravelOnSelectedRoute();
    void advanceTravelWatch();
    void generateTravelSceneCandidates();
    bool acceptSelectedTravelScene(bool prepareEncounter);
    void revealAllFogOnSelectedMap();
    void clearAllFogOnSelectedMap();
    void updateViewCameraSnapshot(const EntityId &mapId, float centerX, float centerY, float zoom);
    bool saveCampaign();
    void onShow() override;
};
