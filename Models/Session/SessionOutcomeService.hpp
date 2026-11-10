#pragma once

#include "Models/Session/SessionOutcomeModels.hpp"
#include "Models/Session/SessionPlanService.hpp"
#include <unordered_map>

namespace SessionOutcomes
{
using Persist = SessionPlan::Persist;
using IdRemap = std::unordered_map<EntityId, EntityId>;

// Only this reserved session property owns the ledger. Missing means legacy empty.
// Invalid or future data is reported without altering the original property.
std::string serialize(const Ledger &);
ReadResult deserialize(const std::string &);
ReadResult read(const SessionModel &);
void write(SessionModel &, const Ledger &);
std::string signature(const Outcome &);
Result capture(Campaign &, EntityId sessionId, Outcome, const Persist &);
Result update(Campaign &, EntityId sessionId, const Outcome &expected, Outcome replacement, const Persist &);
Result reject(Campaign &, EntityId sessionId, const Outcome &expected, const Persist &);
ApplyReview reviewApply(const Campaign &, EntityId sessionId, const std::vector<EntityId> &);
Result apply(Campaign &, const ApplyReview &, const Persist &);
Result undo(Campaign &, EntityId sessionId, EntityId receiptId, const Persist &);
Result carryForward(Campaign &, const CarryForwardRequest &, const Persist &);
Result setFollowUpCompleted(Campaign &, EntityId sessionId, EntityId followUpId, bool, const Persist &);
Result setPublicRecap(Campaign &, EntityId sessionId, const std::string &title,
                      const std::string &intro, const Persist &);
Result setPlayerText(Campaign &, EntityId sessionId, EntityId outcomeId,
                     const std::string &expectedText, bool expectedShare,
                     const std::string &newText, bool newShare, const Persist &);
PlayerRecapReview reviewRecap(const Campaign &, EntityId sessionId);
bool isRecapCurrent(const Campaign &, const PlayerRecapReview &);
std::string markdown(const PlayerRecapDocument &);
std::string html(const PlayerRecapDocument &);

// Clone integration is two-pass: assign all IDs before remapping references.
// Malformed ledgers throw, allowing the enclosing import to abort safely.
void assignCloneIds(Campaign &, IdRemap &);
void remapCloneReferences(Campaign &, const IdRemap &);
void remap(Ledger &, const IdRemap &);
}
