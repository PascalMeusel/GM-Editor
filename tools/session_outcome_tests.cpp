#include "workflow_test_database.hpp"
#include "Models/Session/SessionOutcomeService.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/StarterCampaignTemplate.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include <SQLiteCpp/SQLiteCpp.h>
#include <iostream>
#include <stdexcept>
#include <algorithm>

namespace
{
using namespace SessionOutcomes;
void check(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
Campaign reload(const EntityId &id)
{
    for (auto c : CampaignRepository::loadAllCampaigns()) if (c.metadata.id == id) return c;
    throw std::runtime_error("Campaign missing after reload");
}
Ledger ledger(const Campaign &c, const EntityId &id)
{
    const auto *s = SessionPlan::findSession(c, id); check(s != nullptr, "Session missing");
    auto r = read(*s); check(r.success, "Ledger unreadable"); return r.ledger;
}
Outcome outcome(const Campaign &c, const EntityId &sid, const EntityId &id)
{
    const auto l = ledger(c, sid);
    for (const auto &v : l.outcomes) if (v.id == id) return v;
    throw std::runtime_error("Outcome missing");
}
struct Fixture
{
    Campaign c;
    EntityId sessionId, sceneId, noteId, npcId, placeId, objectiveId, clueId;
    Fixture()
    {
        c.name = "PRIVATE_CAMPAIGN"; c.ruleSet = std::make_shared<DND5e>();
        NPCModel npc; npc.name = "PRIVATE_NPC"; npc.attitudeTowardParty = "Cautious"; npcId = npc.metadata.id; c.npcs.push_back(npc);
        WorldPlaceModel place; place.name = "PRIVATE_PLACE"; place.notes = "Original place notes"; placeId = place.metadata.id; c.worldPlaces.push_back(place);
        StoryObjectiveModel objective; objective.title = "PRIVATE_OBJECTIVE"; objective.playerVisible = false;
        objectiveId = objective.metadata.id; c.storyObjectives.push_back(objective);
        StoryClueModel clue; clue.title = "PRIVATE_CLUE"; clue.gmTruth = "PRIVATE_TRUTH"; clue.playerVisible = false;
        clueId = clue.metadata.id; c.storyClues.push_back(clue);
        StoryThreadModel thread; thread.title = "PRIVATE_THREAD";
        StoryBeatModel scene; scene.title = "PRIVATE_SCENE"; scene.gmNotes = "PRIVATE_SCENE_NOTES";
        sceneId = scene.metadata.id; thread.beats.push_back(scene); c.storyThreads.push_back(thread);
        SessionModel s; s.title = "PRIVATE_SESSION_TITLE"; s.summary = "PRIVATE_SUMMARY"; s.recap = "PRIVATE_RECAP";
        s.roughNotes = "PRIVATE_ROUGH_NOTES"; s.liveModeActive = true; s.startedAtUtc = "2026-10-03T17:00:00Z";
        SessionPlanEntry entry; entry.storyBeatId = sceneId; s.planEntries.push_back(entry);
        SessionNoteModel note; note.body = "PRIVATE_SOURCE_NOTE"; noteId = note.metadata.id; s.notes.push_back(note);
        sessionId = s.metadata.id; c.sessions.push_back(s); c.sessionIds.push_back(sessionId);
        TimelineEventModel event; event.title = "PRIVATE_TIMELINE"; c.timelineEvents.push_back(event);
    }
    Outcome draft(Action action = Action::RecordOnly, Target target = {}) const
    {
        Outcome v; v.privateText = "PRIVATE_OUTCOME\nUnicode: Ruß; \"quoted\" 🕯 第二場";
        v.sourceNoteId = noteId; v.sourceSceneId = sceneId; v.action = action; v.target = std::move(target); return v;
    }
};
void serialization()
{
    static_assert(static_cast<int>(Action::RecordOnly) == 0 && static_cast<int>(Action::NpcAttitude) == 1 &&
        static_cast<int>(Action::PlaceNote) == 2 && static_cast<int>(Action::ObjectiveState) == 3 &&
        static_cast<int>(Action::ClueStatus) == 4 && static_cast<int>(Action::NpcNote) == 5, "Persisted action IDs changed");
    SessionModel legacy; check(read(legacy).success && read(legacy).ledger.outcomes.empty(), "Legacy data invented outcomes");
    Ledger l; Outcome v; v.privateText = "line\nquote\"slash\\; Ruß 🕯 第二場"; v.publicText = "Only this";
    l.outcomes.push_back(v); const auto encoded = serialize(l);
    const auto decoded = deserialize(encoded); check(decoded.success && serialize(decoded.ledger) == encoded, "Codec lost UTF-8/prose/identity");
    check(!deserialize(encoded + "garbage").success, "Trailing bytes accepted");
    check(!deserialize(encoded.substr(0, encoded.size() - 1)).success, "Truncated payload accepted");
    check(!deserialize("999999999:bad").success, "Unbounded field accepted");
    auto future = encoded; const auto version = future.find("gm-session-outcomes-1"); future[version + 19] = '2';
    check(!deserialize(future).success, "Future version accepted");
    auto unsupported = l; unsupported.outcomes[0].action = static_cast<Action>(6);
    bool refusedAction = false; try { serialize(unsupported); } catch (...) { refusedAction = true; }
    check(refusedAction, "Unknown action beyond NPC notes accepted");
    l.outcomes.push_back(v); bool rejected = false; try { serialize(l); } catch (...) { rejected = true; }
    check(rejected, "Duplicate outcome IDs accepted");
    legacy.metadata.customProperties[StorageKey] = future; const auto before = legacy.metadata.customProperties;
    check(!read(legacy).success && legacy.metadata.customProperties == before, "Corrupt property was overwritten");
}
void npcNotes()
{
    Fixture f; int saves = 0;
    const Persist save = [&](Campaign &c) { CampaignRepository::saveCampaign(c); ++saves; return true; };
    const std::string original = "Original dialogue\r\nPrivate: Ruß 🕯 第二場";
    f.c.npcs[0].dialogueNotes = original; CampaignRepository::saveCampaign(f.c);
    auto note = f.draft(Action::NpcNote, {TargetType::Npc, f.npcId});
    note.changeText = "Mara promised a map.\nKeep the original wording.";
    auto invalid = note; invalid.changeText = " \r\n\t";
    check(capture(f.c, f.sessionId, invalid, save).status == Status::Invalid, "Blank NPC note accepted");
    invalid = note; invalid.target = {TargetType::Place, f.placeId};
    check(capture(f.c, f.sessionId, invalid, save).status == Status::Invalid, "NPC note accepted a place target");
    invalid = note; invalid.target.id = "missing-npc";
    check(capture(f.c, f.sessionId, invalid, save).status == Status::NotFound && saves == 0, "Missing NPC note target accepted");
    check(capture(f.c, f.sessionId, note, save).success && f.c.npcs[0].dialogueNotes == original, "Capture changed NPC notes before review");
    auto second = note; second.id = createEntityId("session_outcome"); second.changeText = "Another exact note.";
    check(capture(f.c, f.sessionId, second, save).success, "Second NPC note capture failed");
    f.c = reload(f.c.metadata.id);
    check(outcome(f.c, f.sessionId, note.id).action == Action::NpcNote, "NPC note action changed during SQLite reload");
    const auto review = reviewApply(f.c, f.sessionId, {note.id, second.id});
    const auto first = original + "\n" + note.changeText, both = first + "\n" + second.changeText;
    check(review.success && review.receipt.changes[0].before == original && review.receipt.changes[0].after == first &&
        review.receipt.changes[1].before == first && review.receipt.changes[1].after == both, "NPC note review lost exact append order");
    f.c.npcs[0].dialogueNotes = "Newer dialogue edit";
    check(apply(f.c, review, save).status == Status::Conflict && f.c.npcs[0].dialogueNotes == "Newer dialogue edit",
        "NPC note apply overwrote a newer note");
    f.c.npcs[0].dialogueNotes = original;
    auto removed = f.c; removed.npcs.clear();
    check(apply(removed, review, save).status == Status::NotFound, "NPC note apply accepted a removed NPC");
    SQLite::Database db(ApplicationPaths::databasePath().string(), SQLite::OPEN_READWRITE);
    db.exec("CREATE TRIGGER reject_npc_notes BEFORE INSERT ON campaign_entity_properties WHEN NEW.property_key = 'gm.session.outcomes.v1' BEGIN SELECT RAISE(ABORT,'injected NPC note save failure'); END;");
    check(apply(f.c, review, save).status == Status::PersistenceFailed && f.c.npcs[0].dialogueNotes == original &&
        reload(f.c.metadata.id).npcs[0].dialogueNotes == original && ledger(reload(f.c.metadata.id), f.sessionId).receipts.empty(),
        "NPC note SQL failure leaked field or receipt changes");
    db.exec("DROP TRIGGER reject_npc_notes;");
    const auto savesBefore = saves;
    check(apply(f.c, review, save).success && saves == savesBefore + 1, "NPC note batch did not use one save");
    check(f.c.npcs[0].dialogueNotes == both && f.c.npcs[0].attitudeTowardParty == "Cautious" &&
        SessionPlan::findSession(f.c, f.sessionId)->notes[0].body == "PRIVATE_SOURCE_NOTE", "NPC append changed attitude/source notes");
    check(apply(f.c, review, save).status == Status::NoChange && saves == savesBefore + 1 && f.c.npcs[0].dialogueNotes == both,
        "NPC note retry appended twice");
    f.c = reload(f.c.metadata.id);
    check(f.c.npcs[0].dialogueNotes == both && ledger(f.c, f.sessionId).receipts[0].changes[0].action == Action::NpcNote,
        "NPC note or receipt failed persistence roundtrip");
    check(!reviewRecap(f.c, f.sessionId).success, "Private NPC note silently became a player recap");
    check(setPlayerText(f.c, f.sessionId, note.id, "", false, "Mara offers a map.", true, save).success,
        "Public text could not be set on applied NPC note");
    const auto publicReview = reviewRecap(f.c, f.sessionId);
    check(publicReview.success && markdown(publicReview.document).find("Original dialogue") == std::string::npos &&
        markdown(publicReview.document).find("Keep the original") == std::string::npos, "NPC notes leaked into public projection");
    f.c.npcs[0].dialogueNotes += "\nLater work";
    check(undo(f.c, f.sessionId, review.receipt.id, save).status == Status::Conflict, "NPC note undo overwrote later work");
    f.c.npcs[0].dialogueNotes = both;
    check(!undo(f.c, f.sessionId, review.receipt.id, [](Campaign &) { return false; }).success && f.c.npcs[0].dialogueNotes == both,
        "Failed NPC note undo changed memory");
    check(undo(f.c, f.sessionId, review.receipt.id, save).success && reload(f.c.metadata.id).npcs[0].dialogueNotes == original &&
        isRecapCurrent(f.c, publicReview), "NPC note reverse undo lost original bytes or player text");
    f.c.npcs[0].dialogueNotes.clear();
    const auto empty = reviewApply(f.c, f.sessionId, {note.id});
    check(empty.success && empty.receipt.changes[0].before.empty() && empty.receipt.changes[0].after == note.changeText,
        "First NPC note inserted a leading newline");
}
void servicesAndPersistence()
{
    Fixture f; int saves = 0;
    const Persist save = [&](Campaign &c) { CampaignRepository::saveCampaign(c); ++saves; return true; };
    const Persist fail = [](Campaign &) { return false; };
    const Persist throwing = [](Campaign &) -> bool { throw std::runtime_error("injected"); };
    CampaignRepository::saveCampaign(f.c);
    auto npc = f.draft(Action::NpcAttitude, {TargetType::Npc, f.npcId}); npc.changeText = "Trusts the party";
    npc.publicText = "Mara welcomes the group."; npc.shareWithPlayers = true;
    check(!capture(f.c, f.sessionId, npc, fail).success && ledger(f.c, f.sessionId).outcomes.empty(), "Failed capture leaked");
    check(capture(f.c, f.sessionId, npc, save).success, "Capture failed");
    const int capturedSaves = saves;
    check(capture(f.c, f.sessionId, npc, save).status == Status::NoChange && saves == capturedSaves, "Capture retry duplicated");
    auto collision = npc; collision.privateText += "new";
    check(capture(f.c, f.sessionId, collision, save).status == Status::Conflict, "Identity collision accepted");
    auto place = f.draft(Action::PlaceNote, {TargetType::Place, f.placeId}); place.changeText = "The bridge was destroyed.";
    auto objective = f.draft(Action::ObjectiveState, {TargetType::Objective, f.objectiveId}); objective.desiredStatus = 2;
    auto clue = f.draft(Action::ClueStatus, {TargetType::Clue, f.clueId}); clue.desiredStatus = 1;
    for (const auto &v : {place, objective, clue}) check(capture(f.c, f.sessionId, v, save).success, "Additional capture failed");
    const std::vector<EntityId> ids{npc.id, place.id, objective.id, clue.id};
    check(ledger(reload(f.c.metadata.id), f.sessionId).outcomes.size() == 4, "Outcomes missing after real SQLite reload");
    const auto review = reviewApply(f.c, f.sessionId, ids); check(review.success && review.receipt.changes.size() == 4, "Batch review failed");
    f.c.npcs[0].attitudeTowardParty = "Changed elsewhere";
    check(apply(f.c, review, save).status == Status::Conflict && f.c.worldPlaces[0].notes == "Original place notes", "Stale review applied partially");
    f.c.npcs[0].attitudeTowardParty = "Cautious";
    auto forged = review; forged.receipt.changes[0].after = "Unreviewed value";
    check(apply(f.c, forged, save).status == Status::Conflict, "Modified review accepted");
    check(!apply(f.c, review, throwing).success && f.c.npcs[0].attitudeTowardParty == "Cautious", "Thrown persistence failure leaked");
    SQLite::Database db(ApplicationPaths::databasePath().string(), SQLite::OPEN_READWRITE);
    db.exec("CREATE TRIGGER reject_outcomes BEFORE INSERT ON campaign_entity_properties WHEN NEW.property_key = 'gm.session.outcomes.v1' BEGIN SELECT RAISE(ABORT,'injected outcome save failure'); END;");
    check(apply(f.c, review, save).status == Status::PersistenceFailed, "SQLite injection did not fail");
    const auto durableBefore = reload(f.c.metadata.id);
    check(durableBefore.npcs[0].attitudeTowardParty == "Cautious" && durableBefore.worldPlaces[0].notes == "Original place notes" &&
        ledger(f.c, f.sessionId).receipts.empty(), "Atomic save failure changed earlier rows or receipt");
    db.exec("DROP TRIGGER reject_outcomes;");
    check(apply(f.c, review, save).success, "Reviewed apply failed");
    check(f.c.npcs[0].attitudeTowardParty == "Trusts the party" && f.c.worldPlaces[0].notes == "Original place notes\nThe bridge was destroyed.", "Wrong world changes");
    check(f.c.storyObjectives[0].status == StoryObjectiveStatus::Completed && f.c.storyClues[0].status == StoryClueStatus::Discovered &&
        !f.c.storyObjectives[0].playerVisible && !f.c.storyClues[0].playerVisible, "Status apply implicitly changed visibility");
    const auto originalNote = SessionPlan::findSession(f.c, f.sessionId)->notes[0].body;
    check(originalNote == "PRIVATE_SOURCE_NOTE", "Capture/apply changed original notes");
    const auto after = serialize(ledger(f.c, f.sessionId)); const int appliedSaves = saves;
    check(apply(f.c, review, save).status == Status::NoChange && saves == appliedSaves && serialize(ledger(f.c, f.sessionId)) == after, "Apply retry duplicated changes");
    f.c = reload(f.c.metadata.id);
    f.c.npcs[0].attitudeTowardParty = "Later relationship";
    check(undo(f.c, f.sessionId, review.receipt.id, save).status == Status::Conflict &&
        f.c.storyObjectives[0].status == StoryObjectiveStatus::Completed, "Undo overwrote newer work or partially applied");
    f.c.npcs[0].attitudeTowardParty = "Trusts the party"; f.c.description = "Unrelated later edit";
    check(!undo(f.c, f.sessionId, review.receipt.id, fail).success && !ledger(f.c, f.sessionId).receipts[0].undone, "Failed undo moved receipt");
    check(undo(f.c, f.sessionId, review.receipt.id, save).success && f.c.npcs[0].attitudeTowardParty == "Cautious" &&
        f.c.worldPlaces[0].notes == "Original place notes" && f.c.description == "Unrelated later edit", "Undo did not restore scoped fields");
    check(undo(f.c, f.sessionId, review.receipt.id, save).status == Status::NoChange, "Undo retry failed");
    auto replacement = npc; replacement.privateText = "PRIVATE_EDITED";
    check(update(f.c, f.sessionId, npc, replacement, save).success, "Draft edit failed");
    check(update(f.c, f.sessionId, npc, replacement, save).status == Status::Conflict, "Stale draft accepted");
    auto missing = f.draft(Action::PlaceNote, {TargetType::Place, "missing"}); missing.changeText = "x";
    check(!capture(f.c, f.sessionId, missing, save).success, "Missing target accepted");
    auto otherPlace = place; otherPlace.id = createEntityId("session_outcome"); otherPlace.changeText = "A ferry now crosses here.";
    check(capture(f.c, f.sessionId, otherPlace, save).success, "Second place note capture failed");
    const auto sameTarget = reviewApply(f.c, f.sessionId, {place.id, otherPlace.id});
    check(sameTarget.success && apply(f.c, sameTarget, save).success && undo(f.c, f.sessionId, sameTarget.receipt.id, save).success &&
        f.c.worldPlaces[0].notes == "Original place notes", "Sequential same-field batch or reverse undo failed");
    const auto badBefore = SessionPlan::findSession(f.c, f.sessionId)->metadata.customProperties;
    SessionPlan::findSession(f.c, f.sessionId)->metadata.customProperties[StorageKey] = "corrupt";
    const auto bad = capture(f.c, f.sessionId, f.draft(), save);
    check(bad.status == Status::CorruptData && SessionPlan::findSession(f.c, f.sessionId)->metadata.customProperties.at(StorageKey) == "corrupt", "Corrupt ledger replaced");
    SessionPlan::findSession(f.c, f.sessionId)->metadata.customProperties = badBefore;
}
void carryAndPrivacy()
{
    Fixture f; int saves = 0; const Persist save = [&](Campaign &) { ++saves; return true; };
    const Persist fail = [](Campaign &) { return false; };
    auto v = f.draft(); v.kind = Kind::OpenHook; v.publicText = "Public <script>alert('x')</script> & [link](bad)"; v.shareWithPlayers = true;
    check(capture(f.c, f.sessionId, v, save).success, "Capture failed");
    CarryForwardRequest request; request.sourceSessionId = f.sessionId; request.newSessionTitle = "Second session";
    request.outcomeIds = {v.id}; request.sceneIds = {f.sceneId}; const auto sessionsBefore = f.c.sessions.size();
    check(!carryForward(f.c, request, fail).success && f.c.sessions.size() == sessionsBefore, "Failed carry created a session");
    const int before = saves; const auto carried = carryForward(f.c, request, save);
    check(carried.success && saves == before + 1 && f.c.sessions.size() == sessionsBefore + 1, "New session carry was not atomic");
    const auto next = carried.targetSessionId; const auto l = ledger(f.c, next);
    check(l.followUps.size() == 1 && l.followUps[0].privateText == v.privateText &&
        SessionPlan::findSession(f.c, next)->planEntries[0].storyBeatId == f.sceneId, "Carry lost snapshot or canonical scene link");
    check(carryForward(f.c, request, save).status == Status::NoChange && f.c.sessions.size() == sessionsBefore + 1, "New session retry duplicated destination");
    auto changedRequest = request; changedRequest.newSessionTitle += " altered";
    check(carryForward(f.c, changedRequest, save).status == Status::Conflict, "Changed idempotent request accepted");
    request.targetSessionId = next;
    check(carryForward(f.c, request, save).status == Status::NoChange, "Existing destination duplicated followups");
    request.targetSessionId = f.sessionId; check(!carryForward(f.c, request, save).success, "Active/source destination accepted");
    SessionPlan::findSession(f.c, next)->startedAtUtc = "2026-10-04"; request.targetSessionId = next;
    check(!carryForward(f.c, request, save).success, "Started destination accepted");
    SessionPlan::findSession(f.c, next)->startedAtUtc.clear();
    check(setFollowUpCompleted(f.c, next, l.followUps[0].id, true, save).success && ledger(f.c, next).followUps[0].completed, "Follow-up state not saved");
    auto modified = v; modified.privateText = "CHANGED PRIVATE SOURCE";
    check(update(f.c, f.sessionId, v, modified, save).success && ledger(f.c, next).followUps[0].privateText == v.privateText, "Carry snapshot changed retroactively");
    auto review = reviewRecap(f.c, f.sessionId); check(review.success && isRecapCurrent(f.c, review), "Public recap review failed");
    const auto md = markdown(review.document), page = html(review.document);
    check(md.find("PRIVATE") == std::string::npos && page.find("PRIVATE") == std::string::npos && page.find("CHANGED") == std::string::npos, "Private fields leaked into recap");
    check(page.find("<script>") == std::string::npos && page.find("&lt;script&gt;") != std::string::npos &&
        md.find("&lt;script&gt;") != std::string::npos, "Markup was not escaped");
    auto tampered = review; tampered.document.intro = "Injected"; check(!isRecapCurrent(f.c, tampered), "Tampered DTO review accepted");
    auto again = modified; again.privateText += " private only";
    check(update(f.c, f.sessionId, modified, again, save).success && isRecapCurrent(f.c, review), "Private-only edit altered public projection");
    auto hidden = again; hidden.shareWithPlayers = false;
    check(update(f.c, f.sessionId, again, hidden, save).success && !isRecapCurrent(f.c, review) && !reviewRecap(f.c, f.sessionId).success, "Opt-out failed closed");
    hidden.shareWithPlayers = true; hidden.publicText.clear();
    const auto stored = outcome(f.c, f.sessionId, v.id);
    check(update(f.c, f.sessionId, stored, hidden, save).success && !reviewRecap(f.c, f.sessionId).success, "Blank public text fell back to private");
    check(setPublicRecap(f.c, f.sessionId, "Player title", "Only deliberate introduction", save).success, "Public prose save failed");
    review = reviewRecap(f.c, f.sessionId); check(review.success && review.document.items.empty(), "Explicit intro incorrectly pulled private content");
    check(reject(f.c, f.sessionId, outcome(f.c, f.sessionId, v.id), save).success, "Reject failed");
    CarryForwardRequest rejected; rejected.sourceSessionId = f.sessionId; rejected.targetSessionId = next; rejected.outcomeIds = {v.id};
    check(!carryForward(f.c, rejected, save).success, "Rejected outcome carried");
}
void clone()
{
    Fixture f; const Persist save = [](Campaign &) { return true; };
    auto v = f.draft(Action::NpcAttitude, {TargetType::Npc, f.npcId}); v.changeText = "Friendly";
    auto note = f.draft(Action::NpcNote, {TargetType::Npc, f.npcId}); note.changeText = "PRIVATE_NPC_DIALOGUE";
    check(capture(f.c, f.sessionId, v, save).success, "Clone fixture capture failed");
    check(capture(f.c, f.sessionId, note, save).success, "Clone NPC note capture failed");
    const auto review = reviewApply(f.c, f.sessionId, {v.id, note.id}); check(apply(f.c, review, save).success, "Clone fixture apply failed");
    CarryForwardRequest request; request.sourceSessionId = f.sessionId; request.newSessionTitle = "Next"; request.outcomeIds = {v.id};
    const auto carried = carryForward(f.c, request, save); check(carried.success, "Clone fixture carry failed");
    IdRemap ids{{f.sessionId, "cloned-session"}, {carried.targetSessionId, "cloned-next"}, {f.sceneId, "cloned-scene"},
        {f.noteId, "cloned-note"}, {f.npcId, "cloned-npc"}};
    assignCloneIds(f.c, ids); const auto clonedReceipt = ids.at(review.receipt.id);
    f.c.sessions[0].metadata.id = "cloned-session"; f.c.sessions[0].notes[0].metadata.id = "cloned-note";
    f.c.sessions[0].planEntries[0].storyBeatId = "cloned-scene"; f.c.sessions[1].metadata.id = "cloned-next";
    f.c.storyThreads[0].beats[0].metadata.id = "cloned-scene"; f.c.npcs[0].metadata.id = "cloned-npc";
    remapCloneReferences(f.c, ids);
    const auto l = ledger(f.c, "cloned-session"); const auto next = ledger(f.c, "cloned-next");
    check(l.outcomes[0].id == ids.at(v.id) && l.outcomes[0].sourceNoteId == "cloned-note" &&
        l.outcomes[0].sourceSceneId == "cloned-scene" && l.outcomes[0].target.id == "cloned-npc", "Clone left old outcome references");
    check(next.followUps[0].sourceSessionId == "cloned-session" && next.followUps[0].sourceOutcomeId == ids.at(v.id), "Clone lost carry provenance");
    check(l.outcomes[1].id == ids.at(note.id) && l.outcomes[1].action == Action::NpcNote &&
        l.outcomes[1].target.id == "cloned-npc", "Clone left old NPC note identity/action");
    check(undo(f.c, "cloned-session", clonedReceipt, save).success && f.c.npcs[0].attitudeTowardParty == "Cautious" &&
        f.c.npcs[0].dialogueNotes.empty(), "Clone did not remap attitude/note receipt signatures");
}
void publicEditsAfterApply()
{
    Fixture f; const Persist save = [](Campaign &) { return true; }; const Persist fail = [](Campaign &) { return false; };
    auto v = f.draft(Action::NpcAttitude, {TargetType::Npc, f.npcId}); v.changeText = "Friendly";
    v.publicText = "Old public text"; v.shareWithPlayers = true;
    check(capture(f.c, f.sessionId, v, save).success, "Public edit fixture capture failed");
    const auto review = reviewApply(f.c, f.sessionId, {v.id}); check(apply(f.c, review, save).success, "Public edit fixture apply failed");
    const auto recap = reviewRecap(f.c, f.sessionId);
    check(!setPlayerText(f.c, f.sessionId, v.id, v.publicText, true, "New public text", true, fail).success &&
        isRecapCurrent(f.c, recap), "Failed public edit changed saved projection");
    check(setPlayerText(f.c, f.sessionId, v.id, v.publicText, true, "New public text", true, save).success &&
        !isRecapCurrent(f.c, recap) && recap.document.items[0].text == "Old public text", "Applied outcome public edit or immutable review failed");
    check(setPlayerText(f.c, f.sessionId, v.id, v.publicText, true, "Stale write", true, save).status == Status::Conflict,
        "Stale player draft overwrote newer text");
    check(f.c.npcs[0].attitudeTowardParty == "Friendly" && outcome(f.c, f.sessionId, v.id).privateText == v.privateText,
        "Public edit changed consequence or private source");
    check(undo(f.c, f.sessionId, review.receipt.id, save).success && f.c.npcs[0].attitudeTowardParty == "Cautious" &&
        outcome(f.c, f.sessionId, v.id).publicText == "New public text", "Public recap edits blocked or were lost by consequence undo");
}
void starter()
{
    for (const auto language : {StarterCampaignTemplate::Language::English, StarterCampaignTemplate::Language::German})
    {
        Campaign c; StarterCampaignTemplate::apply(c, language);
        const auto sid = c.sessions[0].metadata.id; const auto l = ledger(c, sid);
        check(c.sessions.size() == 1 && l.outcomes.size() == 2 && l.outcomes[0].state == State::Pending &&
            !l.outcomes[0].shareWithPlayers && !l.outcomes[1].shareWithPlayers, "Starter imposed or published example outcomes");
        const Persist save = [](Campaign &) { return true; };
        check(c.npcs[0].attitudeTowardParty != l.outcomes[0].changeText, "Starter applied its example automatically");
        const auto review = reviewApply(c, sid, {l.outcomes[0].id});
        check(review.success && apply(c, review, save).success, "Starter consequence cannot be applied");
        CarryForwardRequest request; request.sourceSessionId = sid; request.newSessionTitle = "Session two";
        request.outcomeIds = {l.outcomes[1].id};
        const auto result = carryForward(c, request, save);
        check(result.success && c.sessions.size() == 2 && ledger(c, result.targetSessionId).followUps.size() == 1 &&
            ledger(c, sid).outcomes.size() == 2, "Starter cannot demonstrate a second-session consequence without losing source");
    }
}
}
int main()
{
    try { isolateWorkflowDatabase(); serialization(); npcNotes(); servicesAndPersistence(); carryAndPrivacy(); clone(); publicEditsAfterApply(); starter(); std::cout << "Session outcomes passed\n"; return 0; }
    catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
