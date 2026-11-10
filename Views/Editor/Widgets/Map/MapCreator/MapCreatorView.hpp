#pragma once

#include "Models/Map/MapModels.hpp"
#include "Models/Map/MapRenderPlanCache.hpp"
#include "Models/Map/MapPreviewCompiler.hpp"
#include "Models/Map/MapEditTransactions.hpp"
#include "Models/Map/MapEditConsistency.hpp"
#include "Views/Editor/Widgets/DockableWidget.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapAuthoringWorkspaceContract.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapObjectEditing.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapTerrainGroupEditing.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapAreaSelection.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapFogOverlay.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapRegionEditing.hpp"
#include <string>
#include <vector>
#include <future>
#include <atomic>
#include <optional>

class MapCreatorController;
struct MapGenerationValidationReport;
namespace gm::map_creator_view_detail
{
struct CanvasFrame;
struct WorkflowFrame;
}

class MapCreatorView : public DockableWidget
{
  public:
    explicit MapCreatorView(MapCreatorController &controller);
    ~MapCreatorView() override;
    void resetForCampaign();
    void pollRetiredCampaignWork();
    bool hasPendingAuthoringInteraction() const
    {
        return _generatedFeatureGeometryEditActive || _terrainDragging || _areaDragging || _tokenDragArmed || _imageTraceActive ||
               _regionHandle != 0;
    }
    void requestLoadMapReview()
    {
        setWorkflowStep(0);
        _advancedCreation = true;
        _loadMapReviewRequested = true;
    }
    void requestNewMapReview(MapCreationIntent intent = MapCreationIntent::WorldOverview)
    {
        _newMapIntent = intent;
        _newMapReviewRequested = true;
    }
    MapCreationIntent _newMapIntent = MapCreationIntent::WorldOverview;
    void setWorkflowStep(int step)
    {
        // Keep the existing creation/edit/export IDs for controller callers.
        // Player preview is a persistent authoring view, not a creation step.
        const int nextStep = step < 0 ? 0 : (step > 5 ? 5 : step);
        if (nextStep != _workflowStep)
        {
            _regionHandle = 0;
            _regionDragging = false;
            cancelPendingInspectorReveal();
        }
        if (nextStep != _workflowStep)
            _resetWorkflowScroll = true;
        if (gm::mapui::mapCreatorShouldFitOnStageChange(_workflowStep, nextStep))
        {
            _fitCanvasOnNextDraw = true;
            _fitCanvasDelayFrames = 1;
        }
        _workflowStep = nextStep;
    }

  protected:
    int _lastStatusLocale = -1;
    void draw() override;

