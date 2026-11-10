#include "tools/workflow_test_database.hpp"

#include "Models/Project/ApplicationPaths.hpp"
#include "Models/Persistence/DatabaseSchemaVersion.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include "Models/Session/SessionControlService.hpp"
#include "Models/Session/PlayerDisplayReviewService.hpp"
#include "Models/Session/TabletopToolkit/TabletopToolkitService.hpp"

#include <SQLiteCpp/SQLiteCpp.h>

#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>

namespace
{
void require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}
}

int main()
{
    try
    {
        isolateWorkflowDatabase();
        Campaign campaign;
        campaign.name = "Tabletop toolkit fixture";
        campaign.ruleSet = std::make_shared<DND5e>();

        CampaignDiceMacroModel macro;
        macro.name = "Perception";
        macro.expression = "1d20+4";
        macro.notes = "Party lookout";
        campaign.diceMacros.push_back(macro);
        std::string validationError;
        require(TabletopToolkit::validDiceMacro(campaign, macro, &validationError), "Valid dice macro was rejected");

        require(SessionControl::startLiveSession(campaign, "Session 1") != nullptr, "Could not start fixture session");
        std::mt19937 generator(42);
        const TabletopDiceRollResult rolled = TabletopToolkit::roll(campaign, macro.expression, macro.name, true, generator);
        require(rolled.valid && rolled.total >= 5 && rolled.total <= 24, "Dice roll result is outside its valid range");
        require(!campaign.sessions.back().notes.empty() && campaign.sessions.back().notes.back().linkedEntityType == "dice_roll",
                "Live dice roll was not captured in session notes");
        require(!TabletopToolkit::roll(campaign, "1000d6", "Unsafe", false, generator).valid,
                "Unsafe dice count was accepted");

        StoryObjectiveModel objective;
        objective.title = "Find the observatory";
        objective.playerSummary = "Locate the ruined observatory.";
        objective.gmNotes = "The rival party arrives first.";
        objective.playerVisible = true;
        require(TabletopToolkit::setObjectiveStatus(objective, StoryObjectiveStatus::Active), "Objective did not activate");
        campaign.storyObjectives.push_back(objective);

        StoryClueModel clue;
        clue.title = "Astral key";
        clue.playerText = "The bronze key is warm beneath moonlight.";
        clue.gmTruth = "It unlocks the observatory lens.";
        require(TabletopToolkit::revealClue(clue, true), "Clue did not reveal");
        campaign.storyClues.push_back(clue);
        require(TabletopToolkit::playerSafeObjectives(campaign).size() == 1, "Visible objective was omitted");
        require(TabletopToolkit::playerSafeClues(campaign).size() == 1, "Shared clue was omitted");
        require(TabletopToolkit::playerSafeClues(campaign).front().text.find("unlocks") == std::string::npos,
                "GM truth leaked into player-safe clue data");

        SafetyBoundaryModel boundary;
        boundary.kind = SafetyBoundaryKind::Veil;
        boundary.topic = "Body horror";
        boundary.guidance = "Fade to black.";
        boundary.shareWithPlayers = true;
        campaign.safetyProfile.boundaries.push_back(boundary);
        SafetyBoundaryModel privateBoundary;
        privateBoundary.topic = "Private boundary";
        privateBoundary.guidance = "GM-only detail";
        campaign.safetyProfile.boundaries.push_back(privateBoundary);
        SafetyContentWarningModel warning;
        warning.targetEntityId = objective.metadata.id;
        warning.targetEntityType = "story_objective";
        warning.targetLabel = objective.title;
        warning.warning = "Captivity themes";
        campaign.safetyProfile.warnings.push_back(warning);
        require(TabletopToolkit::playerSafeBoundaries(campaign).size() == 1, "Private boundary entered player-safe data");
        require(TabletopToolkit::playerSafeWarnings(campaign, objective.metadata.id).size() == 1,
                "Scoped content warning was not available");
        campaign.localPlayerDisplay.mapOnly = false;
        const PlayerDisplayReviewSnapshot review = PlayerDisplayReview::build(campaign, "en");
        require(review.eligible && review.playerObjectives.size() == 1 && review.playerClues.size() == 1,
                "Story tracker content was not included in the reviewed player payload");
        require(review.playerClues.front().text.find("unlocks") == std::string::npos,
                "Reviewed player payload leaked GM truth");
        campaign.storyClues.front().playerText += " It hums softly.";
        require(PlayerDisplayReview::build(campaign, "en").contentFingerprint != review.contentFingerprint,
                "Player-visible clue changes did not invalidate review");
        require(TabletopToolkit::requestSafetyPause(campaign), "Safety pause did not activate");
        require(campaign.safetyProfile.pauseActive, "Safety pause state was not stored");
        require(campaign.sessions.back().notes.back().body == "Safety pause requested.",
                "Safety note contains unexpected identifying detail");
        require(TabletopToolkit::startSafetyCheckIn(campaign, "Thumbs up, sideways, or down?"), "Check-in did not activate");

        CampaignRepository::saveCampaign(campaign);
        const std::vector<Campaign> loaded = CampaignRepository::loadAllCampaigns();
        require(loaded.size() == 1, "Campaign did not round-trip");
        const Campaign &restored = loaded.front();
        require(restored.diceMacros.size() == 1 && restored.diceMacros.front().expression == "1d20+4",
                "Dice macro did not round-trip");
        require(restored.storyObjectives.size() == 1 && restored.storyObjectives.front().gmNotes == objective.gmNotes,
                "Objective did not round-trip");
        require(restored.storyClues.size() == 1 && restored.storyClues.front().gmTruth == clue.gmTruth,
                "Clue did not round-trip");
        require(restored.safetyProfile.pauseActive && restored.safetyProfile.checkInActive,
                "Safety runtime state did not round-trip");
        require(restored.safetyProfile.boundaries.size() == 2 && restored.safetyProfile.warnings.size() == 1,
                "Safety configuration did not round-trip");

        SQLite::Database database(ApplicationPaths::databasePath().string(), SQLite::OPEN_READONLY);
        require(database.execAndGet("PRAGMA user_version").getInt() == DatabaseSchema::CurrentVersion, "Unexpected schema version");
        require(database.execAndGet("SELECT count(*) FROM schema_migrations WHERE version=27").getInt() == 1,
                "Toolkit schema migration was not recorded");

        std::cout << "Tabletop toolkit behavior and persistence passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
