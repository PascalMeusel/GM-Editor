#include "Models/Session/SessionControlService.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Session/InWorldTimeService.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstring>

namespace
{
constexpr const char *kBreakStartedNote = "[Session State] Break started.";
constexpr const char *kBreakResumedNote = "[Session State] Resumed from break.";
constexpr std::size_t kClockReasonLimit = 180;

std::string trimCopy(const std::string &value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char character) {
        return std::isspace(character) != 0;
    }).base();

    if (first >= last)
        return {};
    return std::string(first, last);
}

std::string boundedNoteCopy(const std::string &value)
{
    std::string result = trimCopy(value);
    if (result.size() > kClockReasonLimit)
        result = result.substr(0, kClockReasonLimit);
    return result;
}

bool hasPrefix(const std::string &value, const char *prefix)
{
    return value.compare(0, std::strlen(prefix), prefix) == 0;
}

std::string sessionStateBody(const char *baseBody, const std::string &detail)
{
    const std::string trimmedDetail = boundedNoteCopy(detail);
    if (trimmedDetail.empty())
        return baseBody;
    return std::string(baseBody) + " " + trimmedDetail;
}

void appendSessionStateNote(SessionModel &session, const std::string &body)
{
    SessionNoteModel note;
    note.timestampUtc = currentUtcTimestamp();
    note.body = body;
    session.notes.push_back(note);
    session.metadata.touch();
}

void appendSessionStateTimelineEvent(Campaign &campaign,
                                     const SessionModel &session,
                                     const std::string &title,
                                     const std::string &description,
                                     const std::string &inWorldTime)
{
    TimelineEventModel event;
    event.title = title;
    event.description = description;
    event.inWorldDate = inWorldTime;
    event.relatedEntityIds.push_back(session.metadata.id);
    event.metadata.tags.push_back("break");
    event.metadata.touch();
    campaign.timelineEvents.push_back(event);
}

void appendClockNote(SessionModel &session, const std::string &body, const std::string &clockLabel)
{
    SessionNoteModel note;
    note.timestampUtc = currentUtcTimestamp();
    note.body = body;
    note.linkedEntityId = session.metadata.id;
    note.linkedEntityType = kSessionClockNoteLinkType;
    note.linkedEntityLabel = clockLabel;
    note.metadata.tags.push_back(kSessionClockNoteTag);
    session.notes.push_back(note);
    session.metadata.touch();
}

void appendClockTimelineEvent(Campaign &campaign, const SessionModel &session, const std::string &title, const std::string &description, const std::string &clockLabel)
{
    TimelineEventModel event;
    event.title = title;
    event.description = description;
    event.inWorldDate = clockLabel;
    event.relatedEntityIds.push_back(session.metadata.id);
    event.metadata.tags.push_back(kSessionClockNoteTag);
    event.metadata.touch();
    campaign.timelineEvents.push_back(event);
}

std::string clockAdvanceLabel(const std::string &currentLabel, const std::string &amount)
{
    if (currentLabel.empty())
        return "Advanced by " + amount;
    return currentLabel + " + " + amount;
}

void replaceSystemSeparator(std::string &text, const char *english, const char *localized)
{
    const std::size_t position = text.find(english);
    if (position != std::string::npos)
        text.replace(position, std::strlen(english), localized);
}

std::string sessionStateSuffixForDisplay(const std::string &suffix)
{
    constexpr const char *kPausedPrefix = " Audio channels paused: ";
    constexpr const char *kResumedPrefix = " Audio channels resumed: ";
    constexpr const char *kLegacyPausedPrefix = " Audio paused: ";
    constexpr const char *kLegacyResumedPrefix = " Audio resumed: ";
    if (hasPrefix(suffix, kPausedPrefix))
        return std::string(gm::localization::trUi(kPausedPrefix)) + suffix.substr(std::strlen(kPausedPrefix));
    if (hasPrefix(suffix, kResumedPrefix))
        return std::string(gm::localization::trUi(kResumedPrefix)) + suffix.substr(std::strlen(kResumedPrefix));
    if (hasPrefix(suffix, kLegacyPausedPrefix) || hasPrefix(suffix, kLegacyResumedPrefix))
    {
        const char *prefix = hasPrefix(suffix, kLegacyPausedPrefix) ? kLegacyPausedPrefix : kLegacyResumedPrefix;
        std::string localized = std::string(gm::localization::trUi(prefix)) + suffix.substr(std::strlen(prefix));
        const std::size_t channels = localized.rfind(" channels.");
        const std::size_t channel = localized.rfind(" channel.");
        if (channels != std::string::npos && channels + std::strlen(" channels.") == localized.size())
            localized.replace(channels, std::strlen(" channels."), gm::localization::trUi(" channels."));
        else if (channel != std::string::npos && channel + std::strlen(" channel.") == localized.size())
            localized.replace(channel, std::strlen(" channel."), gm::localization::trUi(" channel."));
        return localized;
    }
    if (suffix == " Audio pause requested; no playing channels changed." ||
        suffix == " Audio resume requested; no paused channels changed.")
        return gm::localization::trUi(suffix.c_str());
    return suffix;
}
}

