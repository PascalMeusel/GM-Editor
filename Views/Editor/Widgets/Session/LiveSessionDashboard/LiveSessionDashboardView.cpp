#include "Views/Editor/Widgets/Session/LiveSessionDashboard/LiveSessionDashboardViewSupport.hpp"
#include "Views/Editor/Widgets/UiGuidance.hpp"

LiveSessionDashboardView::LiveSessionDashboardView(LiveSessionDashboardController &controller)
    : DockableWidget("Live Session Dashboard", 0, ImVec2(1180.0f, 740.0f)), _controller(controller)
{
}

void LiveSessionDashboardView::draw()
{
    _outcomes.tickDrafts(_controller._currentCampaign);
    if (drawTextRecovery()) return;
    drawHeader();
    drawSessionStatusStrip();
    drawLiveCoreSummary();
    drawPhaseSelector();
    drawPhaseWorkspace(_controller.dashboardPhase());
    if (const auto *session = _controller.activeSession())
        _outcomes.drawLauncher(_controller._currentCampaign, session->metadata.id,
            _controller.sessionPersist("session outcomes"), _controller.activeStoryBeat() ? _controller.activeStoryBeat()->metadata.id : EntityId{},
            _controller._preparationTargetHandler);
    else _outcomes.clearPreparationTarget();
    _controller._tabletopToolkit->drawEmbedded();
    drawPhaseAdvancedSections(_controller.dashboardPhase());
}

void LiveSessionDashboardView::drawEmbedded()
{
    _outcomes.tickDrafts(_controller._currentCampaign);
    const auto *active = _controller.activeSession();
    if (_outcomeSelection.synchronize(_controller._currentCampaign, active ? active->metadata.id : EntityId{}))
        _outcomes.clearPreparationTarget();
    if (drawTextRecovery()) return;
    const bool de = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
    // The workspace header already reports retained drafts. Adding a banner
    // here on the first keystroke moves the focused form under the pointer.
    bool outcomesTabOpen = false;
    if (ImGui::BeginTabBar("LiveSessionWorkspaceTools"))
    {
        if (ImGui::BeginTabItem(de ? "Sitzung leiten" : "Run session", nullptr,
                               _openRunSessionTab ? ImGuiTabItemFlags_SetSelected : 0))
        {
            _openRunSessionTab = false;
            if (ImGui::CollapsingHeader(de ? "Sitzungsübersicht" : "Session overview"))
                drawLiveCoreSummary();
            ImGui::TextWrapped("%s", phaseHelp(_controller.dashboardPhase()));
            UiGuidance::drawDetails(UiGuidance::Topic::SessionPhases);
            drawPhaseControls(_controller.dashboardPhase());
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(de ? "Ergebnisse und Abschluss" : "Outcomes and wrap-up"))
        {
            outcomesTabOpen = true;
            ImGui::BeginChild("SessionOutcomesContent", ImVec2(0, 0));
            drawOutcomesWorkspace();
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(de ? "Würfel, Ziele und Hinweise" : "Dice, objectives and clues", nullptr,
                                _openTableToolsTab ? ImGuiTabItemFlags_SetSelected : 0))
        {
            _openTableToolsTab = false;
            // Workspace embedding has no outer content scroll. Keep every table
            // tool reachable after the compact header and its status message
            // consume the available height; standalone Live keeps its own window
            // scroll behavior in draw().
            ImGui::BeginChild("TabletopToolkitEmbeddedContent", ImVec2(0.0f, 0.0f), false,
                              ImGuiWindowFlags_HorizontalScrollbar);
            _controller._tabletopToolkit->drawEmbedded();
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    if (_outcomesTabWasOpen && !outcomesTabOpen) _outcomes.checkpointDrafts(_controller._currentCampaign);
    _outcomesTabWasOpen = outcomesTabOpen;
}

void LiveSessionDashboardView::drawHeader()
{
    const SessionModel *session = _controller.activeSession();
    bool presentationMode = _controller._currentCampaign.presentationModeActive;
    if (FormUi::Checkbox(gm::localization::tr(gm::localization::TextId::LivePlayerDisplayMode), &presentationMode))
        _controller.setPresentationModeActive(presentationMode);
    ImGui::SameLine();
    ImGui::TextDisabled(localizedUiLabel("%s").c_str(), presentationMode ? gm::localization::tr(gm::localization::TextId::LiveGmNotesHidden)
                                               : gm::localization::tr(gm::localization::TextId::LiveGmView));
    if (session)
    {
        ImGui::TextWrapped(localizedUiLabel("Live: %s").c_str(), session->title.c_str());
        const std::string started = gm::localization::LocalizationService::instance().format(
            gm::localization::TextId::LiveStartedAt,
            {session->startedAtUtc.empty() ? std::string_view{"-"} : std::string_view{session->startedAtUtc}});
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), started.c_str());
    }
    else
    {
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), gm::localization::tr(gm::localization::TextId::LiveNoLiveActive));
    }
    const LocalPlayerDisplayModel &display = _controller._currentCampaign.localPlayerDisplay;
    const char *presentationStatus = display.enabled ? gm::localization::tr(gm::localization::TextId::LiveDisplayPublic)
                                                     : (presentationMode ? gm::localization::tr(gm::localization::TextId::LivePresentationPreview)
                                                                         : gm::localization::tr(gm::localization::TextId::LiveGmControlsVisible));
    drawInlineLabelValue(gm::localization::tr(gm::localization::TextId::LivePresentation), presentationStatus);
    ImGui::Separator();
}

void LiveSessionDashboardView::drawSessionStatusStrip()
{
    const SessionModel *session = _controller.activeSession();
    const StoryThreadModel *thread = _controller.activeStoryThread();
    const StoryBeatModel *beat = _controller.activeStoryBeat();
    const MapModel *map = _controller.activeMap();
    const EncounterModel *encounter = _controller.activeEncounter();
    const bool breakActive = _controller.isSessionBreakActive();

    const LiveSessionDashboardPhase selectedPhase = _controller.dashboardPhase();
    const LiveSessionDashboardPhase inferredPhase = _controller.inferredDashboardPhase();
    std::string mode = phaseLabel(selectedPhase);
    if (selectedPhase != inferredPhase)
        mode = gm::localization::LocalizationService::instance().format(gm::localization::TextId::LiveManualFocus, {mode});
    const std::string scene = sceneSelectionLabel(thread, beat);
    const std::string mapLabel = mapSelectionLabel(map);
    const std::string encounterLabel = encounterSelectionLabel(encounter);
    const std::string selection = gm::localization::LocalizationService::instance().format(
        gm::localization::TextId::LiveSelectionSummary,
        {scene, mapLabel, encounterLabel});
    const std::string nextAction = phaseNextAction(selectedPhase, session, encounter, breakActive, !_controller.sceneOptions().empty());

    UiStatusActionStrip strip;
    strip.mode = mode.c_str();
    strip.selection = selection.c_str();
    strip.nextAction = nextAction.c_str();
    strip.help = "Shows the dashboard phase, the current scene/map/encounter, and the safest next GM action. Other phases stay available below.";
    drawStatusActionStrip("LiveSessionDashboardStatusStrip", strip);
}
