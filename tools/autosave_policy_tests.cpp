#include "Models/Project/ApplicationPaths.hpp"
#include "Models/Project/AtomicProjectFile.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"

#include <SQLiteCpp/SQLiteCpp.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
void require(bool condition, const std::string &message)
{
    if (!condition) throw std::runtime_error(message);
}

void setEnvironment(const char *name, const std::string &value)
{
#if defined(_WIN32)
    require(_putenv_s(name, value.c_str()) == 0, "Could not isolate autosave fixture");
#else
    require(setenv(name, value.c_str(), 1) == 0, "Could not isolate autosave fixture");
#endif
}

struct Fixture
{
    std::chrono::steady_clock::time_point clock{};
    int commits = 0;
    Campaign campaign;
    ProjectSaveService service;

    explicit Fixture(const std::string &name)
        : service(ProjectSaveDependencies{[this](const Campaign &) { ++commits; }, [this] { return clock; }})
    {
        campaign.metadata.id = "autosave_fixture_" + name;
        campaign.name = "Generated autosave fixture " + name;
        campaign.description = "Original durable content";
        campaign.ruleSet = std::make_shared<DND5e>();
        service.setAutosaveEnabled(true);
        service.setAutosaveIntervalSeconds(10);
        service.setBackupSnapshotsEnabled(false);
        service.setRecoverySnapshotsEnabled(false);
        require(service.saveCampaign(campaign, "initial fixture"), "Could not save initial autosave fixture");
        commits = 0;
    }

    void at(int seconds) { clock = std::chrono::steady_clock::time_point{} + std::chrono::seconds(seconds); }
    void edit(const std::string &reason = "continuing edit")
    {
        campaign.description = reason;
        service.markDirty(reason);
    }
    std::string savedDescription()
    {
        SQLite::Database db(ApplicationPaths::databasePath().string(), SQLite::OPEN_READONLY);
        SQLite::Statement query(db, "SELECT description FROM campaigns WHERE stable_id = ?;");
        query.bind(1, campaign.metadata.id);
        require(query.executeStep(), "Saved autosave fixture disappeared");
        return query.getColumn(0).getString();
    }
};

void testIdleThreshold()
{
    Fixture test("idle");
    test.edit("Idle edit");
    test.at(9);
    require(!test.service.updateAutosave(test.campaign) && test.commits == 0, "Autosave ran before ten idle seconds");
    test.at(10);
    require(test.service.updateAutosave(test.campaign) && test.commits == 1, "Ten-second idle autosave did not commit");
    require(!test.service.state().dirty && test.savedDescription() == "Idle edit", "Idle autosave lost edited content");
    require(!test.service.updateAutosave(test.campaign), "Clean campaign autosaved again");

    test.service.setAutosaveIntervalSeconds(25);
    test.edit("Configured idle edit");
    test.at(34);
    require(!test.service.updateAutosave(test.campaign), "Configured idle interval was ignored");
    test.at(35);
    require(test.service.updateAutosave(test.campaign), "Configured idle interval did not autosave");
}

void testContinuousActivityAndSuccessfulReset()
{
    Fixture test("continuous");
    test.edit();
    for (int second = 1; second < 60; ++second)
    {
        test.at(second);
        test.edit();
        require(!test.service.updateAutosave(test.campaign), "Continuous activity autosaved before its deadline");
    }
    test.at(60);
    test.edit("First sixty-second draft");
    require(test.service.updateAutosave(test.campaign) && test.commits == 1,
            "Continuous editing postponed autosave beyond sixty seconds");
    require(test.savedDescription() == "First sixty-second draft", "Deadline save lost the latest edit");
    for (int second = 61; second < 121; ++second)
    {
        test.at(second);
        test.edit("Second continuing draft");
        require(!test.service.updateAutosave(test.campaign), "Successful commit did not reset oldest dirty age");
    }
    test.at(121);
    test.edit("Second sixty-second draft");
    require(test.service.updateAutosave(test.campaign) && test.commits == 2,
            "Second continuous editing interval did not get its own deadline");
}

void testDisabledAndDiscarded()
{
    Fixture test("disabled");
    test.service.setAutosaveEnabled(false);
    test.edit("Disabled autosave draft");
    test.at(120);
    require(!test.service.updateAutosave(test.campaign) && test.commits == 0 && test.service.state().dirty,
            "Disabled autosave committed or discarded dirty content");
    test.service.setAutosaveEnabled(true);
    require(test.service.updateAutosave(test.campaign), "Re-enabling did not protect an overdue draft");
    test.edit("Discarded dirty marker");
    test.at(179);
    test.service.discardUnsavedChanges();
    for (int second = 179; second < 239; ++second)
    {
        test.at(second);
        test.edit("Draft after discard");
        require(!test.service.updateAutosave(test.campaign), "Discard did not reset the next dirty deadline");
    }
    test.at(239);
    test.edit("Draft after discard");
    require(test.service.updateAutosave(test.campaign), "Draft after discard failed its own dirty deadline");
}

void testFailureRetainsAgeAndThrottlesRetry()
{
    Fixture test("failed");
    SQLite::Database db(ApplicationPaths::databasePath().string(), SQLite::OPEN_READWRITE);
    db.exec("CREATE TRIGGER reject_autosave BEFORE UPDATE ON campaigns "
            "WHEN NEW.stable_id = 'autosave_fixture_failed' "
            "BEGIN SELECT RAISE(ABORT, 'injected autosave failure'); END;");
    test.edit("Original pending draft");
    for (int second = 1; second <= 60; ++second)
    {
        test.at(second);
        test.edit("Original pending draft");
    }
    require(!test.service.updateAutosave(test.campaign) && !test.service.state().lastError.empty(),
            "Injected autosave failure was not reported");
    require(test.service.state().dirty && test.commits == 0 && test.savedDescription() == "Original durable content",
            "Failed autosave cleared dirty content or changed durable content");
    db.exec("DROP TRIGGER reject_autosave;");
    // The disk is writable again. An unthrottled retry would now visibly commit.
    for (int second = 60; second < 70; ++second)
    {
        test.at(second);
        test.edit("New edit after failed autosave");
        require(!test.service.updateAutosave(test.campaign) && test.commits == 0,
                "Failed autosave was retried before the ten-second backoff");
    }
    require(std::find(test.service.state().pendingChangeSummary.begin(), test.service.state().pendingChangeSummary.end(),
                      "New edit after failed autosave") != test.service.state().pendingChangeSummary.end(),
            "New edits after failure were discarded");
    test.at(70);
    test.edit("Latest retry draft");
    require(test.service.updateAutosave(test.campaign) && test.commits == 1,
            "Failure reset oldest dirty age or did not retry after backoff");
    require(!test.service.state().dirty && test.service.state().lastError.empty() &&
                test.savedDescription() == "Latest retry draft",
            "Successful retry did not commit the latest retained draft");
}
}

int main()
{
    try
    {
        const auto root = std::filesystem::temp_directory_path() / ("gm-autosave-policy-" + AtomicProjectFile::uniqueSuffix());
        std::filesystem::create_directories(root);
        setEnvironment("TTRPG_DATA_ROOT", root.string());
        setEnvironment("TTRPG_DB_PATH", (root / "TTRPG.db").string());
        setEnvironment("TTRPG_SETTINGS_PATH", (root / "GMEditor.settings").string());
        testIdleThreshold();
        testContinuousActivityAndSuccessfulReset();
        testDisabledAndDiscarded();
        testFailureRetainsAgeAndThrottlesRetry();
        std::cout << "Autosave idle, maximum dirty age, reset, disable and failure retry policies passed without sleeps.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
