#include "Controllers/Editor/EditorWindowControllerSupport.hpp"
#include "Models/Project/ApplicationPaths.hpp"

namespace
{
bool availableSource(const Campaign &campaign, const EntityId &id)
{
    const SessionModel *source = nullptr;
    for (const auto &session : campaign.sessions)
        if (session.metadata.id == id) { if (source || id.empty()) return false; source = &session; }
    return source && std::find(source->metadata.tags.begin(), source->metadata.tags.end(), "archived") == source->metadata.tags.end();
}
}

void EditorWindowController::queuePreparationTarget(PreparationTargetRequest request)
{
    // A second explicit click supersedes a queued click; neither performs work
    // while its source panel still owns references into the campaign.
    _workspace.preparationTarget = EditorPreparationTargetEnvelope{
        std::move(request), &_currentCampaign, ApplicationPaths::writableRoot(),
        _workspace.preparationEpoch, view->_activeWorkspace};
}

void EditorWindowController::revokePreparationTarget()
{
    ++_workspace.preparationEpoch;
    _workspace.preparationTarget.reset();
    view->_sessionWorkspace.clearPreparationTarget();
    if (_children.liveSessionDashboard) _children.liveSessionDashboard->clearPreparationTarget();
}

void EditorWindowController::processPreparationTarget()
{
    if (!_workspace.preparationTarget) return;
    const auto pending = std::move(*_workspace.preparationTarget);
    _workspace.preparationTarget.reset(); // Consume once, including refusal.
    const auto &request = pending.request;
    const bool preparation = request.sourceSurface == SessionOutcomeDraftKeys::Surface::Preparation;
    const auto matches = [&] {
        return preparation ? view->_sessionWorkspace.matchesPreparationTarget(request)
            : _children.liveSessionDashboard->matchesPreparationTarget(request);
    };
    const auto contextCurrent = [&] {
        return pending.campaign == &_currentCampaign && pending.epoch == _workspace.preparationEpoch &&
            pending.root == ApplicationPaths::writableRoot() && view->isVisible &&
            !_currentCampaign.presentationModeActive && request.campaignId == _currentCampaign.metadata.id &&
            pending.workspace == view->_activeWorkspace &&
            (preparation || request.sourceSurface == SessionOutcomeDraftKeys::Surface::Live) &&
            (!preparation || view->_sessionWorkspace.sessionId() == request.sourceSessionId) &&
            !request.carryRequestId.empty() && !request.targetSessionId.empty() &&
            request.sourceSessionId != request.targetSessionId &&
            availableSource(_currentCampaign, request.sourceSessionId) && matches();
    };
    const auto report = [&](PreparationTargetResult result) {
        // Feedback cannot attach to another source's last successful carry.
        if (!matches()) return;
        if (preparation) view->_sessionWorkspace.reportPreparationTarget(request, result);
        else _children.liveSessionDashboard->reportPreparationTarget(request, result);
        _shortcuts.statusMessage = result.message; // Generic; no private title/path.
    };
    if (!contextCurrent())
    {
        report({PreparationTargetStatus::StaleContext, uiText(
            "The session context changed. Review the carry destination again before opening Prepare.",
            "Der Sitzungskontext wurde geändert. Prüfe das Übernahmeziel erneut, bevor du die Vorbereitung öffnest.")});
        return;
    }
    auto result = view->_sessionWorkspace.inspectPreparationTarget(_currentCampaign, request.targetSessionId);
    if (!result.opened()) { report(std::move(result)); return; }
    // Recovery checkpoints are not canonical saves. Any refusal leaves the
    // source workspace, selection, history and all preparation drafts intact.
    if (!checkpointEditorDraftRecoveryForLeave() || !checkpointWorldRecoveryForLeave() ||
        !checkpointOutcomeRecoveryForLeave() || !_children.liveSessionDashboard->checkpointPreparationNavigation())
    {
        report({PreparationTargetStatus::CheckpointFailed, uiText(
            "Unfinished text could not be checkpointed. Your drafts remain open. Review the recovery error and retry Prepare this session.",
            "Unfertiger Text konnte nicht gesichert werden. Deine Entwürfe bleiben geöffnet. Prüfe den Sicherungsfehler und wähle erneut Diese Sitzung vorbereiten.")});
        return;
    }
    if (!contextCurrent())
    {
        report({PreparationTargetStatus::StaleContext, uiText(
            "The session context changed during navigation. Review the destination again.",
            "Der Sitzungskontext wurde während der Navigation geändert. Prüfe das Ziel erneut.")});
        return;
    }
    result = view->_sessionWorkspace.inspectPreparationTarget(_currentCampaign, request.targetSessionId);
    if (!result.opened()) { report(std::move(result)); return; }

    // All guards have completed. This tail has no persistence or callbacks;
    // normal activateWorkspace/restoreSelection may flush drafts, so this
    // exact handoff deliberately uses the already-checked navigation path.
    captureWorkspaceLocation();
    result = view->_sessionWorkspace.openPreparationTarget(_currentCampaign, request.targetSessionId);
    if (!result.opened()) { report(std::move(result)); return; }
    const bool changingWorkspace = view->_activeWorkspace != EditorWorkspace::Home;
    _navigation.navigate(EditorWorkspace::Home, changingWorkspace, {});
    _children.hideWorkspaceTools();
    view->_activeWorkspace = EditorWorkspace::Home;
    view->_returnToLiveContext = false;
    view->_campaignNavigationOpen = false;
    view->_presentationPanelOpen = false;
    restoreWorkspaceLocation();
    auto &location = _navigation.current();
    location.editorRoute.clear(); location.targetUnavailable = false;
    location.sessionId = view->_sessionWorkspace.sessionId();
    location.planEntryId = view->_sessionWorkspace.planEntryId();
    location.sceneId = view->_sessionWorkspace.sceneId(); location.sessionUnavailable = false;
    view->_activeSidebarWorkflowLabel = editorWorkspaceDefinition(EditorWorkspace::Home).label;
    view->_focusCampaignHomeNextFrame = true;
    ImGui::SetWindowFocus("Campaign Home");
    report({PreparationTargetStatus::Opened, uiText(
        "Opened the carried-to session in Prepare. No session was started and no draft was committed.",
        "Die Zielsitzung ist in der Vorbereitung geöffnet. Es wurde keine Sitzung gestartet und kein Entwurf übernommen.")});
}

void EditorWindowController::showLiveSessionRunTab()
{
    if (!view->activateWorkspace(EditorWorkspace::Live)) return;
    view->_liveUtilityRailExplicitlyOpened = true;
    _children.liveSessionDashboard->requestRunSessionTab();
}
