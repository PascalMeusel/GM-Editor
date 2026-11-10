#include "tabletop_history_test_support.hpp"

namespace
{
using namespace TabletopHistoryTests;
using Probe = TabletopHistoryProbe;

template<bool Clues> void orderAndAppend()
{
    Fixture fixture; auto &history = ProjectCommandManager::getInstance();
    const auto original = ids(fixture.records<Clues>());
    Probe::remove<Clues>(fixture.editor, 1);
    require(Probe::toolkit(fixture.editor).persist("remove tracked middle record"), "Controller removal did not save");
    require(fixture.diskIds<Clues>() == std::vector<EntityId>{original[0], original[2]}, "Removal did not persist exact order");
    require(history.undo() && ids(fixture.records<Clues>()) == original && fixture.diskIds<Clues>() == original,
        "Controller undo did not restore the middle position on disk");
    require(history.redo() && fixture.diskIds<Clues>() == std::vector<EntityId>{original[0], original[2]}, "Controller removal redo changed order");
    require(history.undo(), "Could not restore middle record before append");
    history.clear();
    const auto added = Probe::append<Clues>(fixture.editor);
    auto expected = original; expected.push_back(added);
    require(ids(fixture.records<Clues>()) == expected && fixture.diskIds<Clues>() == expected, "New record did not append");
    require(history.undo() && fixture.diskIds<Clues>() == original, "Append undo lost existing records");
    require(history.redo() && fixture.diskIds<Clues>() == expected, "Append redo inserted at the front");
}

template<bool Clues> void normalCommitsAndFailures()
{
    Fixture fixture; auto &history = ProjectCommandManager::getInstance();
    Probe::edit<Clues>(fixture.editor, 1, "First committed text");
    fixture.records<Clues>()[0].title = "Untracked text must survive";
    {
        RejectWrites reject(Clues);
        require(!fixture.save() && !history.canUndo() && Probe::pending(fixture.editor), "Failed root save consumed pending history");
        const auto disk = fixture.disk();
        require((Clues ? disk.storyClues[1].title : disk.storyObjectives[1].title) == "B", "Failed root save changed durable text");
    }
    require(fixture.save() && history.canUndo() && !Probe::pending(fixture.editor), "Root save retry did not register history");
    require(history.undoLabel() == (Clues ? "Edit clue" : "Edit objective"), "Root save exposed an unhelpful save-reason label");
    require(fixture.save(), "Repeated clean root save failed");
    {
        RejectWrites reject(Clues);
        require(!history.undo() && history.canUndo() && !history.canRedo() && fixture.records<Clues>()[1].title == "First committed text",
            "Failed undo changed live state or lost its command");
    }
    require(history.undo() && fixture.records<Clues>()[1].title == "B" && fixture.records<Clues>()[0].title == "Untracked text must survive",
        "Undo retry lost original or unrelated text");
    require(!history.canUndo(), "Clean root save or duplicate owner notification added a duplicate command");
    require(history.redo(), "Redo after save retry failed");
    Probe::edit<Clues>(fixture.editor, 1, "Second committed text", true);
    require(fixture.save(), "Second root commit failed");
    require(history.undo() && fixture.records<Clues>()[1].title == "First committed text", "Consecutive commits collapsed into one history entry");
    require(history.undo() && fixture.records<Clues>()[1].title == "B" && !history.canUndo(), "First commit was duplicated or lost");
    require(history.redo(), "Could not redo first commit for conflict check");
    fixture.records<Clues>()[1].title = "Newer conflicting text";
    require(!history.undo() && fixture.records<Clues>()[1].title == "Newer conflicting text", "Undo overwrote a newer tracked edit");
}

template<bool Clues> void sharedOwners()
{
    Fixture fixture; auto &history = ProjectCommandManager::getInstance();
    Probe::edit<Clues>(fixture.editor, 1, "Live intermediate");
    // Joining an already pending scope must not reset its earliest before image.
    TabletopToolkitController lateOwner(fixture.live(), Probe::shared(fixture.editor));
    Probe::edit<Clues>(fixture.editor, 1, "GM final", true);
    Probe::edit<Clues>(fixture.editor, 2, "Second owner different record", true);
    {
        RejectWrites reject(Clues);
        require(!fixture.save() && Probe::pending(fixture.editor), "Shared failed save lost pending ownership");
    }
    require(fixture.save(), "Shared owner save retry failed");
    require(history.undo() && fixture.records<Clues>()[1].title == "B" && fixture.records<Clues>()[2].title == "C" && !history.canUndo(),
        "Two toolkit owners recorded overlapping or duplicated transactions");
    require(history.redo() && fixture.records<Clues>()[1].title == "GM final" && fixture.records<Clues>()[2].title == "Second owner different record",
        "Shared owner redo lost final values");
    require(lateOwner.commitPendingHistory() && !history.canRedo(), "Late owner consumed or duplicated a cleared scope");
    require(history.undo() && !history.canUndo(), "Late owner produced a duplicate command");
}

void autosaveBoundaryAndBusyRetry()
{
    Fixture fixture; auto &history = ProjectCommandManager::getInstance();
    std::chrono::steady_clock::time_point now{};
    ProjectSaveService autosave(ProjectSaveDependencies{[](const Campaign &) {}, [&] { return now; }});
    autosave.setBackupSnapshotsEnabled(false); autosave.setRecoverySnapshotsEnabled(false);
    autosave.setAutosaveEnabled(true); autosave.setAutosaveIntervalSeconds(10);
    Probe::edit<false>(fixture.editor, 1, "Autosaved objective");
    Probe::edit<true>(fixture.editor, 1, "Autosaved clue", true);
    autosave.markDirty("synthetic ordinary text input");
    now += std::chrono::seconds(9);
    require(!Probe::autosave(fixture.editor, autosave) && !history.canUndo(), "Autosave boundary registered before a commit");
    now += std::chrono::seconds(1);
    {
        RejectWrites reject(false);
        require(!Probe::autosave(fixture.editor, autosave) && !history.canUndo() && Probe::pending(fixture.editor),
            "Failed autosave consumed pending history");
    }
    now += std::chrono::seconds(10);
    require(Probe::autosave(fixture.editor, autosave) && !Probe::pending(fixture.editor), "Successful autosave boundary missed history");
    require(!Probe::autosave(fixture.editor, autosave), "Clean autosave retriggered");
    require(history.undo() && fixture.live().storyClues[1].title == "B", "GM Screen clue autosave undo failed");
    require(history.undo() && fixture.live().storyObjectives[1].title == "B" && !history.canUndo(), "Live objective autosave undo failed or duplicated");
    history.clear();
    Probe::edit<false>(fixture.editor, 1, "Save while manager busy");
    history.execute(std::make_unique<LambdaProjectCommand>("Busy outer command", [&] {
        require(fixture.save(), "Durable save inside outer command failed");
        require(Probe::pending(fixture.editor), "Busy registration consumed the pending baseline");
    }, [] {}));
    require(history.undoLabel() == "Busy outer command" && Probe::pending(fixture.editor), "Busy outer command did not complete");
    require(fixture.save() && !Probe::pending(fixture.editor), "Pending history did not register on later save");
    require(history.undo() && fixture.live().storyObjectives[1].title == "B" && history.undoLabel() == "Busy outer command",
        "Busy registration retry lost its original before image or duplicated history");
}

void stagedCopiesAndCampaignReset()
{
    Fixture fixture; auto &history = ProjectCommandManager::getInstance();
    Probe::edit<false>(fixture.editor, 1, "Retained live edit");
    Campaign staged = fixture.live(); staged.storyObjectives[1].title = "Different staged snapshot";
    require(ProjectSaveService::getInstance().saveCampaign(staged, "independent staged mutation"), "Staged fixture save failed");
    require(!history.canUndo() && Probe::pending(fixture.editor), "Same-ID staged save falsely registered live pending bytes");
    require(fixture.save() && history.undo() && fixture.live().storyObjectives[1].title == "B", "Live commit after staged save lost its history");
    Probe::edit<true>(fixture.editor, 0, "Discarded on campaign reload", true);
    fixture.editor.setCurrentCampaign(fixture.disk());
    require(!Probe::pending(fixture.editor) && !history.canUndo() && !history.canRedo() && fixture.live().storyClues[0].title == "A",
        "Campaign reload retained another live instance's pending history");
    Campaign unrelated = fixture.live();
    bool rejected = false;
    try { TabletopToolkitController wrongOwner(unrelated, Probe::shared(fixture.editor)); }
    catch (const std::invalid_argument &) { rejected = true; }
    require(rejected, "Pending history can be shared by distinct live campaign objects");
}
}

int main()
{
    try
    {
        const auto root = std::filesystem::temp_directory_path() / createEntityId("tabletop-history-tests");
        std::filesystem::create_directories(root);
        environment("TTRPG_DATA_ROOT", root.string()); environment("TTRPG_DB_PATH", (root / "TTRPG.db").string());
        environment("TTRPG_SETTINGS_PATH", (root / "settings.ini").string());
        auto *context = ImGui::CreateContext(); ImGui::GetIO().IniFilename = nullptr;
        gm::localization::LocalizationService::instance().setPreference(gm::localization::LocalePreference::English, false);
        auto &service = ProjectSaveService::getInstance(); service.setBackupSnapshotsEnabled(false); service.setRecoverySnapshotsEnabled(false);
        orderAndAppend<false>(); orderAndAppend<true>(); normalCommitsAndFailures<false>(); normalCommitsAndFailures<true>();
        sharedOwners<false>(); sharedOwners<true>(); autosaveBoundaryAndBusyRetry(); stagedCopiesAndCampaignReset();
        ImGui::DestroyContext(context);
        std::cout << "PASS tabletop history controllers: 8 groups; isolated SQLite, no windows/rendering/audio\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
