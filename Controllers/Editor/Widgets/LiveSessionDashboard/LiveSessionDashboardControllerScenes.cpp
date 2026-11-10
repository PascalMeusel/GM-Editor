#include "Controllers/Editor/Widgets/LiveSessionDashboard/LiveSessionDashboardControllerSupport.hpp"

SessionPlan::Persist LiveSessionDashboardController::sessionPersist(const std::string &reason)
{
    return [reason](Campaign &campaign) { return ProjectSaveService::getInstance().saveCampaign(campaign, reason); };
}

bool LiveSessionDashboardController::acceptSessionResult(const SessionPlan::Result &result)
{
    if (result.applied()) { _sessionPlanMessage.clear(); return true; }
    const bool german = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
    if (!german) _sessionPlanMessage = result.message;
    else if (result.status == SessionPlan::Status::PersistenceFailed)
        _sessionPlanMessage = "Nicht gespeichert. Der bisherige Sitzungsstand bleibt erhalten. Versuche es erneut.";
    else if (result.status == SessionPlan::Status::NotFound)
        _sessionPlanMessage = "Der verknüpfte Inhalt fehlt. Prüfe den Sitzungsplan und repariere die Verknüpfung.";
    else if (result.status == SessionPlan::Status::Conflict)
        _sessionPlanMessage = "Diese Aktion passt nicht zum aktuellen Sitzungsstand. Prüfe die laufende Sitzung und archivierte Inhalte.";
    else _sessionPlanMessage = "Prüfe die Eingaben, bevor du fortfährst.";
    return false;
}

void LiveSessionDashboardController::launchSelectedScene()
{
    const auto *session = activeSession();
    const auto scenes = sceneOptions();
    if (!session || _selectedSceneIndex < 0 || _selectedSceneIndex >= static_cast<int>(scenes.size())) return;
    const EntityId sessionId = session->metadata.id;
    const EntityId sceneId = scenes[static_cast<std::size_t>(_selectedSceneIndex)]->metadata.id;
    EntityId entryId;
    for (const auto &entry : session->planEntries)
        if (entry.storyBeatId == sceneId && entry.state == SessionEntryState::Planned) { entryId = entry.metadata.id; break; }
    if (entryId.empty())
    {
        const auto added = SessionPlan::addScene(_currentCampaign, sessionId, sceneId, sessionPersist("add live scene to plan"));
        if (!acceptSessionResult(added)) return;
        entryId = added.entryId;
    }
    launchPlanEntry(entryId);
}

void LiveSessionDashboardController::launchPlanEntry(const EntityId &entryId)
{
    const auto *session = activeSession(); if (!session) return;
    const EntityId sessionId = session->metadata.id;
    const auto result = SessionPlan::activateEntry(_currentCampaign, sessionId, entryId, sessionPersist("activate planned scene"));
    if (!acceptSessionResult(result)) return;
    _selectedPlanEntryId = entryId;
    playCommittedScene(result.sceneId);
}

void LiveSessionDashboardController::playCommittedScene(const EntityId &sceneId)
{
    const auto *session = activeSession();
    const auto *entry = session ? SessionPlan::findEntry(*session, session->liveState.activePlanEntryId) : nullptr;
    if (!entry || entry->storyBeatId != sceneId) return;
    const auto *savedScene = SessionPlan::findScene(_currentCampaign, sceneId);
    if (!savedScene) return;
    const StoryBeatModel scene = *savedScene;
    std::vector<StoryLinkModel> links = scene.links;
    for (const auto &thread : _currentCampaign.storyThreads)
        for (const auto &beat : thread.beats)
            if (beat.metadata.id == sceneId) links = SceneLaunch::collectSceneLinks(thread, beat);
    int audioCount = 0;
    if (!scene.cueAudioAssetId.empty() && playAudioAsset(scene.cueAudioAssetId, scene.cueVolume, scene.cueLoop, "scene cue")) ++audioCount;
    audioCount += AudioTriggers::playMatchingTriggers(_currentCampaign, _playbackService, "StoryBeat", sceneId, "Scene Started", _audioErrorMessage);
    for (const auto &link : links)
        if (link.targetEntityType == "Audio" && playAudioAsset(link.targetEntityId, 1.0f, true, "linked audio")) ++audioCount;
    _sceneLaunchMessage = gm::localization::LocalizationService::instance().format(
        gm::localization::TextId::LiveSceneLaunched, {scene.title, std::to_string(audioCount)});
    // Remain in Live. Linked editors are available contextually instead of opening
    // several workspaces and displacing the session controls during scene launch.
}

void LiveSessionDashboardController::markPlanEntry(const EntityId &entryId, SessionEntryState state)
{
    const auto *session = activeSession(); if (!session) return;
    const auto *entry = SessionPlan::findEntry(*session, entryId); if (!entry) return;
    const EntityId sessionId = session->metadata.id;
    const std::string note = entry->prepNote;
    acceptSessionResult(SessionPlan::updateEntry(_currentCampaign, sessionId, entryId, note, state, sessionPersist("record planned scene progress")));
}
void LiveSessionDashboardController::openActiveEncounter()
{
    const EncounterModel *encounter = activeEncounter();
    const EntityId encounterId = encounter ? encounter->metadata.id : EntityId{};
    if (encounter)
    {
        if (const auto *session = activeSession())
        {
            const EntityId sessionId = session->metadata.id;
            if (!acceptSessionResult(SessionPlan::activateEncounter(_currentCampaign, sessionId, encounterId, sessionPersist("activate session encounter")))) return;
        }
    }
    if (_openEncounterHandler)
        _openEncounterHandler(encounterId);
}

void LiveSessionDashboardController::openEncounterPreparation()
{
    if (_openEncounterHandler)
        _openEncounterHandler(EntityId{});
}

void LiveSessionDashboardController::renderActiveCombatWorkspace()
{
    const EncounterModel *encounter = activeEncounter();
    if (encounter && _renderCombatHandler)
        _renderCombatHandler(encounter->metadata.id);
}
