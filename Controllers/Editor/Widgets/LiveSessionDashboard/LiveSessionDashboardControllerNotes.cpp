#include "Controllers/Editor/Widgets/LiveSessionDashboard/LiveSessionDashboardControllerSupport.hpp"

void LiveSessionDashboardController::saveSessionSummary()
{
    flushSessionDrafts();
}

std::vector<EntityRegistryEntry> LiveSessionDashboardController::noteLinkCandidates() const
{
    std::vector<EntityRegistryEntry> entries = EntityRegistry::getInstance().entries();
    entries.erase(std::remove_if(entries.begin(), entries.end(), [](const EntityRegistryEntry &entry) {
                      return entry.type == "campaign" || entry.type == "session_note" || entry.displayName.empty();
                  }),
                  entries.end());
    return entries;
}

void LiveSessionDashboardController::addSessionNote()
{
    if (!_sessionNoteDraft.empty()) flushSessionDrafts();
}

void LiveSessionDashboardController::removeSessionNote(const EntityId &noteId)
{
    const auto *session = activeSession(); if (!session) return;
    const EntityId id = session->metadata.id;
    acceptSessionResult(SessionPlan::removeNote(_currentCampaign, id, noteId, sessionPersist("remove session note")));
}

void LiveSessionDashboardController::saveSessionRecap()
{
    flushSessionDrafts();
}

std::vector<std::string> LiveSessionDashboardController::sessionTemplateLabels() const
{
    using gm::localization::TextId;
    return {gm::localization::tr(TextId::LiveTemplatePrepChecklist),
            gm::localization::tr(TextId::LiveRecap),
            gm::localization::tr(TextId::LiveTemplateScenes),
            gm::localization::tr(TextId::LiveTemplateEncounters),
            gm::localization::tr(TextId::LiveTemplateTreasure),
            gm::localization::tr(TextId::LiveTemplateFollowUps)};
}

void LiveSessionDashboardController::applySessionTemplate(int templateIndex)
{
    SessionModel *session = activeSession();
    if (!session)
        return;

    switch (templateIndex)
    {
    case 0:
        _sessionSummaryDraft = gm::localization::trUi("Prep Checklist\n- Review previous recap\n- Confirm active hooks\n- Prepare opening scene\n- Check maps and encounters\n- Queue audio\n- Note player spotlight moments");
        break;
    case 1:
        _recapDraft = gm::localization::trUi("Opening situation:\n\nMajor events:\n\nConsequences:\n");
        _unresolvedThreadsDraft = gm::localization::trUi("Unresolved threads:\n- ");
        break;
    case 2:
        _sessionSummaryDraft = gm::localization::trUi("Scenes\n1. Opening:\n2. Exploration/social:\n3. Conflict:\n4. Closing choice:");
        break;
    case 3:
        _sessionSummaryDraft = gm::localization::trUi("Encounters\n- Setup:\n- Stakes:\n- Enemy goal:\n- Terrain notes:\n- Retreat/failure state:");
        break;
    case 4:
        _lootAwardedDraft = gm::localization::trUi("Coins:\nItems:\nConsumables:\nStory rewards:\n");
        _xpMilestonesDraft = gm::localization::trUi("XP/Milestone:\n");
        break;
    case 5:
        _openHooksDraft = gm::localization::trUi("Follow-ups\n- \n");
        _unresolvedThreadsDraft = gm::localization::trUi("Threads to revisit\n- \n");
        break;
    default:
        return;
    }

    saveSessionSummary();
    saveSessionRecap();
}

void LiveSessionDashboardController::addTimelineEvent()
{
    if (_timelineTitleDraft.empty())
        return;

    TimelineEventModel event;
    event.title = _timelineTitleDraft;
    event.inWorldDate = _timelineDateDraft;
    event.description = _timelineDescriptionDraft;
    const StoryBeatModel *beat = activeStoryBeat();
    if (beat)
        event.relatedEntityIds.push_back(beat->metadata.id);
    _currentCampaign.timelineEvents.push_back(event);
    _timelineTitleDraft.clear();
    _timelineDescriptionDraft.clear();
    _currentCampaign.metadata.touch();
    saveCampaign("add timeline event");
}

