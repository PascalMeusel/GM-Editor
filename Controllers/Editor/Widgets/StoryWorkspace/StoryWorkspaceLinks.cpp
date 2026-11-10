#include "StoryWorkspaceController.hpp"
#include "Models/Audio/AudioMixService.hpp"
#include "Models/Project/CampaignBookmarkService.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include "Models/Audio/AudioTriggerService.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Story/SceneLaunchService.hpp"
#include "Models/Session/SessionPlanService.hpp"
#include "Models/Session/SessionControlService.hpp"
#include <algorithm>
#include <cstddef>
#include <utility>

void StoryWorkspaceController::addThreadLink()
{
    StoryThreadModel *thread = getSelectedThread();
    if (!thread)
        return;

    const std::vector<StoryEntityOption> targets = getEntityOptions(_threadLinkTargetType);
    const auto selectedTarget = std::find_if(targets.begin(), targets.end(), [&](const StoryEntityOption &target) {
        return target.id == _threadLinkSelectedTargetId;
    });
    if (selectedTarget == targets.end())
        return;

    _draftThreadLink.targetEntityId = selectedTarget->id;
    _draftThreadLink.targetEntityType = selectedTarget->type;
    if (_draftThreadLink.label.empty())
        _draftThreadLink.label = selectedTarget->label;
    _draftThreadLink.metadata.touch();
    for (auto &candidate : _currentCampaign.storyThreads)
        if (std::any_of(candidate.links.begin(), candidate.links.end(), [&](const auto &link) { return link.metadata.id == _draftThreadLink.metadata.id; })) thread = &candidate;
    auto existing = std::find_if(thread->links.begin(), thread->links.end(), [&](const auto &link) { return link.metadata.id == _draftThreadLink.metadata.id; });
    if (existing == thread->links.end()) thread->links.push_back(_draftThreadLink); else *existing = _draftThreadLink;
    thread->metadata.touch();
    if (!saveCampaign()) return;
    _draftThreadLink = StoryLinkModel{};
    _threadLinkSelectedTargetId.clear();
    _threadLinkTargetIndex = -1;
}

void StoryWorkspaceController::addBeatLink()
{
    StoryBeatModel *beat = getSelectedBeat();
    if (!beat)
        return;

    const std::vector<StoryEntityOption> targets = getEntityOptions(_beatLinkTargetType);
    const auto selectedTarget = std::find_if(targets.begin(), targets.end(), [&](const StoryEntityOption &target) {
        return target.id == _beatLinkSelectedTargetId;
    });
    if (selectedTarget == targets.end())
        return;

    _draftBeatLink.targetEntityId = selectedTarget->id;
    _draftBeatLink.targetEntityType = selectedTarget->type;
    if (_draftBeatLink.label.empty())
        _draftBeatLink.label = selectedTarget->label;
    _draftBeatLink.metadata.touch();
    for (auto &thread : _currentCampaign.storyThreads)
        for (auto &candidate : thread.beats)
            if (std::any_of(candidate.links.begin(), candidate.links.end(), [&](const auto &link) { return link.metadata.id == _draftBeatLink.metadata.id; })) beat = &candidate;
    auto existing = std::find_if(beat->links.begin(), beat->links.end(), [&](const auto &link) { return link.metadata.id == _draftBeatLink.metadata.id; });
    if (existing == beat->links.end()) beat->links.push_back(_draftBeatLink); else *existing = _draftBeatLink;
    beat->metadata.touch();
    if (!saveCampaign()) return;
    _draftBeatLink = StoryLinkModel{};
    _beatLinkSelectedTargetId.clear();
    _beatLinkTargetIndex = -1;
}

void StoryWorkspaceController::deleteThreadLink(std::size_t index)
{
    StoryThreadModel *thread = getSelectedThread();
    if (!thread || index >= thread->links.size())
        return;
    thread->links.erase(thread->links.begin() + static_cast<std::ptrdiff_t>(index));
    thread->metadata.touch();
    saveCampaign();
}

void StoryWorkspaceController::deleteBeatLink(std::size_t index)
{
    StoryBeatModel *beat = getSelectedBeat();
    if (!beat || index >= beat->links.size())
        return;
    beat->links.erase(beat->links.begin() + static_cast<std::ptrdiff_t>(index));
    beat->metadata.touch();
    saveCampaign();
}

