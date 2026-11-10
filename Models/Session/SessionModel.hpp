#pragma once

#include "Models/Core/EntityMetadata.hpp"
#include <string>
#include <vector>

inline constexpr const char *kSessionClockNoteLinkType = "session_clock";
inline constexpr const char *kSessionClockNoteTag = "clock";

enum class SessionEntryState { Planned, Completed, Skipped };
enum class SessionPhase { Setup, Scene, Exploration, Combat, Break, WrapUp };

struct SessionPlanEntry
{
    EntityMetadata metadata{"session_plan_entry"};
    EntityId storyBeatId;
    std::string prepNote;
    SessionEntryState state = SessionEntryState::Planned;
};

struct SessionLiveState
{
    // False distinguishes an old session from an explicitly saved Setup phase.
    bool initialized = false;
    SessionPhase phase = SessionPhase::Setup;
    SessionPhase phaseBeforeBreak = SessionPhase::Setup;
    EntityId activePlanEntryId;
    EntityId activeEncounterId;
};

struct SessionNoteModel
{
    EntityMetadata metadata{"session_note"};
    std::string timestampUtc;
    std::string body;
    EntityId linkedEntityId;
    std::string linkedEntityType;
    std::string linkedEntityLabel;
};

struct SessionModel
{
    EntityMetadata metadata{"session"};
    std::string title;
    std::string scheduledAtUtc;
    std::string startedAtUtc;
    std::string endedAtUtc;
    std::string summary;
    std::string recap;
    std::string unresolvedThreads;
    std::string lootAwarded;
    std::string xpMilestones;
    std::string npcsMet;
    std::string locationsVisited;
    std::string openHooks;
    std::vector<EntityId> participantCharacterIds;
    std::vector<EntityId> encounterIds;
    std::vector<EntityId> storyDocumentIds;
    std::vector<SessionNoteModel> notes;
    bool liveModeActive = false;
    std::string roughNotes;
    std::vector<SessionPlanEntry> planEntries;
    SessionLiveState liveState;
};
