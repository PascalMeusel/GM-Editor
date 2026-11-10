#pragma once

#include "Models/Map/MapModels.hpp"
#include "Models/Map/MapRenderPlanCache.hpp"
#include "Views/Editor/Widgets/DockableWidget.hpp"

class MapViewerController;
namespace gm::map_viewer_detail
{
struct CanvasFrame;
}

class MapViewerView : public DockableWidget
{
public:
    explicit MapViewerView(MapViewerController &controller);
    void requestTravelWorkflow() { _openTravelWorkflowRequested = true; }

protected:
    void draw() override;

private:
    friend struct UiGuidanceTestProbe;
    MapViewerController &_controller;
    MapRenderPlanCache _renderPlanCache{24};
    int _expandEastCells = 16;
    int _expandSouthCells = 0;
    int _existingMapToPlaceIndex = 0;
    EntityId _pendingExistingMapId;
    bool _showDetails = false;
    bool _openTravelWorkflowRequested = false;
    bool _canvasFocus = false;
    EntityId _canvasPreferenceCampaign;
    bool _openDetailDesignReview = false;
    std::uint64_t _reviewedDetailRequestSerial = 0;
    int _enlargedDetailDesign = -1;
    bool _detailDesignCreateFailed = false;

    void drawAtlasHeader(const MapModel &map);
    void drawAtlasSidePanel(MapModel &map);
    void drawDetailDesignReview();
    void drawWorldExpansionPanel(const MapModel &map);
    void drawExistingMapPlacementPanel(const MapModel &map);
    void drawMapModeControls(const MapModel &map);
    void drawTravelStatus(const MapModel &map);
    void drawMapCanvas();
    void drawMapCanvasCells(const gm::map_viewer_detail::CanvasFrame &frame);
    void drawMapCanvasSemantics(const gm::map_viewer_detail::CanvasFrame &frame);
    void handleMapCanvasInput(const gm::map_viewer_detail::CanvasFrame &frame);
    void drawMapSidePanel(MapModel &map);
    bool drawTravelWorkflowPanel(MapModel &map);
};
