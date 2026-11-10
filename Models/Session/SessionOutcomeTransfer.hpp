#pragma once
// Included only by SessionOutcomeService.cpp.
namespace SessionOutcomes
{
namespace detail
{
inline std::string carryToken(const CarryForwardRequest &r)
{
    Writer w; w.text(r.sourceSessionId); w.text(r.newSessionTitle);
    w.number(r.outcomeIds.size()); for (const auto &id : r.outcomeIds) w.text(id);
    w.number(r.sceneIds.size()); for (const auto &id : r.sceneIds) w.text(id);
    return w.value;
}
inline void rewrite(EntityId &id, const IdRemap &ids)
{ if (const auto found = ids.find(id); found != ids.end()) id = found->second; }
inline void rewriteOutcome(Outcome &v, const IdRemap &ids)
{ rewrite(v.id, ids); rewrite(v.sourceNoteId, ids); rewrite(v.sourceSceneId, ids); rewrite(v.target.id, ids); }
}
Result carryForward(Campaign &c, const CarryForwardRequest &request, const Persist &persist)
{
    return detail::commit(c, persist, [&](Campaign &staged) {
        detail::check(!request.outcomeIds.empty() || !request.sceneIds.empty(), Status::Invalid, "Select outcomes or scenes to carry forward.");
        const SessionModel source = detail::session(staged, request.sourceSessionId);
        auto sourceLedger = detail::ledger(source);
        EntityId destinationId = request.targetSessionId;
        bool created = false;
        if (destinationId.empty())
        {
            detail::check(!request.requestId.empty() && !detail::blank(request.newSessionTitle), Status::Invalid, "Name the next session.");
            const auto token = detail::carryToken(request);
            for (const auto &candidate : staged.sessions)
            {
                auto candidateLedger = detail::ledger(candidate);
                if (candidateLedger.carryRequestId != request.requestId) continue;
                detail::check(candidateLedger.carryRequestToken == token, Status::Conflict, "This carry-forward request has changed.");
                destinationId = candidate.metadata.id; break;
            }
            if (destinationId.empty())
            {
                SessionModel next; next.title = request.newSessionTitle; destinationId = next.metadata.id;
                Ledger l; l.carryRequestId = request.requestId; l.carryRequestToken = token; write(next, l);
                staged.sessions.push_back(std::move(next)); staged.sessionIds.push_back(destinationId); created = true;
            }
        }
        detail::check(destinationId != source.metadata.id, Status::Invalid, "Choose a different next session.");
        auto &destination = detail::session(staged, destinationId);
        detail::check(!destination.liveModeActive && destination.startedAtUtc.empty() && destination.endedAtUtc.empty(),
            Status::Conflict, "The destination must be an unstarted session.");
        auto l = detail::ledger(destination); bool changed = created;
        std::unordered_set<EntityId> selected;
        for (const auto &id : request.outcomeIds)
        {
            detail::check(selected.insert(id).second, Status::Invalid, "An outcome was selected twice.");
            const auto &v = detail::find(sourceLedger, id);
            detail::check(v.state != State::Rejected, Status::Conflict, "Rejected outcomes cannot be carried forward.");
            const auto duplicate = std::find_if(l.followUps.begin(), l.followUps.end(), [&](const auto &row) {
                return row.sourceSessionId == source.metadata.id && row.sourceOutcomeId == id;
            });
            if (duplicate != l.followUps.end()) continue;
            FollowUp next; next.sourceSessionId = source.metadata.id; next.sourceOutcomeId = id;
            next.sourceSceneId = v.sourceSceneId; next.privateText = v.privateText; next.target = v.target;
            l.followUps.push_back(std::move(next)); changed = true;
        }
        selected.clear();
        for (const auto &id : request.sceneIds)
        {
            detail::check(selected.insert(id).second, Status::Invalid, "A scene was selected twice.");
            detail::check(detail::availableScene(staged, id) &&
                std::any_of(source.planEntries.begin(), source.planEntries.end(), [&](const auto &e) { return e.storyBeatId == id; }),
                Status::NotFound, "A selected source scene is unavailable.");
            if (std::any_of(destination.planEntries.begin(), destination.planEntries.end(), [&](const auto &e) { return e.storyBeatId == id; })) continue;
            SessionPlanEntry entry; entry.storyBeatId = id; destination.planEntries.push_back(std::move(entry)); changed = true;
        }
        if (changed) write(destination, l);
        auto result = detail::success(changed); result.targetSessionId = destinationId; return result;
    });
}
void assignCloneIds(Campaign &c, IdRemap &ids)
{
    const auto assign = [&](const EntityId &id, const char *prefix) {
        if (!id.empty() && !ids.count(id)) ids.emplace(id, createEntityId(prefix));
    };
    for (const auto &s : c.sessions)
    {
        const auto l = detail::ledger(s);
        for (const auto &v : l.outcomes) assign(v.id, "session_outcome");
        for (const auto &v : l.receipts) assign(v.id, "outcome_receipt");
        for (const auto &v : l.followUps) assign(v.id, "session_followup");
        assign(l.carryRequestId, "outcome_carry");
    }
}
void remap(Ledger &l, const IdRemap &ids)
{
    for (auto &v : l.outcomes) detail::rewriteOutcome(v, ids);
    for (auto &r : l.receipts)
    {
        detail::rewrite(r.id, ids);
        for (auto &v : r.changes)
        {
            detail::rewrite(v.outcomeId, ids); detail::rewrite(v.target.id, ids);
            detail::Reader reader{v.outcomeSignature}; auto original = detail::outcome(reader);
            detail::require(reader.cursor == v.outcomeSignature.size());
            detail::rewriteOutcome(original, ids); v.outcomeSignature = signature(original);
        }
    }
    for (auto &v : l.followUps)
    {
        detail::rewrite(v.id, ids); detail::rewrite(v.sourceSessionId, ids); detail::rewrite(v.sourceOutcomeId, ids);
        detail::rewrite(v.sourceSceneId, ids); detail::rewrite(v.target.id, ids);
    }
    detail::rewrite(l.carryRequestId, ids);
    if (!l.carryRequestToken.empty())
    {
        detail::Reader r{l.carryRequestToken}; CarryForwardRequest request;
        request.sourceSessionId = r.text(); request.newSessionTitle = r.text();
        auto count = r.number(detail::MaxEntries); for (std::size_t i = 0; i < count; ++i) request.outcomeIds.push_back(r.text());
        count = r.number(detail::MaxEntries); for (std::size_t i = 0; i < count; ++i) request.sceneIds.push_back(r.text());
        detail::require(r.cursor == l.carryRequestToken.size());
        detail::rewrite(request.sourceSessionId, ids);
        for (auto &id : request.outcomeIds) detail::rewrite(id, ids);
        for (auto &id : request.sceneIds) detail::rewrite(id, ids);
        l.carryRequestToken = detail::carryToken(request);
    }
}
void remapCloneReferences(Campaign &c, const IdRemap &ids)
{
    for (auto &s : c.sessions)
    {
        if (!s.metadata.customProperties.count(StorageKey)) continue;
        auto l = detail::ledger(s); remap(l, ids); write(s, l);
    }
}
}
