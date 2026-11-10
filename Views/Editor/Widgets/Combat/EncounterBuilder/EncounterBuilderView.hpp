#pragma once

#include "Views/Editor/Widgets/DockableWidget.hpp"
#include "Models/Map/MapRenderPlanCache.hpp"

class EncounterBuilderController;

class EncounterBuilderView : public DockableWidget
{
public:
    explicit EncounterBuilderView(EncounterBuilderController &controller);

protected:
    void draw() override;

private:
    EncounterBuilderController &_controller;
    MapRenderPlanCache _previewPlans{8};
    bool _showTacticalOptions = false;

    void drawEncounterList();
    void drawEncounterDetails();
    void drawBattlefield();
    void drawDifficultyPreview();
    void drawParticipantTools();
    void drawParticipantTable();
};
