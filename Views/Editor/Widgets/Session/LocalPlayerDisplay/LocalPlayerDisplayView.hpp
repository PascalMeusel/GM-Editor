#pragma once

#include "imgui.h"

class LocalPlayerDisplayController;
struct EncounterModel;
struct MapModel;
struct MapRenderPlan;

class LocalPlayerDisplayView
{
public:
    explicit LocalPlayerDisplayView(LocalPlayerDisplayController &controller);

    void update();
    void show();
    void hide();
    bool isVisible() const;
    void requestPlacement();

private:
    LocalPlayerDisplayController &_controller;
    bool _isVisible = false;
    bool _placementRequested = true;
    int _lastResolvedMonitorIndex = -2;
    bool _lastFullscreen = false;
    bool _lastFallback = true;

    void draw();
    void drawBlackoutSurface();
    void drawScenePanel();
    void drawMapPanel(const MapModel *map, const MapRenderPlan *plan, bool mapOnly = false,
                      bool fillAvailableHeight = false);
    void drawInitiativePanel();
    void drawPartyPanel();
    void drawInventoryPanel();
    void drawVisibleNpcsPanel();
    void drawHandoutsPanel();
    void drawStoryTrackingPanel();
    void drawSafetySummaryPanel();
    void drawSharedArtPanel(const MapModel *map, const MapRenderPlan *plan);
};
