#pragma once

#include "Views/Editor/Widgets/DockableWidget.hpp"
#include "Views/Editor/SessionOutcomesPanel.hpp"
#include "Models/Session/LiveOutcomeSessionSelection.hpp"
#include <string>

class LiveSessionDashboardController;

enum class LiveSessionDashboardPhase
{
    Prep,
    RunningScene,
    ExplorationTravel,
    Combat,
    Break,
    WrapUp
};

class LiveSessionDashboardView : public DockableWidget
{
public:
    explicit LiveSessionDashboardView(LiveSessionDashboardController &controller);
    void drawEmbedded();
    void requestRunSessionTab() { _openRunSessionTab = true; _openTableToolsTab = false; }
    void requestTableToolsTab() { _openTableToolsTab = true; _openRunSessionTab = false; }
    void clearTabRequests() { _openRunSessionTab = _openTableToolsTab = false; }
    bool matchesPreparationTarget(const PreparationTargetRequest &request) const { return _outcomes.matchesPreparationTarget(request); }
    void reportPreparationTarget(const PreparationTargetRequest &request, const PreparationTargetResult &result)
    { _outcomes.reportPreparationTarget(request, result); }
    void clearPreparationTarget() { _outcomes.clearPreparationTarget(); }
    bool checkpointOutcomeDrafts(Campaign &campaign) { return _outcomes.checkpointDrafts(campaign); }
    bool hasPendingOutcomeDraftWrites() const { return _outcomes.hasPendingDraftWrites(); }
    void resetOutcomeDrafts()
    { _outcomes = SessionOutcomesPanel(SessionOutcomeDraftKeys::Surface::Live); _outcomeSelection = {}; _outcomesTabWasOpen = false; clearTabRequests(); }

protected:
    void draw() override;

private:
    friend struct SessionTargetProbe;
    SessionOutcomesPanel _outcomes{SessionOutcomeDraftKeys::Surface::Live};
    LiveOutcomeSessionSelection _outcomeSelection;
    bool _outcomesTabWasOpen = false;
    void drawOutcomesWorkspace();
    bool drawTextRecovery();
    LiveSessionDashboardController &_controller;
    bool _openTableToolsTab = false;
    bool _openRunSessionTab = false;
    std::string _pendingSessionNoteRemovalId;
    int _pendingTimelineEventRemovalIndex = -1;

    void drawHeader();
    void drawSessionStatusStrip();
    void drawLiveCoreSummary();
    void drawPhaseSelector();
    void drawPhaseWorkspace(LiveSessionDashboardPhase phase);
    void drawPhaseAdvancedSections(LiveSessionDashboardPhase currentPhase);
    void drawPhaseControls(LiveSessionDashboardPhase phase);
    void drawSessionSupport();
    void drawGmOnlyNotice(const char *sectionLabel);
    void drawGmSessionNotes();
    void drawGmPrepWrapTools();
    void drawSessionLifecycleControls();
    void drawClockCalendarControls();
    void drawTimelineControls();
    void drawQuickRolls();
    void drawPartyState();
    void drawSceneLauncher();
    void drawSceneMapEncounter();
    void drawCombatCommandCenter();
    void drawEffectsWatch();
    void drawAudioControls();
    void drawSessionNotes();
    void drawSessionTemplates();
    void drawSessionRecap();
    void drawCompletedRecapEntry();
    bool drawCompletedRecap();
    void drawPostSessionReport();
    void drawClockAndRolls();
};