  private:
    friend struct WorkspaceInteractionProbe;
    friend struct MapCampaignBoundaryProbe;
    MapCreatorController &_controller;
    MapRenderPlanCache _renderPlanCache{32};
    MapPreviewCompiler _canvasPreviewCompiler;
    double _nextCanvasPreviewRetry = 0;
    std::future<MapModel> _setupPreviewJob;
    bool _discardSetupPreviewResult = false;
    std::shared_ptr<std::atomic_bool> _setupPreviewCancel;
    std::optional<MapModel> _setupPreviewMap;
    std::string _setupPreviewObservedKey, _setupPreviewPendingKey, _setupPreviewReadyKey, _setupPreviewFailedKey;
    double _setupPreviewChangedAt = 0;
    std::uint64_t _setupPreviewRevision = 0;
    void drawSetupPreview(ImVec2 size);
    void drawArtStyleControl(bool generationBusy, bool compact = false);
    void applyDraftDimensions();
    void useSelectedWorldIdea();
    void prepareWorldBriefForGeneration();
    void drawIdeaControls(bool generationBusy);
    void drawFactsControls(bool generationBusy);
    void drawPlayerReviewControls(bool generationBusy);
    void drawExportControls(bool generationBusy);
    void drawExportPreview();
    void drawPlayerPreview(bool stackedLayout, bool generationBusy);
    void drawDesignCandidates(bool stackedLayout);
    void drawGenerationConflicts();
    int _workflowStep = 0;
    bool _resetWorkflowScroll = true;
    bool _loadMapReviewRequested = false;
    bool _newMapReviewRequested = false;
    bool _advancedCreation = false;
    bool _keepGenerationSeed = false;
    EntityId _dimensionDraftMapId;
    int _draftWidth = 72;
    int _draftHeight = 48;
    std::string _pendingGeneratedFeatureProjectionId;
    MapGeneratedFeature _pendingGeneratedFeatureOriginal;
    bool _hasPendingGeneratedFeatureProjection = false;
    int _selectedGeneratedFeaturePoint = -1;
    int _draggedGeneratedFeaturePoint = -1;
    gm::mapui::MapObjectEditMode _objectEditMode = gm::mapui::MapObjectEditMode::Move;
    bool _draggingWholeObject = false;
    MapFeaturePoint _objectDragStart;
    std::string _generatedFeatureGeometryEditId;
    MapGeneratedFeature _generatedFeatureGeometryBeforeEdit;
    bool _generatedFeatureGeometryEditActive = false;
    bool _geometryPreviewPending = false;
    std::uint64_t _generatedFeatureGeometryRenderSerial = 0;
    gm::mapui::TerrainGroupSelection _terrainSelection;
    std::uint64_t _terrainSelectionSerial = 0;
    bool _terrainDragArmed = false, _terrainDragging = false;
    MapFeaturePoint _terrainDragStart;
    int _terrainDeltaX = 0, _terrainDeltaY = 0;
    // Token movement is committed only on button-up. This keeps a cancelled
    // drag mutation-free and gives each completed move one undo snapshot.
    bool _tokenDragArmed = false;
    int _draggedTokenIndex = -1;
    MapFeaturePoint _tokenDragStart;
    float _tokenDragStartX = 0.0f;
    float _tokenDragStartY = 0.0f;
    bool _boxSelectMode = false, _boxArmed = false, _boxDragging = false;
    int _regionHandle = 0, _regionDragIndex = -1, _regionGestureTool = 0;
    bool _regionDragging = false;
    MapFeaturePoint _regionDragStart;
    gm::mapui::RegionBounds _regionBefore, _regionPreview;
    std::string _regionGestureMapId, _regionGestureId;
    std::uint64_t _regionGestureSerial = 0;
    bool handleRegionCanvasInteraction(ImVec2 origin, float cellSize, bool hovered);
    MapFeaturePoint _boxStart, _boxEnd;
    std::vector<gm::mapui::AreaSelectionCandidate> _areaCandidates;
    std::uint64_t _areaSelectionSerial = 0;
    int _areaFilter = 0;
    char _areaSearch[96]{};
    void drawAreaSelectionOverlay(ImDrawList *canvas, ImVec2 origin, float cellSize);
    void drawAreaSelectionSidebar();
    ImVec2 _interactionCanvasOrigin;
    float _interactionCanvasCellSize=0;
    bool _areaDragArmed=false, _areaDragging=false;
    MapFeaturePoint _areaDragStart;
    int _areaDeltaX=0, _areaDeltaY=0;
    int _previewedGenerationCandidate = 0;
    float _designPreviewZoom = 1.0f;
    bool _designPreviewShowLabels = true;
    bool _designPreviewShowPoliticalBorders = false;
    bool _resetDesignPreviewScroll = true;
    int _lastInteractivePreviewCandidate = -1;
    bool _fitCanvasOnNextDraw = true;
    int _fitCanvasDelayFrames = 1;
    bool _showWorldPoliticalOverlay = false;
    float _editorFogOpacity = 0.28f;
    bool _showCompactInspector = false;
    // A selection must not move the canvas between its press and release.
    bool _pendingCanvasInspectorReveal = false;
    void cancelPendingInspectorReveal();
    bool _drawingCompactInspector = false;
    bool _resetInspectorScroll = true;
    std::string _inspectorContext;
    int _toolCategory = 0;
    float _playerPreviewZoom = 1.0f;
    gm::mapui::MapCreatorPlayerPreviewCache _playerPreviewCache;
    bool _showTileLegend = false;
    std::string _lastSelectionSummary;
    std::vector<MapEditConflict> _mapEditConflicts;
    struct ConflictInspection
    {
        EntityId mapId;
        std::uint64_t serial;
        std::vector<MapEditConflict> conflicts;
        std::string error;
    };
    std::future<ConflictInspection> _mapEditInspection;
    bool _discardInspectionResult = false;
    std::string _mapEditInspectionError;
    double _mapEditInspectionRetryAt = 0;
    EntityId _mapEditConflictsMapId;
    std::uint64_t _mapEditConflictsSerial = 0;
    bool _mapEditConflictsReady = false;
    std::string _mapEditConflictMessage;
    int _regenerationFamilyIndex = 0;
    bool _regenerationUseArea = false;
    bool _regenerationPickingArea = false;
    bool _regenerationAreaDragging = false;
    int _regenerationAreaStartX = 0;
    int _regenerationAreaStartY = 0;
    int _regenerationAreaEndX = 0;
    int _regenerationAreaEndY = 0;
    bool _openPartialRegenerationConfirmation = false;
    bool _openCheckpointRestoreConfirmation = false;
    bool _imageTraceActive = false;
    int _imageTraceModeIndex = 0;
    std::vector<MapFeaturePoint> _imageTraceControlPoints;
    std::string _imageTraceName;
    bool _imageTraceSnap = true;
    bool _imageTraceSmooth = true;
    bool _imageTraceGmOnly = true;
    int _imageTraceDraggedPoint = -1;