namespace SessionControl
{
SessionModel *activeSession(Campaign &campaign)
{
    const auto found = std::find_if(campaign.sessions.begin(), campaign.sessions.end(), [](const SessionModel &session) {
        return session.liveModeActive;
    });
    return found == campaign.sessions.end() ? nullptr : &(*found);
}

const SessionModel *activeSession(const Campaign &campaign)
{
    const auto found = std::find_if(campaign.sessions.begin(), campaign.sessions.end(), [](const SessionModel &session) {
        return session.liveModeActive;
    });
    return found == campaign.sessions.end() ? nullptr : &(*found);
}

std::string defaultSessionTitle(const Campaign &campaign)
{
    return "Session " + std::to_string(campaign.sessions.size() + 1);
}

bool isBreakActive(const SessionModel &session)
{
    if (session.liveState.initialized)
        return session.liveState.phase == SessionPhase::Break;
    for (auto it = session.notes.rbegin(); it != session.notes.rend(); ++it)
    {
        if (hasPrefix(it->body, kBreakStartedNote))
            return true;
        if (hasPrefix(it->body, kBreakResumedNote))
            return false;
    }
    return false;
}

bool isLiveSessionBreakActive(const Campaign &campaign)
{
    const SessionModel *session = activeSession(campaign);
    return session && isBreakActive(*session);
}

std::string currentInWorldTime(const SessionModel &session)
{
    for (auto it = session.notes.rbegin(); it != session.notes.rend(); ++it)
    {
        if (it->linkedEntityType == kSessionClockNoteLinkType && !it->linkedEntityLabel.empty())
            return it->linkedEntityLabel;
    }
    return {};
}

std::string systemTextForDisplay(const std::string &text)
{
    if (hasPrefix(text, kBreakStartedNote))
        return std::string(gm::localization::trUi(kBreakStartedNote)) + sessionStateSuffixForDisplay(text.substr(std::strlen(kBreakStartedNote)));
    if (hasPrefix(text, kBreakResumedNote))
        return std::string(gm::localization::trUi(kBreakResumedNote)) + sessionStateSuffixForDisplay(text.substr(std::strlen(kBreakResumedNote)));

    constexpr const char *kAdvancedPrefix = "[Game Time] Advanced by ";
    constexpr const char *kSetPrefix = "[Game Time] Set game time to ";
    if (hasPrefix(text, kAdvancedPrefix) || hasPrefix(text, kSetPrefix))
    {
        const char *prefix = hasPrefix(text, kAdvancedPrefix) ? kAdvancedPrefix : kSetPrefix;
        std::string localized = std::string(gm::localization::trUi(prefix)) + text.substr(std::strlen(prefix));
        replaceSystemSeparator(localized, " Previous: ", gm::localization::trUi(" Previous: "));
        replaceSystemSeparator(localized, " Note: ", gm::localization::trUi(" Note: "));
        return InWorldTime::localizeDateTimeText(localized);
    }

    if (text == "Session break started" || text == "Session break resumed" ||
        text == "Game time advanced" || text == "Game time set")
        return gm::localization::trUi(text.c_str());

    constexpr const char *kAdvancedLabel = "Advanced by ";
    if (hasPrefix(text, kAdvancedLabel))
        return std::string(gm::localization::trUi(kAdvancedLabel)) + text.substr(std::strlen(kAdvancedLabel));

    return text;
}

SessionModel *startLiveSession(Campaign &campaign, const std::string &title)
{
    if (activeSession(campaign))
        return nullptr;

    SessionModel session;
    session.title = title.empty() ? defaultSessionTitle(campaign) : title;
    session.scheduledAtUtc = currentUtcTimestamp();
    session.startedAtUtc = session.scheduledAtUtc;
    session.liveModeActive = true;
    session.liveState.initialized = true;
    session.liveState.phase = SessionPhase::Setup;
    session.metadata.tags.push_back("live");
    for (const CharacterModel &character : campaign.characters)
        session.participantCharacterIds.push_back(character.metadata.id);

    campaign.sessionIds.push_back(session.metadata.id);
    campaign.sessions.push_back(session);
    campaign.metadata.touch();
    return &campaign.sessions.back();
}

bool endLiveSession(Campaign &campaign, EntityId *endedSessionId, std::string *endedSessionTitle)
{
    SessionModel *session = activeSession(campaign);
    if (!session)
        return false;

    session->liveModeActive = false;
    session->liveState.initialized = true;
    session->liveState.phase = SessionPhase::WrapUp;
    session->endedAtUtc = currentUtcTimestamp();
    session->metadata.touch();
    // Ending the table session is a security boundary. Keep the configured
    // public payload for the next reviewed publication, but never leave the
    // second screen showing campaign content after the session has ended.
    blackoutLocalPlayerDisplay(campaign.localPlayerDisplay);
    campaign.localPlayerDisplay.metadata.touch();
    campaign.metadata.touch();

    if (endedSessionId)
        *endedSessionId = session->metadata.id;
    if (endedSessionTitle)
        *endedSessionTitle = session->title;
    return true;
}

bool startBreak(Campaign &campaign, const std::string &detail, const std::string &inWorldTime)
{
    SessionModel *session = activeSession(campaign);
    if (!session || isBreakActive(*session))
        return false;

    const std::string body = sessionStateBody(kBreakStartedNote, detail);
    session->liveState.phaseBeforeBreak = session->liveState.phase;
    session->liveState.initialized = true;
    session->liveState.phase = SessionPhase::Break;
    appendSessionStateNote(*session, body);
    appendSessionStateTimelineEvent(campaign, *session, "Session break started", body, inWorldTime.empty() ? InWorldTime::formatDateTime(campaign.clock) : inWorldTime);
    campaign.metadata.touch();
    return true;
}

bool resumeFromBreak(Campaign &campaign, const std::string &detail, const std::string &inWorldTime)
{
    SessionModel *session = activeSession(campaign);
    if (!session || !isBreakActive(*session))
        return false;

    const std::string body = sessionStateBody(kBreakResumedNote, detail);
    session->liveState.initialized = true;
    session->liveState.phase = session->liveState.phaseBeforeBreak;
    appendSessionStateNote(*session, body);
    appendSessionStateTimelineEvent(campaign, *session, "Session break resumed", body, inWorldTime.empty() ? InWorldTime::formatDateTime(campaign.clock) : inWorldTime);
    campaign.metadata.touch();
    return true;
}

bool advanceInWorldTime(Campaign &campaign,
                        const std::string &amount,
                        const std::string &reason,
                        const std::string &fallbackInWorldTime,
                        std::string *newInWorldTime)
{
    SessionModel *session = activeSession(campaign);
    const std::string trimmedAmount = trimCopy(amount);
    const std::string trimmedReason = boundedNoteCopy(reason);
    if (!session || trimmedAmount.empty() || trimmedReason.empty())
        return false;

    long long minutes = 0;
    if (!InWorldTime::parseDurationMinutes(trimmedAmount, campaign.clock.calendar, minutes))
        return false;

    std::string previous = InWorldTime::formatDateTime(campaign.clock);
    if (previous.empty())
        previous = trimCopy(fallbackInWorldTime);
    InWorldTime::advanceMinutes(campaign.clock, minutes);
    const std::string clockLabel = InWorldTime::formatDateTime(campaign.clock);

    std::string body = "[Game Time] Advanced by " + trimmedAmount + ".";
    if (!previous.empty())
        body += " Previous: " + previous + ".";
    body += " Note: " + trimmedReason;

    appendClockNote(*session, body, clockLabel);
    appendClockTimelineEvent(campaign, *session, "Game time advanced", body, clockLabel);
    campaign.metadata.touch();
    if (newInWorldTime)
        *newInWorldTime = clockLabel;
    return true;
}

bool adjustInWorldTime(Campaign &campaign,
                       const std::string &inWorldTime,
                       const std::string &reason,
                       std::string *newInWorldTime)
{
    SessionModel *session = activeSession(campaign);
    const std::string trimmedTime = trimCopy(inWorldTime);
    const std::string trimmedReason = boundedNoteCopy(reason);
    if (!session || trimmedTime.empty() || trimmedReason.empty())
        return false;

    const std::string previous = currentInWorldTime(*session);
    std::string body = "[Game Time] Set game time to " + trimmedTime + ".";
    if (!previous.empty() && previous != trimmedTime)
        body += " Previous: " + previous + ".";
    body += " Note: " + trimmedReason;

    appendClockNote(*session, body, trimmedTime);
    appendClockTimelineEvent(campaign, *session, "Game time set", body, trimmedTime);
    campaign.metadata.touch();
    if (newInWorldTime)
        *newInWorldTime = trimmedTime;
    return true;
}

bool setInWorldDateTime(Campaign &campaign,
                        const InWorldDateTime &dateTime,
                        const std::string &reason,
                        std::string *newInWorldTime)
{
    SessionModel *session = activeSession(campaign);
    const std::string trimmedReason = boundedNoteCopy(reason);
    if (!session || trimmedReason.empty())
        return false;

    const std::string previous = InWorldTime::formatDateTime(campaign.clock);
    InWorldTime::setDateTime(campaign.clock, dateTime, campaign.clock.running);
    const std::string clockLabel = InWorldTime::formatDateTime(campaign.clock);

    std::string body = "[Game Time] Set game time to " + clockLabel + ".";
    if (!previous.empty() && previous != clockLabel)
        body += " Previous: " + previous + ".";
    body += " Note: " + trimmedReason;

    appendClockNote(*session, body, clockLabel);
    appendClockTimelineEvent(campaign, *session, "Game time set", body, clockLabel);
    campaign.metadata.touch();
    if (newInWorldTime)
        *newInWorldTime = clockLabel;
    return true;
}
}