void StoryWorkspaceController::moveBeat(int fromIndex, int offset)
{
    StoryThreadModel *thread = getSelectedThread();
    if (!thread)
        return;
    const int toIndex = fromIndex + offset;
    if (fromIndex < 0 || toIndex < 0 || fromIndex >= static_cast<int>(thread->beats.size()) || toIndex >= static_cast<int>(thread->beats.size()))
        return;
    std::swap(thread->beats[fromIndex], thread->beats[toIndex]);
    _selectedBeatIndex = toIndex;
    thread->metadata.touch();
    saveCampaign();
}

void StoryWorkspaceController::startSelectedBeatCue()
{
    const auto *selected = getSelectedBeat(); if (!selected) return;
    const EntityId sceneId = selected->metadata.id;
    _errorMessage.clear(); _statusMessage.clear();
    const bool de = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
    const SessionPlan::Persist persist = [](Campaign &value) { return ProjectSaveService::getInstance().saveCampaign(value, "stage story scene for session"); };
    const auto accept = [&](const SessionPlan::Result &result) {
        if (result.applied()) return true;
        _errorMessage = de ? "Die Aktion wurde nicht gespeichert. Prüfe den Sitzungsstand und versuche es erneut." : result.message;
        return false;
    };
    const auto *next = SessionPlan::nextSession(_currentCampaign);
    EntityId sessionId;
    if (next) sessionId = next->metadata.id;
    else
    {
        const auto created = SessionPlan::createSession(_currentCampaign, SessionControl::defaultSessionTitle(_currentCampaign), persist);
        if (!accept(created)) return;
        sessionId = created.sessionId;
    }
    const auto *session = SessionPlan::findSession(_currentCampaign, sessionId);
    EntityId entryId;
    for (const auto &entry : session->planEntries)
        if (entry.storyBeatId == sceneId) { entryId = entry.metadata.id; break; }
    if (entryId.empty())
    {
        const auto added = SessionPlan::addScene(_currentCampaign, sessionId, sceneId, persist);
        if (!accept(added)) return;
        entryId = added.entryId;
    }
    if (!SessionPlan::findSession(_currentCampaign, sessionId)->liveModeActive)
    {
        _statusMessage = de ? "Zum Sitzungsplan hinzugefügt. Beginne die vorbereitete Sitzung in Live." : "Added to the session plan. Start the prepared session in Live.";
        return;
    }
    if (!accept(SessionPlan::activateEntry(_currentCampaign, sessionId, entryId, persist))) return;
    const StoryBeatModel beat = *SessionPlan::findScene(_currentCampaign, sceneId);
    _statusMessage = std::string(gm::localization::trUi("Launched scene:")) + " " + beat.title;
    // Start external effects only after the new scene and cursor are committed.
    if (!beat.cueAudioAssetId.empty())
    {
        AudioAssetModel *asset = findAudioAsset(beat.cueAudioAssetId);
        if (!asset) _errorMessage = gm::localization::trUi("The selected music cue no longer exists in the audio library.");
        else if (!_playbackService) _errorMessage = gm::localization::trUi("Audio playback is not available. Scene status was updated, but the cue did not start.");
        else _playbackService->play(*asset, ProjectAssets::resolveAudioPath(*asset), AudioMix::effectiveVolume(_currentCampaign, *asset, beat.cueVolume), beat.cueLoop, _errorMessage);
    }
    AudioTriggers::playMatchingTriggers(_currentCampaign, _playbackService, "StoryBeat", sceneId, "Scene Started", _errorMessage);
}
void StoryWorkspaceController::stopSelectedBeatCue()
{
    StoryBeatModel *beat = getSelectedBeat();
    if (!beat || beat->cueAudioAssetId.empty() || !_playbackService)
        return;

    _errorMessage.clear();
    AudioAssetModel *asset = findAudioAsset(beat->cueAudioAssetId);
    if (!asset)
        return;

    if (_playbackService->stop(asset->metadata.id, _errorMessage))
        _statusMessage = std::string(gm::localization::trUi("Stopped cue:")) + " " + asset->name;
}

bool StoryWorkspaceController::saveCampaign()
{
    _currentCampaign.metadata.touch();
    auto &service = ProjectSaveService::getInstance();
    _saveFailed = !service.saveCampaign(_currentCampaign, "save story editing");
    if (_saveFailed)
    {
        _statusMessage.clear();
        _errorMessage = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
            ? "Nicht gespeichert. Deine Änderungen bleiben hier. Bitte erneut speichern. "
            : "Not saved. Your changes remain here. Retry saving. ";
        _errorMessage += service.state().lastError;
    }
    else _errorMessage.clear();
    return !_saveFailed;
}

void StoryWorkspaceController::onShow()
{
    _view->show();
    _view->requestDockOnNextUpdate();
}
