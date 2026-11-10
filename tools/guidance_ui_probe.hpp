#pragma once
#include "Controllers/Editor/Widgets/MapViewer/MapViewerController.hpp"
#include <sstream>
// Subpanel-only access avoids every canvas, texture, background task and audio
// path. Input actions still run the production view and controller methods.
struct UiGuidanceTestProbe
{
    static void marker(MapViewerController &c){c._view->drawMapModeControls(*c.getSelectedMap());}
    static void travel(MapViewerController &c){c._view->drawTravelWorkflowPanel(*c.getSelectedMap());}
    static void destination(MapViewerController &c,int x,int y){c.selectCell(x,y);c.previewPartyMoveToCell(x,y);}
    static std::string state(const MapViewerController &c){std::ostringstream s;
        s<<c._selectedMapIndex<<'/'<<c._selectedRouteIndex<<'/'<<c._selectedCellX<<'/'<<c._selectedCellY<<'/'
            <<c._pendingPartyMoveX<<'/'<<c._pendingPartyMoveY<<'/'<<c._hasPendingPartyMove<<'/'
            <<int(c._interactionMode)<<'/'<<c._zoom<<'/'<<c._fitZoomRequested<<'/'<<c._mapRenderContentVersion<<'/'
            <<c._view->_showDetails<<'/'<<c._view->_canvasFocus;return s.str();}
};
