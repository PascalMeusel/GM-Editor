#pragma once
#include "Models/Session/SessionOutcomeService.hpp"
#include "Models/Session/SessionOutcomeCodec.hpp"
#include <algorithm>
#include <functional>
#include <utility>

namespace SessionOutcomes::detail
{
struct Failure : std::runtime_error
{
    Status status;
    Failure(Status code, const std::string &message) : std::runtime_error(message), status(code) {}
};
inline void check(bool condition, Status code, const char *message)
{ if (!condition) throw Failure(code, message); }
inline Result failure(Status code, const std::string &message)
{ Result r; r.status = code; r.error = message; return r; }
inline Result success(bool changed = true)
{ Result r; r.success = true; r.status = changed ? Status::Applied : Status::NoChange; return r; }
inline bool blank(const std::string &value) { return value.find_first_not_of(" \r\n\t") == std::string::npos; }
inline bool archived(const EntityMetadata &m)
{ return std::find(m.tags.begin(), m.tags.end(), "archived") != m.tags.end(); }
inline bool availableScene(const Campaign &c, const EntityId &id)
{
    for (const auto &thread : c.storyThreads)
        if (!archived(thread.metadata))
            for (const auto &scene : thread.beats)
                if (scene.metadata.id == id && !archived(scene.metadata)) return true;
    return false;
}
inline const SessionModel &session(const Campaign &c, const EntityId &id)
{
    const auto *s = SessionPlan::findSession(c, id);
    check(s && !archived(s->metadata), Status::NotFound, "Session is unavailable."); return *s;
}
inline SessionModel &session(Campaign &c, const EntityId &id)
{ session(static_cast<const Campaign &>(c), id); return *SessionPlan::findSession(c, id); }
inline Ledger ledger(const SessionModel &s)
{
    auto r = read(s); check(r.success, Status::CorruptData, r.error.c_str()); return std::move(r.ledger);
}
inline Outcome &find(Ledger &l, const EntityId &id)
{
    const auto it = std::find_if(l.outcomes.begin(), l.outcomes.end(), [&](const auto &v) { return v.id == id; });
    check(it != l.outcomes.end(), Status::NotFound, "Outcome is unavailable."); return *it;
}
template<class T> inline const T &entity(const std::vector<T> &values, const EntityId &id)
{
    const auto it = std::find_if(values.begin(), values.end(), [&](const auto &v) { return v.metadata.id == id && !archived(v.metadata); });
    check(it != values.end(), Status::NotFound, "Linked entry is unavailable."); return *it;
}
template<class T> inline T &entity(std::vector<T> &values, const EntityId &id)
{ return const_cast<T &>(entity(static_cast<const std::vector<T> &>(values), id)); }
inline void validateTarget(const Campaign &c, const Target &t)
{
    check((t.type == TargetType::None) == t.id.empty(), Status::Invalid, "Choose a valid linked entry.");
    switch (t.type)
    {
    case TargetType::None: return;
    case TargetType::Npc: (void)entity(c.npcs, t.id); return;
    case TargetType::Place: (void)entity(c.worldPlaces, t.id); return;
    case TargetType::Objective: (void)entity(c.storyObjectives, t.id); return;
    case TargetType::Clue: (void)entity(c.storyClues, t.id); return;
    }
    throw Failure(Status::Invalid, "Unsupported target type.");
}
inline void validate(const Campaign &c, const SessionModel &s, const Outcome &v)
{
    check(!v.id.empty() && !blank(v.privateText), Status::Invalid, "Describe the outcome before saving.");
    check(static_cast<int>(v.kind) >= 0 && static_cast<int>(v.kind) <= 2 &&
          static_cast<int>(v.state) >= 0 && static_cast<int>(v.state) <= 2, Status::Invalid, "Invalid outcome state.");
    validateTarget(c, v.target);
    if (!v.sourceNoteId.empty())
        check(std::any_of(s.notes.begin(), s.notes.end(), [&](const auto &n) { return n.metadata.id == v.sourceNoteId; }),
              Status::NotFound, "Source session note is unavailable.");
    if (!v.sourceSceneId.empty())
        check(availableScene(c, v.sourceSceneId) && std::any_of(s.planEntries.begin(), s.planEntries.end(),
            [&](const auto &e) { return e.storyBeatId == v.sourceSceneId; }), Status::NotFound, "Source scene is unavailable in this session.");
    switch (v.action)
    {
    case Action::RecordOnly: return;
    case Action::NpcAttitude: check(v.target.type == TargetType::Npc && !blank(v.changeText), Status::Invalid, "Choose an NPC and proposed attitude."); return;
    case Action::NpcNote: check(v.target.type == TargetType::Npc && !blank(v.changeText), Status::Invalid, "Choose an NPC and a dialogue note to append."); return;
    case Action::PlaceNote: check(v.target.type == TargetType::Place && !blank(v.changeText), Status::Invalid, "Choose a place and proposed note."); return;
    case Action::ObjectiveState: check(v.target.type == TargetType::Objective && v.desiredStatus >= 0 && v.desiredStatus <= 3, Status::Invalid, "Choose a valid objective state."); return;
    case Action::ClueStatus: check(v.target.type == TargetType::Clue && v.desiredStatus >= 0 && v.desiredStatus <= 2, Status::Invalid, "Choose a valid clue status."); return;
    }
    throw Failure(Status::Invalid, "Unsupported outcome action.");
}
inline std::string field(const Campaign &c, const Change &v)
{
    validateTarget(c, v.target);
    switch (v.action)
    {
    case Action::RecordOnly: return {};
    case Action::NpcAttitude: return entity(c.npcs, v.target.id).attitudeTowardParty;
    case Action::NpcNote: return entity(c.npcs, v.target.id).dialogueNotes;
    case Action::PlaceNote: return entity(c.worldPlaces, v.target.id).notes;
    case Action::ObjectiveState: return std::to_string(static_cast<int>(entity(c.storyObjectives, v.target.id).status));
    case Action::ClueStatus: return std::to_string(static_cast<int>(entity(c.storyClues, v.target.id).status));
    }
    throw Failure(Status::Invalid, "Unsupported outcome action.");
}
inline int statusValue(const std::string &text, int maximum)
{
    check(text.size() == 1 && text[0] >= '0' && text[0] <= '0' + maximum, Status::Invalid, "Invalid reviewed status.");
    return text[0] - '0';
}
inline void setField(Campaign &c, const Change &v, const std::string &text)
{
    switch (v.action)
    {
    case Action::RecordOnly: break;
    case Action::NpcAttitude: { auto &e = entity(c.npcs, v.target.id); e.attitudeTowardParty = text; e.metadata.touch(); break; }
    case Action::NpcNote: { auto &e = entity(c.npcs, v.target.id); e.dialogueNotes = text; e.metadata.touch(); break; }
    case Action::PlaceNote: { auto &e = entity(c.worldPlaces, v.target.id); e.notes = text; e.metadata.touch(); break; }
    case Action::ObjectiveState: { auto &e = entity(c.storyObjectives, v.target.id); e.status = static_cast<StoryObjectiveStatus>(statusValue(text, 3)); e.metadata.touch(); break; }
    case Action::ClueStatus: { auto &e = entity(c.storyClues, v.target.id); e.status = static_cast<StoryClueStatus>(statusValue(text, 2)); e.metadata.touch(); break; }
    }
}
inline Change changeFromBefore(const Outcome &v, const std::string &before)
{
    Change result; result.outcomeId = v.id; result.target = v.target; result.action = v.action;
    // Sharing is an independent reviewed projection and must not disable undo
    // of an otherwise unchanged consequence after the GM edits a recap.
    auto consequence = v; consequence.publicText.clear(); consequence.shareWithPlayers = false;
    result.outcomeSignature = signature(consequence); result.before = before;
    switch (v.action)
    {
    case Action::RecordOnly: break;
    case Action::NpcAttitude: result.after = v.changeText; break;
    case Action::NpcNote: case Action::PlaceNote: result.after = result.before + (result.before.empty() ? "" : "\n") + v.changeText; break;
    case Action::ObjectiveState: case Action::ClueStatus: result.after = std::to_string(v.desiredStatus); break;
    }
    return result;
}
inline Change changeFor(const Campaign &c, const Outcome &v)
{
    Change key; key.target = v.target; key.action = v.action;
    return changeFromBefore(v, field(c, key));
}
inline bool same(const Change &a, const Change &b)
{
    return a.outcomeId == b.outcomeId && a.target.type == b.target.type && a.target.id == b.target.id &&
        a.action == b.action && a.before == b.before && a.after == b.after && a.outcomeSignature == b.outcomeSignature;
}
template<class Mutation> inline Result commit(Campaign &c, const Persist &persist, Mutation mutation)
{
    try
    {
        check(static_cast<bool>(persist), Status::PersistenceFailed, "A save operation is required.");
        Campaign staged = c; auto result = mutation(staged);
        if (!result.success || result.status == Status::NoChange) return result;
        staged.metadata.touch();
        try { if (!persist(staged)) return failure(Status::PersistenceFailed, "Changes could not be saved. Previous state and drafts are retained."); }
        catch (...) { return failure(Status::PersistenceFailed, "Changes could not be saved. Previous state and drafts are retained."); }
        c = std::move(staged); return result;
    }
    catch (const Failure &e) { return failure(e.status, e.what()); }
    catch (const std::exception &e) { return failure(Status::CorruptData, e.what()); }
}
}
