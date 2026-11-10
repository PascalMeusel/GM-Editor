#pragma once

#include "Models/Core/EntityMetadata.hpp"
#include <vector>

namespace SessionOutcomes
{
inline constexpr const char *StorageKey = "gm.session.outcomes.v1";
enum class Kind { Decision, Consequence, OpenHook };
enum class State { Pending, Applied, Rejected };
enum class TargetType { None, Npc, Place, Objective, Clue };
// Persisted numeric identities: append new actions without reordering old ones.
enum class Action { RecordOnly = 0, NpcAttitude = 1, PlaceNote = 2, ObjectiveState = 3, ClueStatus = 4, NpcNote = 5 };
struct Target { TargetType type = TargetType::None; EntityId id; };

struct Outcome
{
    EntityId id = createEntityId("session_outcome");
    std::string createdAtUtc = currentUtcTimestamp();
    Kind kind = Kind::Consequence;
    State state = State::Pending;
    std::string privateText, publicText;
    bool shareWithPlayers = false;
    EntityId sourceNoteId, sourceSceneId;
    Target target;
    Action action = Action::RecordOnly;
    // NpcAttitude replaces the attitude; NpcNote/PlaceNote append this text.
    // ObjectiveState and ClueStatus use their respective model enum integer.
    std::string changeText;
    int desiredStatus = 0;
};
struct Change
{
    EntityId outcomeId;
    Target target;
    Action action = Action::RecordOnly;
    std::string before, after, outcomeSignature;
};
struct Receipt
{
    EntityId id = createEntityId("outcome_receipt");
    std::string appliedAtUtc;
    bool undone = false;
    std::vector<Change> changes;
};
struct FollowUp
{
    EntityId id = createEntityId("session_followup");
    EntityId sourceSessionId, sourceOutcomeId, sourceSceneId;
    std::string privateText;
    Target target;
    bool completed = false;
};
struct Ledger
{
    std::vector<Outcome> outcomes;
    std::vector<Receipt> receipts;
    std::vector<FollowUp> followUps;
    std::string publicTitle, publicIntro;
    EntityId carryRequestId;
    std::string carryRequestToken;
};
enum class Status { Applied, NoChange, Invalid, NotFound, Conflict, PersistenceFailed, CorruptData };
struct Result
{
    bool success = false;
    Status status = Status::Invalid;
    std::string error;
    EntityId outcomeId, receiptId, targetSessionId;
    bool applied() const { return success; }
};
struct ReadResult
{
    bool success = false;
    std::string error;
    Ledger ledger;
};
struct ApplyReview
{
    bool success = false;
    std::string error;
    EntityId campaignId, sessionId;
    Receipt receipt;
};
struct CarryForwardRequest
{
    EntityId requestId = createEntityId("outcome_carry");
    EntityId sourceSessionId, targetSessionId;
    std::string newSessionTitle;
    std::vector<EntityId> outcomeIds, sceneIds;
};
struct PlayerRecapItem { EntityId outcomeId; std::string text; };
struct PlayerRecapDocument
{
    std::string title, intro;
    std::vector<PlayerRecapItem> items;
};
struct PlayerRecapReview
{
    bool success = false;
    std::string error;
    EntityId campaignId, sessionId;
    PlayerRecapDocument document;
    // Exact canonical projection bytes, not a hash with collision risk.
    std::string contentToken;
};
}