void LiveSessionDashboardController::removeTimelineEvent(int eventIndex)
{
    if (eventIndex < 0 || eventIndex >= static_cast<int>(_currentCampaign.timelineEvents.size()))
        return;

    _currentCampaign.timelineEvents.erase(_currentCampaign.timelineEvents.begin() + eventIndex);
    _currentCampaign.metadata.touch();
    saveCampaign("remove timeline event");
}

bool LiveSessionDashboardController::generatePostSessionReport()
{
    const auto target = _sessionReview.report;
    const SessionModel *session = SessionReport::validate(_currentCampaign, target);
    if (!session)
    {
        const bool de = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
        _sessionReview.reportError = de ? "Die ausgewählte Sitzung hat sich geändert. Bitte abbrechen und erneut prüfen."
            : "The reviewed session changed. Cancel and review the report again.";
        return false;
    }

    std::ostringstream report;
    report << "# " << (session->title.empty() ? gm::localization::trUi("Session Report") : session->title) << "\n\n";
    if (!session->startedAtUtc.empty())
        report << gm::localization::trUi("Started:") << " " << session->startedAtUtc << "\n";
    if (!session->endedAtUtc.empty())
        report << gm::localization::trUi("Ended:") << " " << session->endedAtUtc << "\n";
    report << "\n## " << gm::localization::trUi("Recap") << "\n" << (session->recap.empty() ? session->summary : session->recap) << "\n\n";
    report << "## " << gm::localization::trUi("Unresolved Threads") << "\n" << session->unresolvedThreads << "\n\n";
    report << "## " << gm::localization::trUi("Loot Awarded") << "\n" << session->lootAwarded << "\n\n";
    report << "## " << gm::localization::trUi("XP / Milestones") << "\n" << session->xpMilestones << "\n\n";
    report << "## " << gm::localization::trUi("NPCs Met") << "\n" << session->npcsMet << "\n\n";
    report << "## " << gm::localization::trUi("Locations Visited") << "\n" << session->locationsVisited << "\n\n";
    report << "## " << gm::localization::trUi("Open Hooks") << "\n" << session->openHooks << "\n\n";

    report << "## " << gm::localization::trUi("Timestamped Notes") << "\n";
    if (session->notes.empty())
    {
        report << "- " << gm::localization::trUi("None") << "\n";
    }
    else
    {
        for (const SessionNoteModel &note : session->notes)
        {
            report << "- " << (note.timestampUtc.empty() ? note.metadata.createdAtUtc : note.timestampUtc) << ": " << SessionControl::systemTextForDisplay(note.body);
            if (!note.linkedEntityLabel.empty())
                report << " [" << (note.linkedEntityType == kSessionClockNoteLinkType
                                         ? InWorldTime::localizeDateTimeText(note.linkedEntityLabel)
                                         : note.linkedEntityLabel)
                       << "]";
            report << "\n";
        }
    }

    report << "\n## " << (gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German ? "Kampagnenzeitverlauf" : "Campaign timeline") << "\n";
    if (_currentCampaign.timelineEvents.empty())
    {
        report << "- " << gm::localization::trUi("None") << "\n";
    }
    else
    {
        for (const TimelineEventModel &event : _currentCampaign.timelineEvents)
            report << "- " << (event.inWorldDate.empty() ? "-" : InWorldTime::localizeDateTimeText(event.inWorldDate))
                   << ": " << SessionControl::systemTextForDisplay(event.title)
                   << " - " << SessionControl::systemTextForDisplay(event.description) << "\n";
    }

    _postSessionReportDraft = report.str();
    _sessionReview.output = target;
    _sessionReview.report = {}; _sessionReview.reportError.clear();
    return true;
}
