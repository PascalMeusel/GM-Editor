#include "Models/Random/RandomTableService.hpp"
#include "Models/Project/CampaignReferenceService.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/CampaignSearchIndex.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include "Models/Session/SessionControlService.hpp"
#include "workflow_test_database.hpp"

#include <algorithm>
#include <iostream>
#include <random>
#include <memory>
#include <stdexcept>

namespace
{
void require(bool value, const char *message)
{
    if (!value)
        throw std::runtime_error(message);
}

RandomTableEntryModel entry(const std::string &text, int weight)
{
    RandomTableEntryModel result;
    result.result = text;
    result.weight = weight;
    return result;
}
}

int main()
{
    try
    {
        isolateWorkflowDatabase();
        RandomTables::DiceExpression expression;
        std::string error;
        require(RandomTables::parseDiceExpression("2d6 + 3", expression, error), "Valid dice expression rejected");
        require(expression.count == 2 && expression.sides == 6 && expression.modifier == 3 &&
                    expression.minimum() == 5 && expression.maximum() == 15,
                "Dice expression parsed incorrectly");
        require(!RandomTables::parseDiceExpression("d20", expression, error), "Missing dice count accepted");

        RandomTableModel weighted;
        weighted.name = "Reactions";
        weighted.entries = {entry("Helpful", 1), entry("Wary", 2), entry("Hostile", 1)};
        weighted.entries[1].playerSafe = true;
        weighted.entries[1].linkedEntityId = "npc_watch";
        weighted.entries[1].linkedEntityLabel = "The watch captain";
        require(RandomTables::validate(weighted).empty(), "Valid weighted table rejected");

        std::mt19937 first(42), second(42);
        for (int index = 0; index < 20; ++index)
        {
            const auto left = RandomTables::roll(weighted, first);
            const auto right = RandomTables::roll(weighted, second);
            require(left.rolled && right.rolled && left.roll == right.roll && left.entryId == right.entryId,
                    "Seeded weighted rolls are not deterministic");
        }

        RandomTableModel ranged;
        ranged.name = "Weather";
        ranged.selectionMode = RandomTableSelectionMode::DiceRange;
        ranged.diceExpression = "1d6";
        ranged.entries = {entry("Calm", 1), entry("Rain", 1), entry("Storm", 1)};
        ranged.entries[0].minimumRoll = 1;
        ranged.entries[0].maximumRoll = 2;
        ranged.entries[1].minimumRoll = 3;
        ranged.entries[1].maximumRoll = 5;
        ranged.entries[2].minimumRoll = 6;
        ranged.entries[2].maximumRoll = 6;
        require(RandomTables::validate(ranged).empty(), "Valid dice-range table rejected");
        std::mt19937 rangedGenerator(9);
        const auto rangedRoll = RandomTables::roll(ranged, rangedGenerator);
        require(rangedRoll.rolled && rangedRoll.roll >= 1 && rangedRoll.roll <= 6 && !rangedRoll.result.empty(),
                "Dice-range roll failed");
        require(RandomTables::formatRoll(rangedRoll).find("Weather") != std::string::npos,
                "Formatted roll omitted table name");

        ranged.entries[1].minimumRoll = 2;
        require(!RandomTables::validate(ranged).empty(), "Overlapping ranges accepted");
        weighted.entries[0].weight = 0;
        require(!RandomTables::validate(weighted).empty(), "Zero weight accepted");

        weighted.entries[0].weight = 1;
        Campaign campaign;
        campaign.name = "Generated random table workflow";
        campaign.ruleSet = std::make_shared<DND5e>();
        SessionModel *session = SessionControl::startLiveSession(campaign, "Random table test session");
        require(session != nullptr, "Test session could not be started");
        RandomTableRollResult shareableRoll;
        shareableRoll.rolled = true;
        shareableRoll.tableId = weighted.metadata.id;
        shareableRoll.tableName = weighted.name;
        shareableRoll.entryId = weighted.entries[1].metadata.id;
        shareableRoll.expression = "weighted d4";
        shareableRoll.roll = 2;
        shareableRoll.result = weighted.entries[1].result;
        shareableRoll.gmNotes = "Do not reveal the captain's concern.";
        shareableRoll.playerSafe = true;
        shareableRoll.timestampUtc = currentUtcTimestamp();
        require(RandomTables::appendToActiveSessionNotes(campaign, shareableRoll, error),
                "Player-safe result was not added to active session notes");
        require(campaign.sessions.back().notes.size() == 1 &&
                    campaign.sessions.back().notes[0].body.find("GM:") != std::string::npos,
                "Session-note capture omitted the result or GM context");
        require(RandomTables::createPlayerHandout(campaign, shareableRoll, error),
                "Player-safe result did not create a handout");
        require(campaign.handouts.size() == 1 && campaign.handouts[0].body == shareableRoll.result,
                "Generated handout leaked GM notes or changed the player-safe result");
        shareableRoll.playerSafe = false;
        require(!RandomTables::createPlayerHandout(campaign, shareableRoll, error),
                "GM-only result was allowed to create a player handout");
        NPCModel captain;
        captain.metadata.id = "npc_watch";
        captain.name = "The watch captain";
        campaign.npcs.push_back(captain);
        campaign.randomTables.push_back(weighted);
        CampaignRepository::saveCampaign(campaign);
        const auto campaigns = CampaignRepository::loadAllCampaigns();
        require(campaigns.size() == 1 && campaigns[0].randomTables.size() == 1,
                "Random table was not restored from SQLite");
        const RandomTableModel &restored = campaigns[0].randomTables[0];
        require(restored.name == "Reactions" && restored.entries.size() == 3 &&
                    restored.entries[1].linkedEntityId == "npc_watch" && restored.entries[1].playerSafe,
                "Random table entry fields changed during SQLite round trip");

        CampaignSearchIndex search;
        search.rebuild(campaigns[0]);
        const auto matches = search.search("hostile watch", 10);
        require(std::any_of(matches.begin(), matches.end(), [&](const CampaignSearchResult &match) {
                    return match.document.id == restored.metadata.id;
                }),
                "Random table entries are missing from campaign search");
        const auto backlinks = CampaignReferences::backlinksTo(campaigns[0], "npc_watch");
        require(!backlinks.empty() && backlinks[0].sourceEntityId == restored.metadata.id,
                "Random table linked entity backlink is missing");

        std::cout << "Random tables: dice parsing, rolling, notes, safe handouts, SQLite round trip, search, and backlinks passed.\n";
        return 0;
    }
    catch (const std::exception &exception)
    {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
