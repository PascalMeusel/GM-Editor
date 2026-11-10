#include "BattleTrackerController.hpp"
#include "Models/Audio/AudioTriggerService.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include <algorithm>
#include <initializer_list>
#include <sstream>
#include <string_view>
#include <utility>

namespace {
std::string combatText(const char *english)
{
    return gm::localization::trUi(english);
}

std::string combatFormat(const char *englishTemplate, std::initializer_list<std::string_view> arguments)
{
    std::string result = combatText(englishTemplate);
    std::size_t index = 0;
    for (const std::string_view argument : arguments)
    {
        const std::string placeholder = "{" + std::to_string(index++) + "}";
        std::size_t offset = 0;
        while ((offset = result.find(placeholder, offset)) != std::string::npos)
        {
            result.replace(offset, placeholder.size(), argument.data(), argument.size());
            offset += argument.size();
        }
    }
    return result;
}

}

void BattleTrackerController::generateAfterCombatSummary()
{
    // A failed action is retried through Save, never by applying it again.
    if (!canLeaveForPreparation())
        return;
    const bool completingCombat = _draftEncounter.combatPhase != CombatPhase::WrapUp;
    std::vector<std::string> defeatedNames;
    std::vector<std::string> monsterChallengeRatings;
    const std::vector<MonsterDefinitionModel> monsters = getMonsterCatalog();
    for (const EncounterParticipantModel &participant : _draftEncounter.participants)
    {
        if (!participant.defeated)
            continue;
        defeatedNames.push_back(participant.displayName);
        if (participant.kind == CombatantKind::Monster)
        {
            const auto monsterIt = std::find_if(monsters.begin(), monsters.end(), [&](const MonsterDefinitionModel &monster) {
                return monster.metadata.id == participant.monsterDefinitionId || monster.metadata.id == participant.sourceEntityId;
            });
            if (monsterIt != monsters.end())
                monsterChallengeRatings.push_back(monsterIt->challengeRating);
        }
    }

    int experience = 0;
    if (_context.campaign.ruleSet)
    {
        for (const std::string &challengeRating : monsterChallengeRatings)
            experience += _context.campaign.ruleSet->getExperienceForChallengeRating(challengeRating);
    }

    std::stringstream summary;
    summary << combatFormat("Encounter: {0}", {_draftEncounter.name.empty() ? combatText("Untitled Encounter") : _draftEncounter.name}) << "\n";
    summary << combatFormat("Ended at round {0}.", {std::to_string(std::max(1, _draftEncounter.round))}) << "\n";
    summary << combatText("Defeated enemies: ");
    if (defeatedNames.empty())
    {
        summary << combatText("none recorded");
    }
    else
    {
        for (std::size_t index = 0; index < defeatedNames.size(); ++index)
        {
            if (index > 0)
                summary << ", ";
            summary << defeatedNames[index];
        }
    }
    summary << ".\n";
    if (experience > 0)
        summary << combatFormat("XP available: {0}.", {std::to_string(experience)}) << "\n";
    if (!_draftEncounter.afterCombatLootNotes.empty())
        summary << combatFormat("Loot: {0}", {_draftEncounter.afterCombatLootNotes}) << "\n";
    if (!_draftEncounter.afterCombatStoryConsequences.empty())
        summary << combatFormat("Story consequences: {0}", {_draftEncounter.afterCombatStoryConsequences}) << "\n";
    if (!_draftEncounter.afterCombatSessionNotes.empty())
        summary << combatFormat("Session notes: {0}", {_draftEncounter.afterCombatSessionNotes}) << "\n";

    _draftEncounter.afterCombatSummary = summary.str();
    setCombatPhase(CombatPhase::WrapUp);
    if (completingCombat)
    {
        CombatLogEntryModel entry;
        entry.round = std::max(1, _draftEncounter.round);
        entry.actorName = combatText("GM");
        entry.eventType = "After Combat";
        entry.summary = combatText("After-combat summary generated.");
        entry.metadata.touch();
        _draftEncounter.combatLog.push_back(std::move(entry));
    }

    // Persist phase, summary and completion log together. The generic log
    // helper saves on its own and would split this transition into two writes.
    saveEncounter();
    if (_encounterSaveFailed || !completingCombat)
        return;

    std::string triggerError;
    AudioTriggers::playMatchingTriggers(_context.campaign, _context.playbackService, "CombatEvent", _draftEncounter.metadata.id, "Combat Ended", triggerError);
    if (!triggerError.empty())
        _combatErrorMessage = triggerError;
}

std::vector<MonsterDefinitionModel> BattleTrackerController::getMonsterCatalog() const
{
    std::vector<MonsterDefinitionModel> monsters = _context.campaign.customMonsters;
    if (_context.campaign.ruleSet)
    {
        const std::vector<MonsterDefinitionModel> rulesetMonsters = _context.campaign.ruleSet->getMonsters();
        monsters.insert(monsters.end(), rulesetMonsters.begin(), rulesetMonsters.end());
    }
    return monsters;
}
