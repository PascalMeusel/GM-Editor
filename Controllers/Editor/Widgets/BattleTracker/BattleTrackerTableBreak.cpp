#include "BattleTrackerController.hpp"
#include "Models/Combat/EncounterDraftPersistence.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Session/SessionControlService.hpp"
#include "Models/Session/InWorldTimeService.hpp"
#include <algorithm>
#include <initializer_list>
#include <string_view>
#include <utility>
namespace {
std::string combatText(const char *english)
{
    return gm::localization::trUi(english);
}

std::string combatFormat(const char *englishTemplate, std::initializer_list<std::string_view> arguments)
{
    std::string result = combatText(englishTemplate);
    std::size_t index = 0;
    for (const std::string_view argument : arguments)
    {
        const std::string placeholder = "{" + std::to_string(index++) + "}";
        std::size_t offset = 0;
        while ((offset = result.find(placeholder, offset)) != std::string::npos)
        {
            result.replace(offset, placeholder.size(), argument.data(), argument.size());
            offset += argument.size();
        }
    }
    return result;
}

}


void BattleTrackerController::startCombatTableBreak(bool pauseAudio)
{
    changeCombatTableBreak(false, pauseAudio);
}

void BattleTrackerController::resumeCombatFromTableBreak(bool resumeAudio)
{
    changeCombatTableBreak(true, resumeAudio);
}

void BattleTrackerController::changeCombatTableBreak(bool resume, bool changeAudio)
{
    if (!canLeaveForPreparation() || _draftEncounter.participants.empty())
        return;
    const SessionModel *session = SessionControl::activeSession(_context.campaign);
    if (!session)
    {
        _combatErrorMessage = combatText(resume ? "Start a live session before resuming a table break from combat."
                                               : "Start a live session before starting a table break from combat.");
        return;
    }
    if (SessionControl::isBreakActive(*session) != resume)
    {
        _combatErrorMessage = combatText(resume ? "The live session is not in break state."
                                               : "The live session is already in break state.");
        return;
    }

    _combatErrorMessage.clear();
    _pendingTableBreakSessionId = session->metadata.id;
    _pendingTableBreakResume = resume;
    _pendingTableBreakDetail = combatText(resume ? "Combat resumed from Battle Tracker." : "Combat paused from Battle Tracker.");
    if (changeAudio)
        _pendingTableBreakDetail += combatText(resume ? " Audio resume requested." : " Audio pause requested.");
    _pendingTableBreakClock = InWorldTime::formatDateTime(_context.campaign.clock);
    setCombatPhase(resume ? CombatPhase::ActiveTurn : CombatPhase::PausedBreak);
    _freePlacementMode = false;
    CombatLogEntryModel entry;
    entry.round = std::max(1, _draftEncounter.round);
    entry.actorName = _draftEncounter.participants[std::clamp(_draftEncounter.activeParticipantIndex, 0,
        static_cast<int>(_draftEncounter.participants.size()) - 1)].displayName;
    entry.eventType = "Combat";
    entry.summary = combatText(resume
        ? (changeAudio ? "Combat resumed after table break; audio resume requested." : "Combat resumed after table break.")
        : (changeAudio ? "Combat paused for table break; audio pause requested." : "Combat paused for table break."));
    entry.metadata.touch();
    _draftEncounter.combatLog.push_back(std::move(entry));

    saveEncounter();
    if (_encounterSaveFailed)
        return;
    // Save/Retry never reaches this effect dispatch: it persists the retained
    // transition silently. Runtime effects follow only the successful action.
    AudioPlaybackBatchResult result;
    if (changeAudio && _context.playbackService)
        result = resume ? _context.playbackService->resumePaused(_combatErrorMessage)
                        : _context.playbackService->pausePlaying(_combatErrorMessage);
    _combatStatusMessage = combatText(resume ? "Session break ended and combat resumed." : "Combat paused and session break started.");
    if (changeAudio && result.changed > 0)
        _combatStatusMessage += combatFormat(resume ? " Audio channels resumed: {0}." : " Audio channels paused: {0}.",
                                             {std::to_string(result.changed)});
}

void BattleTrackerController::persistPendingTableBreak()
{
    const SessionModel *active = SessionControl::activeSession(_context.campaign);
    if (!active || active->metadata.id != _pendingTableBreakSessionId ||
        SessionControl::isBreakActive(*active) != *_pendingTableBreakResume)
    {
        _encounterSaveFailed = true;
        _combatErrorMessage = combatText("The live session changed while this combat transition was waiting to save. Your draft is retained.");
        return;
    }
    // Build from current data on each attempt so unrelated notes/library edits
    // are preserved. The shared Live phase is published only after disk success.
    Campaign candidate = _context.campaign;
    const bool changed = *_pendingTableBreakResume
        ? SessionControl::resumeFromBreak(candidate, _pendingTableBreakDetail, _pendingTableBreakClock)
        : SessionControl::startBreak(candidate, _pendingTableBreakDetail, _pendingTableBreakClock);
    if (!changed)
    {
        _encounterSaveFailed = true;
        _combatErrorMessage = combatText("Combat changes could not be saved. Your edits are retained. Retry saving.");
        return;
    }
    int selectedIndex = _selectedEncounterIndex;
    _encounterSaveFailed = !persistEncounterDraftSnapshot(candidate, _draftEncounter, selectedIndex,
        [](Campaign &campaign) { return ProjectSaveService::getInstance().saveCampaign(campaign, "combat table break"); });
    if (_encounterSaveFailed)
    {
        _combatErrorMessage = combatText("Combat changes could not be saved. Your edits are retained. Retry saving.");
        return;
    }
    _context.campaign = std::move(candidate);
    _selectedEncounterIndex = selectedIndex;
    _pendingTableBreakResume.reset();
    _pendingTableBreakSessionId.clear();
    _pendingTableBreakDetail.clear();
    _pendingTableBreakClock.clear();
    _combatErrorMessage.clear();
}

bool BattleTrackerController::hasActiveSession() const
{
    return SessionControl::activeSession(_context.campaign) != nullptr;
}

bool BattleTrackerController::isSessionBreakActive() const
{
    return SessionControl::isLiveSessionBreakActive(_context.campaign);
}

int BattleTrackerController::playingAudioChannelCount() const
{
    if (!_context.playbackService)
        return 0;

    int count = 0;
    for (const EntityId &assetId : _context.playbackService->activeAssetIds())
    {
        if (!_context.playbackService->isPaused(assetId))
            ++count;
    }
    return count;
}

int BattleTrackerController::pausedAudioChannelCount() const
{
    if (!_context.playbackService)
        return 0;

    int count = 0;
    for (const EntityId &assetId : _context.playbackService->activeAssetIds())
    {
        if (_context.playbackService->isPaused(assetId))
            ++count;
    }
    return count;
}

