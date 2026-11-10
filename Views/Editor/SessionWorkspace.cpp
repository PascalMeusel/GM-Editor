#include "Views/Editor/SessionWorkspaceSupport.hpp"
#include "Models/Project/CampaignEditHistory.hpp"

bool SessionWorkspace::report(const SessionPlan::Result &result)
{
    _error = result.status == SessionPlan::Status::PersistenceFailed || result.status == SessionPlan::Status::DraftConflict;
    _draftConflict = result.status == SessionPlan::Status::DraftConflict;
    _lastStatus = result.status;
    return result.applied();
}

const char *SessionWorkspace::statusText() const
{
    if (_lastStatus == SessionPlan::Status::DraftConflict)
        return st("This scene changed in another editor. Copy your draft, then reload the saved scene to resolve the conflict.", "Diese Szene wurde in einem anderen Editor geändert. Kopiere deinen Entwurf und lade die gespeicherte Szene neu.");
    if (_lastStatus == SessionPlan::Status::PersistenceFailed)
        return dirty()
            ? st("Not saved. Your draft is still here. Check the problem and retry.", "Nicht gespeichert. Dein Entwurf bleibt erhalten. Prüfe den Fehler und versuche es erneut.")
            : st("The action was not saved. Your content is intact. Repeat the action to try again.", "Die Aktion wurde nicht gespeichert. Deine Inhalte bleiben erhalten. Wiederhole die Aktion, um es erneut zu versuchen.");
    if (_lastStatus != SessionPlan::Status::Applied)
        return st("This action is unavailable. Check the selected session and scene.", "Diese Aktion ist nicht verfügbar. Prüfe die ausgewählte Sitzung und Szene.");
    return st("Saved", "Gespeichert");
}

void SessionWorkspace::changed(bool &flag) { flag = true; _error = false; _lastEdit = ImGui::GetTime(); }

void SessionWorkspace::load(Campaign &campaign, const EntityId &id, const EntityId &entryId)
{
    _selectionUnavailable = false;
    _sessionId = id;
    _entryId = entryId;
    _sceneId.clear();
    _scene = _sceneBase = {};
    _entryNote.clear();
    _selectedNotes.clear();
    _resetCompactScroll = true;
    _sessionDirty = _sceneDirty = _entryDirty = false;
    _sessionTextBase.clear();
    if (const auto *session = SessionPlan::findSession(campaign, id))
    {
        _title = session->title; _notes = session->roughNotes; _scheduled = session->scheduledAtUtc;
        if (_entryId.empty() && !session->planEntries.empty())
            _entryId = session->liveState.activePlanEntryId.empty() ? session->planEntries.front().metadata.id : session->liveState.activePlanEntryId;
        if (const auto *entry = SessionPlan::findEntry(*session, _entryId))
        {
            _entryNote = entry->prepNote;
            _sceneId = entry->storyBeatId;
            if (const auto *scene = SessionPlan::findScene(campaign, _sceneId)) _scene = _sceneBase = *scene;
        }
    }
    _showClues = !_scene.clues.empty();
    _showRoughNotes = _entryId.empty();
    if (_showRoughNotes) _compactPane = 1;
    _showConsequences = !_scene.consequences.empty();
    _showPlayerSummary = !_scene.playerSummary.empty();
    _showEntryNote = !_entryNote.empty();
    refreshRecoveryBase(campaign);
}

bool SessionWorkspace::flush(Campaign &campaign, const SessionPlan::Persist &persist)
{
    if (!_outcomes.checkpointDrafts(campaign)) return false;
    checkpointRecovery(campaign, true);
    if (!dirty()) return true;
    if (_campaignId != campaign.metadata.id || _sessionId.empty()) return false;
    if (_sessionDirty)
    {
        if (!report(SessionPlan::updateDraft(campaign, _sessionId, _title, _notes, _scheduled, persist))) return false;
        _sessionDirty = false;
    }
    if (_sceneDirty)
    {
        const auto *saved = SessionPlan::findScene(campaign, _sceneId);
        const auto before = saved ? saved->links : std::vector<StoryLinkModel>{};
        if (!report(SessionPlan::updateSceneDraft(campaign, _sceneId, _sceneBase, _scene, persist))) return false;
        _sceneDirty = false;
        if (const auto *scene = SessionPlan::findScene(campaign, _sceneId))
        {
            CampaignEditHistory::recordSceneLinks(campaign, _sceneId, before, scene->links, persist, "Edit scene links");
            _scene = _sceneBase = *scene;
        }
    }
    if (_entryDirty)
    {
        const auto *session = SessionPlan::findSession(campaign, _sessionId);
        const auto *entry = session ? SessionPlan::findEntry(*session, _entryId) : nullptr;
        if (!entry) return false;
        const auto state = entry->state;
        if (!report(CampaignEditHistory::editPlan(campaign, _sessionId, persist, "Edit session scene note", [&] {
            return SessionPlan::updateEntry(campaign, _sessionId, _entryId, _entryNote, state, persist);
        }))) return false;
        _entryDirty = false;
    }
    _recoveryReview = false;
    refreshRecoveryBase(campaign);
    checkpointRecovery(campaign, true);
    return true;
}

int SessionWorkspace::captureSelection(ImGuiInputTextCallbackData *data)
{
    auto &self = *static_cast<SessionWorkspace *>(data->UserData);
    const int begin = std::min(data->SelectionStart, data->SelectionEnd);
    const int end = std::max(data->SelectionStart, data->SelectionEnd);
    self._selectedNotes.assign(data->Buf + begin, static_cast<std::size_t>(end - begin));
    return 0;
}

bool SessionWorkspace::attachMap(Campaign &campaign, const EntityId &mapId, const SessionPlan::Persist &persist)
{
    if (_campaignId != campaign.metadata.id || _sceneId.empty() || !flush(campaign, persist)) return false;
    const auto *session = SessionPlan::findSession(campaign, _sessionId);
    const auto *scene = SessionPlan::findScene(campaign, _sceneId);
    const auto map = std::find_if(campaign.maps.begin(), campaign.maps.end(), [&](const auto &item) { return item.metadata.id == mapId; });
    if (!session || !session->endedAtUtc.empty() || !scene || map == campaign.maps.end()) return false;
    _scene = _sceneBase = *scene;
    StoryLinkModel link; link.targetEntityId = mapId; link.targetEntityType = "Map"; link.label = map->name;
    // The newly attached map becomes the first scene map used by Live; retain
    // any other map links as references rather than deleting campaign material.
    const auto old = std::find_if(_scene.links.begin(), _scene.links.end(), [&](const auto &item) { return item.targetEntityType == "Map" && item.targetEntityId == mapId; });
    if (old != _scene.links.end()) { link = *old; _scene.links.erase(old); }
    _scene.links.insert(_scene.links.begin(), std::move(link));
    changed(_sceneDirty);
    _compactPane = 2;
    _resetCompactScroll = true;
    return flush(campaign, persist);
}
