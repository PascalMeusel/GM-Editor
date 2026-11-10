#include "Models/Project/CampaignEditHistory.hpp"
#include "Models/Project/ProjectCommand.hpp"
#include "Models/Project/ProjectEventBus.hpp"
#include "Models/Project/WorldInspectorHistory.hpp"
#include "Models/Map/MapStructuredCreation.hpp"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace
{
void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
void isolate(const char *key, const std::string &value)
{
#ifdef _WIN32
    require(_putenv_s(key, value.c_str()) == 0, "Fixture environment failed");
#else
    require(setenv(key, value.c_str(), 1) == 0, "Fixture environment failed");
#endif
}
struct Fixture
{
    Campaign live, durable;
    bool fail = false, throwSave = false;
    int saves = 0;
    SessionPlan::Persist persist;
    Fixture()
    {
        ProjectCommandManager::getInstance().clear();
        live.metadata.id = "history-fixture";
        live.name = "History fixture";
        SessionModel session; session.metadata.id = "session";
        StoryThreadModel thread; thread.metadata.id = "thread";
        for (const auto *id : {"a", "b", "c"})
        {
            StoryBeatModel scene; scene.metadata.id = std::string("scene-") + id;
            scene.title = id; thread.beats.push_back(scene);
            SessionPlanEntry entry; entry.metadata.id = id; entry.storyBeatId = scene.metadata.id;
            session.planEntries.push_back(entry);
        }
        live.sessions.push_back(session); live.storyThreads.push_back(thread); durable = live;
        persist = [this](Campaign &staged) {
            ++saves;
            if (throwSave) throw std::runtime_error("Injected persistence exception");
            if (fail) return false;
            durable = staged;
            return true;
        };
    }
    ~Fixture() { ProjectCommandManager::getInstance().clear(); }
};

void managerFailures()
{
    auto &history = ProjectCommandManager::getInstance(); history.clear();
    bool failUndo = true, failRedo = false; int value = 1;
    require(history.recordApplied(std::make_unique<LambdaProjectCommand>("Recorded", [&] {
        if (failRedo) throw std::runtime_error("redo failure"); value = 1;
    }, [&] { if (failUndo) throw std::runtime_error("undo failure"); value = 0; })), "Could not record committed command");
    require(value == 1 && history.undoLabel() == "Recorded", "recordApplied executed mutation");
    require(!history.undo() && history.canUndo() && !history.canRedo() && value == 1, "Failed undo moved stacks");
    require(history.lastError() == "undo failure", "Undo failure missing from UI error");
    failUndo = false; require(history.undo() && value == 0, "Retry undo failed");
    failRedo = true;
    require(!history.redo() && history.canRedo() && !history.canUndo() && value == 0, "Failed redo moved stacks");
    failRedo = false; require(history.redo() && value == 1 && history.lastError().empty(), "Retry redo failed");
    history.execute(std::make_unique<LambdaProjectCommand>("Fail execute", [] { throw std::runtime_error("execute failure"); }, [] {}));
    require(history.undoLabel() == "Recorded" && history.lastError() == "execute failure", "Failed execute entered history");
    history.clear(); require(!history.canUndo() && !history.canRedo() && history.lastError().empty(), "Campaign reset did not clear history");
}

void planOrderAndUnrelatedChanges()
{
    Fixture test; auto &history = ProjectCommandManager::getInstance();
    require(CampaignEditHistory::editPlan(test.live, "session", test.persist, "Move scene", [&] {
        return SessionPlan::moveEntry(test.live, "session", "a", 1, test.persist);
    }).applied(), "Plan reorder failed");
    test.live.description = "Unrelated campaign change";
    SessionPlan::findEntry(test.live.sessions.front(), "c")->prepNote = "Newer unrelated note";
    require(history.undo(), "Plan undo failed");
    require(test.live.sessions.front().planEntries.front().metadata.id == "a", "Wrong restored order");
    require(test.live.description == "Unrelated campaign change" &&
        SessionPlan::findEntry(test.live.sessions.front(), "c")->prepNote == "Newer unrelated note", "Order undo overwrote unrelated edits");
    require(test.durable.sessions.front().planEntries.front().metadata.id == "a", "Undo was not persisted");
    require(history.redo() && test.live.sessions.front().planEntries.front().metadata.id == "b", "Plan redo failed");
    std::swap(test.live.sessions.front().planEntries[1], test.live.sessions.front().planEntries[2]);
    const auto saves = test.saves;
    require(!history.undo() && test.saves == saves && history.canUndo(), "Newer reorder was overwritten");
}

void planFailureAndActiveEntry()
{
    Fixture test; auto &history = ProjectCommandManager::getInstance();
    const auto before = test.live.sessions.front().planEntries;
    test.live.sessions.front().planEntries[0].prepNote = "Edited note";
    CampaignEditHistory::recordPlanEntries(test.live, "session", before, test.live.sessions.front().planEntries, test.persist, "Note");
    test.fail = true;
    require(!history.undo() && test.live.sessions.front().planEntries[0].prepNote == "Edited note", "Failed save changed live plan");
    require(history.canUndo() && !history.canRedo(), "Failed save lost undo command");
    test.fail = false; test.throwSave = true;
    require(!history.undo() && history.canUndo(), "Thrown save lost undo command");
    test.throwSave = false;
    require(history.undo() && test.live.sessions.front().planEntries[0].prepNote.empty(), "Undo retry failed");
    require(history.redo(), "Redo after save retry failed");
    test.live.sessions.front().planEntries[0].prepNote = "Conflicting newer note";
    const auto saves = test.saves;
    require(!history.undo() && test.saves == saves, "Conflict persisted anything");
    history.clear();
    StoryBeatModel scene; scene.metadata.id = "new-scene"; test.live.storyThreads.front().beats.push_back(scene);
    auto result = CampaignEditHistory::editPlan(test.live, "session", test.persist, "Add scene", [&] {
        return SessionPlan::addScene(test.live, "session", scene.metadata.id, test.persist);
    });
    require(result.applied(), "Scene add failed");
    test.live.sessions.front().liveModeActive = true;
    test.live.sessions.front().liveState.activePlanEntryId = result.entryId;
    require(!history.undo() && SessionPlan::findEntry(test.live.sessions.front(), result.entryId), "Undo removed active entry");
    test.live.sessions.front().liveState.activePlanEntryId = "a";
    require(history.undo() && !SessionPlan::findEntry(test.live.sessions.front(), result.entryId), "Inactive added entry could not be undone");
    test.live.sessions.front().endedAtUtc = "2026-10-03T12:00:00Z";
    require(!history.redo(), "History changed ended session");
}

void objectiveAndClueChanges()
{
    Fixture test; auto &history = ProjectCommandManager::getInstance();
    StoryObjectiveModel goal; goal.metadata.id = "goal"; goal.title = "Original";
    test.live.storyObjectives.push_back(goal);
    auto before = test.live.storyObjectives;
    test.live.storyObjectives[0].title = "Edited";
    CampaignEditHistory::recordObjectives(test.live, before, test.live.storyObjectives, test.persist, "Edit objective");
    StoryObjectiveModel other; other.metadata.id = "other"; other.title = "Unrelated";
    test.live.storyObjectives.push_back(other);
    require(history.undo() && test.live.storyObjectives.size() == 2 && test.live.storyObjectives[0].title == "Original", "Objective undo lost unrelated record");
    require(history.redo() && test.live.storyObjectives[0].title == "Edited", "Objective redo failed");
    before = test.live.storyObjectives; test.live.storyObjectives.erase(test.live.storyObjectives.begin());
    CampaignEditHistory::recordObjectives(test.live, before, test.live.storyObjectives, test.persist, "Delete objective");
    require(history.undo() && test.live.storyObjectives[0].metadata.id == "goal", "Deleted objective identity was not restored");
    const auto cluesBefore = test.live.storyClues;
    StoryClueModel clue; clue.metadata.id = "clue"; clue.title = "Private clue"; clue.gmTruth = "Hidden truth";
    test.live.storyClues.push_back(clue);
    CampaignEditHistory::recordClues(test.live, cluesBefore, test.live.storyClues, test.persist, "Add clue");
    require(!history.canRedo(), "New recorded edit did not clear redo");
    require(history.undo() && test.live.storyClues.empty(), "Clue creation undo failed");
    require(history.redo() && test.live.storyClues[0].gmTruth == "Hidden truth", "Clue redo lost fields");
    const auto editedBefore = test.live.storyClues;
    test.live.storyClues[0].playerVisible = true;
    CampaignEditHistory::recordClues(test.live, editedBefore, test.live.storyClues, test.persist, "Reveal clue");
    require(history.undo() && !test.live.storyClues[0].playerVisible, "Clue visibility undo failed");
    test.live.metadata.id = "different-campaign";
    const auto saves = test.saves;
    require(!history.redo() && test.saves == saves, "Command crossed campaign identity");
}

void sceneLinksAreScoped()
{
    Fixture test; auto &history = ProjectCommandManager::getInstance();
    auto *scene = SessionPlan::findScene(test.live, "scene-a");
    const auto before = scene->links;
    StoryLinkModel link; link.metadata.id = "link"; link.targetEntityId = "map"; link.targetEntityType = "Map";
    scene->links.push_back(link);
    CampaignEditHistory::recordSceneLinks(test.live, "scene-a", before, scene->links, test.persist, "Link map");
    scene->summary = "Newer authored text";
    require(history.undo(), "Link undo failed");
    scene = SessionPlan::findScene(test.live, "scene-a");
    require(scene->links.empty() && scene->summary == "Newer authored text", "Link undo replaced authored scene text");
    require(history.redo() && SessionPlan::findScene(test.live, "scene-a")->links.front().metadata.id == "link", "Link redo lost identity");
}

template<class T> std::vector<EntityId> ids(const std::vector<T> &values)
{
    std::vector<EntityId> result;
    for (const auto &value : values) result.push_back(value.metadata.id);
    return result;
}

void middleRemovalKeepsSceneOrder()
{
    Fixture test; auto &history = ProjectCommandManager::getInstance();
    require(CampaignEditHistory::editPlan(test.live, "session", test.persist, "Remove scene", [&] {
        return SessionPlan::removeEntry(test.live, "session", "b", test.persist);
    }).applied(), "Middle scene removal failed");
    SessionPlan::findEntry(test.live.sessions[0], "c")->prepNote = "Keep newer preparation";
    require(history.undo() && ids(test.live.sessions[0].planEntries) == std::vector<EntityId>{"a", "b", "c"},
        "Middle scene undo restored the wrong order");
    require(test.live.sessions[0].planEntries[2].prepNote == "Keep newer preparation", "Scene undo replaced unrelated preparation");
    require(history.redo() && ids(test.durable.sessions[0].planEntries) == std::vector<EntityId>{"a", "c"},
        "Middle scene redo did not persist the original removal");
    std::swap(test.live.sessions[0].planEntries[0], test.live.sessions[0].planEntries[1]);
    const auto saves = test.saves;
    require(!history.undo() && test.saves == saves, "Middle scene undo overwrote a newer sequence");
    history.clear();
    auto *scene = SessionPlan::findScene(test.live, "scene-a");
    for (const auto *id : {"left", "middle", "right"})
    { StoryLinkModel link; link.metadata.id = id; link.label = id; scene->links.push_back(link); }
    const auto before = scene->links; scene->links.erase(scene->links.begin() + 1);
    CampaignEditHistory::recordSceneLinks(test.live, "scene-a", before, scene->links, test.persist, "Remove scene link");
    scene->links.back().label = "Keep newer link text"; scene->summary = "Keep newer scene text";
    require(history.undo(), "Middle link undo failed");
    scene = SessionPlan::findScene(test.live, "scene-a");
    require(ids(scene->links) == std::vector<EntityId>{"left", "middle", "right"} &&
        scene->links.back().label == "Keep newer link text" && scene->summary == "Keep newer scene text",
        "Middle link undo lost order or unrelated text");
    require(history.redo() && ids(SessionPlan::findScene(test.durable, "scene-a")->links) == std::vector<EntityId>{"left", "right"},
        "Middle link redo lost order");
    scene = SessionPlan::findScene(test.live, "scene-a"); std::swap(scene->links[0], scene->links[1]);
    require(!history.undo(), "Link undo overwrote a newer sequence");
}

void scopedPositionsAndRegistration()
{
    Fixture test; auto &history = ProjectCommandManager::getInstance();
    for (const auto *id : {"a", "b", "c", "d"})
    { StoryObjectiveModel value; value.metadata.id = id; value.title = id; test.live.storyObjectives.push_back(value); }
    const auto before = test.live.storyObjectives;
    test.live.storyObjectives.erase(test.live.storyObjectives.begin() + 1, test.live.storyObjectives.begin() + 3);
    test.live.storyObjectives[0].title = "Untracked newer text";
    const std::set<EntityId> tracked{"b", "c"};
    bool acknowledged = true;
    history.execute(std::make_unique<LambdaProjectCommand>("Outer command", [&] {
        acknowledged = CampaignEditHistory::recordObjectives(test.live, before, test.live.storyObjectives, test.persist, "Remove objectives", &tracked);
    }, [] {}));
    require(!acknowledged, "Busy command manager falsely acknowledged a history record");
    history.clear();
    require(CampaignEditHistory::recordObjectives(test.live, before, test.live.storyObjectives, test.persist, "Remove objectives", &tracked),
        "History registration retry failed");
    require(history.undo() && ids(test.live.storyObjectives) == std::vector<EntityId>{"a", "b", "c", "d"},
        "Scoped multi-delete undo lost full-vector positions");
    require(test.live.storyObjectives[0].title == "Untracked newer text", "Scoped undo captured an untracked edit");
    test.live.storyObjectives[1].title = "Conflicting restored objective";
    const auto saves = test.saves;
    require(!history.redo() && test.saves == saves && history.canRedo(), "Scoped redo overwrote a newer tracked record");
}
#include "world_history_cases.hpp"
#include "project_command_reset_cases.hpp"
}

int main()
{
    try
    {
        const auto root = std::filesystem::temp_directory_path() / ("gm-history-" + EntityMetadata{}.id);
        isolate("TTRPG_DATA_ROOT", root.string());
        isolate("TTRPG_DB_PATH", (root / "TTRPG.db").string());
        isolate("TTRPG_SETTINGS_PATH", (root / "GMEditor.settings").string());
        managerFailures(); planOrderAndUnrelatedChanges(); planFailureAndActiveEntry(); objectiveAndClueChanges(); sceneLinksAreScoped();
        middleRemovalKeepsSceneOrder(); scopedPositionsAndRegistration();
        WorldHistoryCases::run();managerDeferredClearCases();
        std::cout << "PASS campaign edit history: existing 7 groups, 4 World patch groups and deferred-reset cases; isolated callbacks, no UI\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