    void drawToolControls();
    void drawSimpleCreation();
    void drawStructuredCreation();
    void drawStructuredEntities();
    bool drawWorkflowNavigation(MapModel &map, bool generationBusy, bool authoring, bool stackedEditLayout);
    void drawWorkflowLayout(const gm::map_creator_view_detail::WorkflowFrame &frame);
    void drawWorkflowStageControls(const gm::map_creator_view_detail::WorkflowFrame &frame,
                                   bool stackedLayout);
    void drawWorkflowConfirmationModals(const gm::map_creator_view_detail::WorkflowFrame &frame);
    void drawWorkflowControlFooter(const gm::map_creator_view_detail::WorkflowFrame &frame);
    void drawAuthoringToolbar();
    void drawToolPalette(bool inSidebar = false);
    int _sidebarPage = 0;
    void drawMapOutline();
    void drawFactReviewCards(const MapGenerationValidationReport &requestValidation);
    void drawMapCanvas(bool compactInspector);
    void drawMapCanvasTerrain(const gm::map_creator_view_detail::CanvasFrame &frame);
    void drawMapCanvasObjects(const gm::map_creator_view_detail::CanvasFrame &frame);
    void drawMapCanvasSelection(const gm::map_creator_view_detail::CanvasFrame &frame);
    void handleMapCanvasPrimaryInput(const gm::map_creator_view_detail::CanvasFrame &frame);
    void drawMapCanvasToolPreview(const gm::map_creator_view_detail::CanvasFrame &frame, const ImVec2 &mouse, int x, int y);
    void finishMapCanvasGestures(const gm::map_creator_view_detail::CanvasFrame &frame);
    void finishGeneratedFeatureGeometryEdit();
    bool removeSelectedGeneratedFeaturePoint();
    void resetGeneratedFeatureGeometryInteraction();
    void drawImageTraceControls();
    void cancelImageTrace();
    void drawPartialRegenerationControls();
    void drawPartialRegenerationPreview();
    void drawCheckpointControls();
    MapGenerationSelection partialRegenerationSelection() const;
    void drawSelectableTileLegend(const std::vector<MapTileKind> &tiles);
    void drawMapSidePanels();
    void drawSelectedObjectPanel();
    void drawSelectedTokenInspector();
    void drawSelectedFeatureInspector();
    void drawSelectedGeneratedFeatureInspector();
    void drawSelectedRegionInspector();
    void drawSelectedRouteInspector();
    std::string inspectorContext() const;
    void drawGeneratedPlacePromotionPanel();
    void drawFogAndLayerPanel();
    void drawTokenSetupPanel();
    void drawFeatureNoteSetupPanel();
    void drawRegionSetupPanel();
    void drawRouteSetupPanel();
    std::string localizedActiveSelectionSummary() const;
    std::string localizedSelectedObjectSummary() const;
    void finishUndoablePropertyEditIfIdle();
    bool undoableCheckbox(const char *label, bool &value, const char *undoLabel);
    bool undoableCombo(const char *label, int &value, const char *items, const char *undoLabel);
    bool undoableInputFloat(const char *label, float &value, const char *undoLabel);
    bool undoableInputInt(const char *label, int &value, const char *undoLabel);
    bool undoableInputText(const char *label, std::string &value, const char *undoLabel);
    bool undoableInputTextMultiline(const char *label, std::string &value, const ImVec2 &size, const char *undoLabel);
    bool undoableSliderFloat(const char *label, float &value, float minValue, float maxValue, const char *format,
                             const char *undoLabel);
};
