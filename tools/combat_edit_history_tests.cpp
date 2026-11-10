#include "Models/Combat/CombatEditHistory.hpp"
#include "Models/Project/ProjectCommand.hpp"
#include <iostream>
#include <stdexcept>

namespace
{
using namespace CombatEditHistory;
void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
struct Fixture
{
    Campaign live, durable;
    EncounterModel draft;
    bool fail = false, throwSave = false, context = true;
    int saves = 0;
    Fixture()
    {
        ProjectCommandManager::getInstance().clear();
        live.metadata.id = "combat-history-fixture";
        CharacterModel player; player.metadata.id = "player"; player.currentHitPoints = player.maxHitPoints = 20; player.temporaryHitPoints = 3;
        ResourceTrackerModel resource; resource.metadata.id = "resource"; resource.maximum = 5; resource.used = 0;
        player.resourceTrackers.push_back(resource); live.characters.push_back(player);
        CharacterModel unrelated; unrelated.metadata.id = "unrelated"; unrelated.currentHitPoints = 30; live.characters.push_back(unrelated);
        draft.metadata.id = "encounter"; draft.round = 2; draft.combatPhase = CombatPhase::ActiveTurn;
        EncounterParticipantModel actor; actor.metadata.id = "actor"; actor.sourceEntityId = "player"; actor.kind = CombatantKind::Player;
        actor.currentHitPoints = actor.maxHitPoints = 20; actor.temporaryHitPoints = 3; actor.x = actor.y = actor.turnStartX = actor.turnStartY = 1;
        draft.participants.push_back(actor);
        EncounterParticipantModel other; other.metadata.id = "other-actor"; other.kind = CombatantKind::Monster; other.currentHitPoints = 11;
        draft.participants.push_back(other); draft.activeParticipantIndex = 0;
        CombatLogEntryModel log; log.summary = "Original event"; draft.combatLog.push_back(log);
        commitOriginal();
    }
    ~Fixture() { ProjectCommandManager::getInstance().clear(); }
    Snapshot before(Kind kind, const EntityId &resource = {}) { return capture(live, draft, "actor", kind, resource); }
    void commitOriginal() { live.encounters = {draft}; durable = live; }
    void record(const Snapshot &old)
    {
        recordApplied(live, draft, old, [this](Campaign &staged) {
            ++saves;
            if (throwSave) throw std::runtime_error("Injected save exception");
            if (fail) return false;
            durable = staged; return true;
        }, [this] { return context; }, "Fixture combat correction");
    }
};
void healthRoundTrip()
{
    Fixture test; auto &history = ProjectCommandManager::getInstance();
    const auto old = test.before(Kind::Health);
    test.draft.participants[0].currentHitPoints = test.live.characters[0].currentHitPoints = 16;
    test.draft.participants[0].temporaryHitPoints = test.live.characters[0].temporaryHitPoints = 0;
    test.commitOriginal(); test.record(old);
    test.live.characters[1].currentHitPoints = 27;
    test.draft.participants[1].currentHitPoints = test.live.encounters[0].participants[1].currentHitPoints = 8;
    test.draft.combatLog[0].summary = test.live.encounters[0].combatLog[0].summary = "Later annotation";
    require(history.undo(), "Health correction undo failed");
    require(test.live.characters[0].currentHitPoints == 20 && test.live.characters[0].temporaryHitPoints == 3 &&
        test.draft.participants[0].currentHitPoints == 20 && test.durable.characters[0].currentHitPoints == 20, "Player and combat health correction diverged");
    require(test.live.characters[1].currentHitPoints == 27 && test.draft.participants[1].currentHitPoints == 8 &&
        test.draft.combatLog[0].summary == "Later annotation" && test.draft.combatLog.size() == 2, "Health correction reverted unrelated work or replaced logs");
    require(test.draft.combatLog.back().eventType == "Correction" && history.redo(), "Correction log or redo failed");
    require(test.live.characters[0].currentHitPoints == 16 && test.draft.combatLog.size() == 3, "Health redo lost new logs");
}
void failuresAndConflicts()
{
    Fixture test; auto &history = ProjectCommandManager::getInstance();
    const auto old = test.before(Kind::Health);
    test.draft.participants[0].currentHitPoints = test.live.characters[0].currentHitPoints = 10;
    test.commitOriginal(); test.record(old);
    test.fail = true;
    require(!history.undo() && test.live.characters[0].currentHitPoints == 10 && test.draft.combatLog.size() == 1 && history.canUndo(), "Failed correction modified live data/history");
    test.fail = false; test.throwSave = true;
    require(!history.undo() && test.durable.characters[0].currentHitPoints == 10, "Thrown persistence modified durable state");
    test.throwSave = false;
    test.live.characters[0].currentHitPoints = 9;
    const auto saves = test.saves;
    require(!history.undo() && test.saves == saves, "Newer source health was overwritten");
    test.live.characters[0].currentHitPoints = 10;
    require(history.undo(), "Retained correction could not retry");
    test.context = false;
    require(!history.redo() && history.canRedo(), "Selection generation guard was ignored");
}
void resourceRoundTrip()
{
    Fixture test; auto &history = ProjectCommandManager::getInstance();
    const auto old = test.before(Kind::Resource, "resource");
    test.live.characters[0].resourceTrackers[0].used = 3; test.commitOriginal(); test.record(old);
    test.live.characters[0].resourceTrackers[0].notes = "New unrelated resource note";
    require(history.undo() && test.live.characters[0].resourceTrackers[0].used == 0, "Resource undo failed");
    require(test.live.characters[0].resourceTrackers[0].notes == "New unrelated resource note" && history.redo(), "Resource correction replaced unrelated fields");
    test.live.characters[0].resourceTrackers[0].maximum = 6;
    require(!history.undo(), "Changed resource definition was overwritten");
}
void movementRoundTripAndTurnGuard()
{
    Fixture test; auto &history = ProjectCommandManager::getInstance();
    const auto old = test.before(Kind::Movement);
    test.draft.participants[0].x = 3; test.draft.participants[0].y = 4; test.draft.participants[0].movementUsedFeet = 20;
    test.commitOriginal(); test.record(old);
    test.draft.participants[0].currentHitPoints = test.live.encounters[0].participants[0].currentHitPoints = 17;
    test.live.characters[0].currentHitPoints = 17;
    require(history.undo() && test.draft.participants[0].x == 1 && test.draft.participants[0].movementUsedFeet == 0, "Token position/budget undo failed");
    require(test.draft.participants[0].currentHitPoints == 17 && history.redo(), "Movement undo changed health or redo failed");
    ++test.draft.round; ++test.live.encounters[0].round;
    const auto saves = test.saves;
    require(!history.undo() && test.saves == saves && test.draft.participants[0].x == 3, "Correction crossed turn boundary");
}
void identityAndWrapUpGuard()
{
    Fixture test; auto &history = ProjectCommandManager::getInstance();
    const auto old = test.before(Kind::Movement);
    test.draft.participants[0].x = 2; test.commitOriginal(); test.record(old);
    test.live.encounters[0].participants[0].sourceEntityId = "replaced-source";
    require(!history.undo(), "Replaced participant source was overwritten");
    test.live.encounters[0].participants[0].sourceEntityId = "player";
    test.draft.combatPhase = test.live.encounters[0].combatPhase = CombatPhase::WrapUp;
    require(!history.undo(), "Finished combat was changed");
}
void originalSaveFailureThenRetry()
{
    Fixture test; auto &history = ProjectCommandManager::getInstance();
    const auto old = test.before(Kind::Health);
    test.context = false;
    test.draft.participants[0].currentHitPoints = test.live.characters[0].currentHitPoints = 16;
    test.live.encounters = {test.draft}; // Retained draft; durable snapshot still has 20 HP.
    test.record(old);
    require(history.canUndo() && !history.undo() && history.canUndo() && test.saves == 0 &&
        test.durable.characters[0].currentHitPoints == 20 && test.live.characters[0].currentHitPoints == 16,
        "Pending original save allowed undo or lost its correction");
    test.live.characters[1].currentHitPoints = 27;
    test.durable = test.live; // Successful retry commits the retained draft.
    test.context = true;
    require(history.undo() && test.durable.characters[0].currentHitPoints == 20 &&
        test.durable.characters[1].currentHitPoints == 27, "Retried original edit lost undo or unrelated later work");
    require(history.redo() && test.durable.characters[0].currentHitPoints == 16 &&
        test.draft.participants[0].currentHitPoints == 16, "Retried original edit lost redo");
}
void laterEditBeforeOriginalSaveRetry()
{
    Fixture test; auto &history = ProjectCommandManager::getInstance();
    const auto old = test.before(Kind::Health);
    test.context = false;
    test.draft.participants[0].currentHitPoints = test.live.characters[0].currentHitPoints = 16;
    test.live.encounters = {test.draft}; test.record(old);
    test.draft.participants[0].currentHitPoints = test.live.characters[0].currentHitPoints = 15;
    test.live.encounters = {test.draft}; test.durable = test.live; test.context = true;
    require(!history.undo() && history.canUndo() && test.saves == 0 &&
        test.durable.characters[0].currentHitPoints == 15 && test.draft.participants[0].currentHitPoints == 15,
        "Original save retry allowed stale undo to overwrite a later same-field edit");
}
}
int main()
{
    try
    {
        healthRoundTrip(); failuresAndConflicts(); resourceRoundTrip(); movementRoundTripAndTurnGuard(); identityAndWrapUpGuard();
        originalSaveFailureThenRetry(); laterEditBeforeOriginalSaveRetry();
        std::cout << "PASS combat edit history: 7 groups; typed transaction fixtures, no UI/audio\n"; return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
