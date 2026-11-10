#include "MapCreatorView.hpp"

void MapCreatorView::resetForCampaign()
{
    // These workers capture values, not the mutable campaign. Revoke results
    // and retain their future/worker storage so replacement never joins them.
    if (_setupPreviewCancel) _setupPreviewCancel->store(true);
    _discardSetupPreviewResult = _setupPreviewJob.valid();
    _discardInspectionResult = _mapEditInspection.valid();
    _setupPreviewCancel.reset();
    _setupPreviewMap.reset();
    _setupPreviewObservedKey.clear();
    _setupPreviewPendingKey.clear();
    _setupPreviewReadyKey.clear();
    _setupPreviewFailedKey.clear();
    _setupPreviewChangedAt = 0;
    ++_setupPreviewRevision;
    _canvasPreviewCompiler.clear();
    _renderPlanCache.clear();
    _playerPreviewCache = {};
    _nextCanvasPreviewRetry = 0;
    _mapEditConflicts.clear();
    _mapEditInspectionError.clear();
    _mapEditInspectionRetryAt = 0;
    _mapEditConflictsMapId.clear();
    _mapEditConflictsSerial = 0;
    _mapEditConflictsReady = false;
    _mapEditConflictMessage.clear();

    _workflowStep = 0;
    _resetWorkflowScroll = true;
    _loadMapReviewRequested = _newMapReviewRequested = false;
    _openPartialRegenerationConfirmation = _openCheckpointRestoreConfirmation = false;
    _newMapIntent = MapCreationIntent::WorldOverview;
    _advancedCreation = _keepGenerationSeed = false;
    _dimensionDraftMapId.clear();
    _pendingGeneratedFeatureProjectionId.clear();
    _pendingGeneratedFeatureOriginal = {};
    _hasPendingGeneratedFeatureProjection = false;
    _selectedGeneratedFeaturePoint = _draggedGeneratedFeaturePoint = -1;
    _draggingWholeObject = false;
    _objectDragStart = {};
    _generatedFeatureGeometryEditId.clear();
    _generatedFeatureGeometryBeforeEdit = {};
    _generatedFeatureGeometryEditActive = _geometryPreviewPending = false;
    _generatedFeatureGeometryRenderSerial = 0;
    _terrainSelection = {};
    _terrainSelectionSerial = 0;
    _terrainDragArmed = _terrainDragging = false;
    _terrainDragStart = {};
    _terrainDeltaX = _terrainDeltaY = 0;
    _tokenDragArmed = false;
    _draggedTokenIndex = -1;
    _tokenDragStart = {};
    _tokenDragStartX = _tokenDragStartY = 0;
    _boxSelectMode = _boxArmed = _boxDragging = false;
    _regionHandle = 0;
    _regionDragIndex = -1;
    _regionGestureTool = 0;
    _regionDragging = false;
    _regionDragStart = {};
    _regionBefore = _regionPreview = {};
    _regionGestureMapId.clear();
    _regionGestureId.clear();
    _regionGestureSerial = 0;
    _boxStart = _boxEnd = {};
    _areaCandidates.clear();
    _areaSelectionSerial = 0;
    _areaFilter = 0;
    _areaSearch[0] = '\0';
    _interactionCanvasOrigin = {};
    _interactionCanvasCellSize = 0;
    _areaDragArmed = _areaDragging = false;
    _areaDragStart = {};
    _areaDeltaX = _areaDeltaY = 0;
    _previewedGenerationCandidate = 0;
    _designPreviewZoom = _playerPreviewZoom = 1;
    _resetDesignPreviewScroll = true;
    _lastInteractivePreviewCandidate = -1;
    _fitCanvasOnNextDraw = true;
    _fitCanvasDelayFrames = 1;
    _showWorldPoliticalOverlay = _showCompactInspector = false;
    _pendingCanvasInspectorReveal = _drawingCompactInspector = false;
    _resetInspectorScroll = true;
    _inspectorContext.clear();
    _toolCategory = _sidebarPage = 0;
    _lastSelectionSummary.clear();
    _regenerationUseArea = _regenerationPickingArea = _regenerationAreaDragging = false;
    _regenerationAreaStartX = _regenerationAreaStartY = _regenerationAreaEndX = _regenerationAreaEndY = 0;
    _imageTraceActive = false;
    // Presentation preferences may persist, but a new trace starts with the
    // fresh editor's private-by-default visibility and authoring settings.
    _imageTraceModeIndex = 0;
    _imageTraceSnap = _imageTraceSmooth = _imageTraceGmOnly = true;
    _imageTraceControlPoints.clear();
    _imageTraceName.clear();
    _imageTraceDraggedPoint = -1;
    hide();
}

void MapCreatorView::pollRetiredCampaignWork()
{
    if (_discardSetupPreviewResult && _setupPreviewJob.valid() &&
        _setupPreviewJob.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
        try { (void)_setupPreviewJob.get(); } catch (...) {}
        _discardSetupPreviewResult = false;
    }
    if (_discardInspectionResult && _mapEditInspection.valid() &&
        _mapEditInspection.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
        try { (void)_mapEditInspection.get(); } catch (...) {}
        _discardInspectionResult = false;
    }
}
