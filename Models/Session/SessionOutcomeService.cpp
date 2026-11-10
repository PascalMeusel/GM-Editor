#include "Models/Session/SessionOutcomeOperations.hpp"

namespace SessionOutcomes
{
std::string serialize(const Ledger &v)
{
    const auto text = detail::encode(v); (void)detail::decode(text); return text;
}
ReadResult deserialize(const std::string &text)
{
    ReadResult result;
    try { result.ledger = detail::decode(text); result.success = true; }
    catch (const std::exception &e) { result.error = e.what(); }
    return result;
}
ReadResult read(const SessionModel &s)
{
    const auto found = s.metadata.customProperties.find(StorageKey);
    return deserialize(found == s.metadata.customProperties.end() ? std::string{} : found->second);
}
void write(SessionModel &s, const Ledger &v)
{
    auto text = serialize(v); s.metadata.customProperties[StorageKey] = std::move(text); s.metadata.touch();
}
std::string signature(const Outcome &v)
{ detail::Writer w; detail::outcome(w, v); return w.value; }

Result capture(Campaign &c, EntityId sessionId, Outcome draft, const Persist &persist)
{
    return detail::commit(c, persist, [&](Campaign &staged) {
        auto &s = detail::session(staged, sessionId); auto l = detail::ledger(s);
        detail::check(draft.state == State::Pending, Status::Invalid, "New outcomes must await review.");
        detail::validate(staged, s, draft);
        const auto exists = std::find_if(l.outcomes.begin(), l.outcomes.end(), [&](const auto &v) { return v.id == draft.id; });
        if (exists != l.outcomes.end())
        {
            detail::check(signature(*exists) == signature(draft), Status::Conflict, "This outcome identity is already used.");
            auto result = detail::success(false); result.outcomeId = draft.id; return result;
        }
        l.outcomes.push_back(draft); write(s, l); auto result = detail::success(); result.outcomeId = draft.id; return result;
    });
}
Result update(Campaign &c, EntityId sessionId, const Outcome &expected, Outcome replacement, const Persist &persist)
{
    return detail::commit(c, persist, [&](Campaign &staged) {
        auto &s = detail::session(staged, sessionId); auto l = detail::ledger(s); auto &current = detail::find(l, expected.id);
        detail::check(signature(current) == signature(expected), Status::Conflict, "The saved outcome changed. Review your draft again.");
        detail::check(current.state != State::Applied && replacement.state != State::Applied, Status::Conflict, "Undo an applied change before editing it.");
        detail::check(replacement.id == expected.id && replacement.createdAtUtc == expected.createdAtUtc &&
            replacement.sourceNoteId == expected.sourceNoteId && replacement.sourceSceneId == expected.sourceSceneId,
            Status::Invalid, "An outcome's original identity and source must be preserved.");
        detail::validate(staged, s, replacement);
        if (signature(current) == signature(replacement)) return detail::success(false);
        current = std::move(replacement); write(s, l); return detail::success();
    });
}
Result reject(Campaign &c, EntityId sessionId, const Outcome &expected, const Persist &persist)
{
    auto replacement = expected; replacement.state = State::Rejected;
    return update(c, sessionId, expected, std::move(replacement), persist);
}
ApplyReview reviewApply(const Campaign &c, EntityId sessionId, const std::vector<EntityId> &ids)
{
    ApplyReview review; review.campaignId = c.metadata.id; review.sessionId = sessionId;
    try
    {
        detail::check(!ids.empty(), Status::Invalid, "Select at least one outcome.");
        Campaign preview = c; const auto &s = detail::session(preview, sessionId); auto l = detail::ledger(s);
        std::unordered_set<EntityId> seen;
        for (const auto &id : ids)
        {
            detail::check(seen.insert(id).second, Status::Invalid, "An outcome was selected twice.");
            const auto &v = detail::find(l, id);
            detail::check(v.state == State::Pending, Status::Conflict, "Only pending outcomes can be applied.");
            detail::validate(preview, s, v);
            auto change = detail::changeFor(preview, v); detail::setField(preview, change, change.after);
            review.receipt.changes.push_back(std::move(change));
        }
        review.success = true;
    }
    catch (const std::exception &e) { review.error = e.what(); }
    return review;
}
Result apply(Campaign &c, const ApplyReview &review, const Persist &persist)
{
    return detail::commit(c, persist, [&](Campaign &staged) {
        detail::check(review.success && review.campaignId == staged.metadata.id && !review.receipt.id.empty() &&
            !review.receipt.changes.empty(), Status::Invalid, "A current apply review is required.");
        auto &s = detail::session(staged, review.sessionId); auto l = detail::ledger(s);
        const auto old = std::find_if(l.receipts.begin(), l.receipts.end(), [&](const auto &r) { return r.id == review.receipt.id; });
        if (old != l.receipts.end())
        {
            detail::check(!old->undone && old->changes.size() == review.receipt.changes.size() &&
                std::equal(old->changes.begin(), old->changes.end(), review.receipt.changes.begin(), detail::same),
                Status::Conflict, "This apply review has already changed or been undone.");
            auto result = detail::success(false); result.receiptId = old->id; return result;
        }
        std::unordered_set<EntityId> seen;
        for (const auto &change : review.receipt.changes)
        {
            detail::check(seen.insert(change.outcomeId).second, Status::Invalid, "An outcome was selected twice.");
            auto &v = detail::find(l, change.outcomeId);
            detail::check(v.state == State::Pending, Status::Conflict, "This outcome is no longer pending.");
            detail::validate(staged, s, v);
            detail::check(detail::same(detail::changeFor(staged, v), change), Status::Conflict, "The outcome or target changed after review.");
            detail::setField(staged, change, change.after); v.state = State::Applied;
        }
        auto receipt = review.receipt; receipt.appliedAtUtc = currentUtcTimestamp(); receipt.undone = false;
        l.receipts.push_back(receipt); write(s, l);
        auto result = detail::success(); result.receiptId = receipt.id; return result;
    });
}
Result undo(Campaign &c, EntityId sessionId, EntityId receiptId, const Persist &persist)
{
    return detail::commit(c, persist, [&](Campaign &staged) {
        auto &s = detail::session(staged, sessionId); auto l = detail::ledger(s);
        auto found = std::find_if(l.receipts.begin(), l.receipts.end(), [&](const auto &v) { return v.id == receiptId; });
        detail::check(found != l.receipts.end(), Status::NotFound, "Applied change is unavailable.");
        if (found->undone) return detail::success(false);
        for (auto it = found->changes.rbegin(); it != found->changes.rend(); ++it)
        {
            auto &v = detail::find(l, it->outcomeId); auto original = v; original.state = State::Pending;
            detail::check(v.state == State::Applied && detail::same(detail::changeFromBefore(original, it->before), *it) &&
                detail::field(staged, *it) == it->after, Status::Conflict,
                "This outcome or target has changed. Undo would overwrite newer work.");
            detail::setField(staged, *it, it->before); v.state = State::Pending;
        }
        found->undone = true; write(s, l); auto result = detail::success(); result.receiptId = receiptId; return result;
    });
}
Result setFollowUpCompleted(Campaign &c, EntityId sessionId, EntityId id, bool completed, const Persist &persist)
{
    return detail::commit(c, persist, [&](Campaign &staged) {
        auto &s = detail::session(staged, sessionId); auto l = detail::ledger(s);
        const auto it = std::find_if(l.followUps.begin(), l.followUps.end(), [&](const auto &v) { return v.id == id; });
        detail::check(it != l.followUps.end(), Status::NotFound, "Follow-up is unavailable.");
        if (it->completed == completed) return detail::success(false);
        it->completed = completed; write(s, l); return detail::success();
    });
}
Result setPublicRecap(Campaign &c, EntityId sessionId, const std::string &title, const std::string &intro, const Persist &persist)
{
    return detail::commit(c, persist, [&](Campaign &staged) {
        auto &s = detail::session(staged, sessionId); auto l = detail::ledger(s);
        if (l.publicTitle == title && l.publicIntro == intro) return detail::success(false);
        l.publicTitle = title; l.publicIntro = intro; write(s, l); return detail::success();
    });
}
Result setPlayerText(Campaign &c, EntityId sessionId, EntityId outcomeId,
                     const std::string &expectedText, bool expectedShare,
                     const std::string &newText, bool newShare, const Persist &persist)
{
    return detail::commit(c, persist, [&](Campaign &staged) {
        auto &s = detail::session(staged, sessionId); auto l = detail::ledger(s); auto &v = detail::find(l, outcomeId);
        detail::check(v.publicText == expectedText && v.shareWithPlayers == expectedShare,
            Status::Conflict, "The player-facing text changed. Review your draft again.");
        if (v.publicText == newText && v.shareWithPlayers == newShare) return detail::success(false);
        v.publicText = newText; v.shareWithPlayers = newShare; write(s, l); return detail::success();
    });
}
}

// Implementation-only splits keep the persistence/service boundary easy to audit.
#include "Models/Session/SessionOutcomeTransfer.hpp"
#include "Models/Session/SessionOutcomeRecap.hpp"
