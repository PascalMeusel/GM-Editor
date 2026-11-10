#include "Models/Session/TabletopToolkit/TabletopToolkitService.hpp"

#include "Models/Session/SessionControlService.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace
{
constexpr int kMaximumDicePerRoll = 500;
constexpr int kMaximumDieSides = 100000;

bool diceForExpression(const std::string &expression, std::mt19937 &generator,
                       std::vector<int> &rolls, std::string &error)
{
    int totalDice = 0;
    for (std::size_t index = 0; index < expression.size(); ++index)
    {
        if (expression[index] != 'd' && expression[index] != 'D')
            continue;

        std::size_t countStart = index;
        while (countStart > 0 && std::isdigit(static_cast<unsigned char>(expression[countStart - 1])))
            --countStart;
        int count = 1;
        if (countStart < index)
        {
            try { count = std::stoi(expression.substr(countStart, index - countStart)); }
            catch (...) { error = "Invalid dice count."; return false; }
        }

        const std::size_t sidesStart = index + 1;
        std::size_t sidesEnd = sidesStart;
        while (sidesEnd < expression.size() && std::isdigit(static_cast<unsigned char>(expression[sidesEnd])))
            ++sidesEnd;
        if (sidesStart == sidesEnd || count <= 0)
        {
            error = "Invalid dice term.";
            return false;
        }

        int sides = 0;
        try { sides = std::stoi(expression.substr(sidesStart, sidesEnd - sidesStart)); }
        catch (...) { error = "Invalid die size."; return false; }
        if (sides <= 0 || sides > kMaximumDieSides || totalDice + count > kMaximumDicePerRoll)
        {
            error = "Dice expression exceeds the safe roll limit.";
            return false;
        }

        std::uniform_int_distribution<int> die(1, sides);
        for (int rollIndex = 0; rollIndex < count; ++rollIndex)
            rolls.push_back(die(generator));
        totalDice += count;
        index = sidesEnd == 0 ? sidesEnd : sidesEnd - 1;
    }
    if (rolls.empty())
    {
        error = "Enter at least one dice term.";
        return false;
    }
    return true;
}

std::string joinedRolls(const std::vector<int> &rolls)
{
    std::ostringstream stream;
    for (std::size_t index = 0; index < rolls.size(); ++index)
    {
        if (index > 0) stream << ", ";
        stream << rolls[index];
    }
    return stream.str();
}

void appendNeutralSessionNote(Campaign &campaign, const std::string &body, const std::string &tag)
{
    SessionModel *session = SessionControl::activeSession(campaign);
    if (!session) return;

    SessionNoteModel note;
    note.timestampUtc = currentUtcTimestamp();
    note.body = body;
    note.linkedEntityType = tag;
    note.linkedEntityLabel = tag == "safety_pause" ? "Safety" : "Dice";
    note.metadata.tags.push_back(tag);
    session->notes.push_back(std::move(note));
    session->metadata.touch();
}
}

namespace TabletopToolkit
{
bool validDiceMacro(const Campaign &campaign, const CampaignDiceMacroModel &macro, std::string *error)
{
    if (macro.name.empty())
    {
        if (error) *error = "Give the macro a name.";
        return false;
    }
    if (!campaign.ruleSet || !campaign.ruleSet->summarizeDiceExpression(macro.expression).valid)
    {
        if (error) *error = "Enter a valid dice expression such as 1d20+5.";
        return false;
    }
    if (error) error->clear();
    return true;
}

TabletopDiceRollResult roll(Campaign &campaign, const std::string &expression, const std::string &label,
                            bool addToSessionNotes, std::mt19937 &generator)
{
    TabletopDiceRollResult outcome;
    outcome.expression = expression;
    outcome.label = label.empty() ? "Roll" : label;
    outcome.rolledAtUtc = currentUtcTimestamp();
    if (!campaign.ruleSet)
    {
        outcome.error = "No ruleset is active.";
        return outcome;
    }

    const RuleDiceSummary summary = campaign.ruleSet->summarizeDiceExpression(expression);
    if (!summary.valid)
    {
        outcome.error = "Invalid dice expression.";
        return outcome;
    }
    if (!diceForExpression(expression, generator, outcome.rolls, outcome.error))
        return outcome;

    const RuleDiceRollResult evaluated = campaign.ruleSet->evaluateDiceExpression(expression, outcome.rolls);
    if (!evaluated.valid)
    {
        outcome.error = evaluated.error.empty() ? "Invalid dice expression." : evaluated.error;
        return outcome;
    }

    outcome.valid = true;
    outcome.total = evaluated.total;
    outcome.rolls = evaluated.rolls;
    outcome.message = outcome.label + ": " + expression + " = " + std::to_string(outcome.total) +
                      " (" + joinedRolls(outcome.rolls) + ")";
    if (addToSessionNotes)
        appendNeutralSessionNote(campaign, outcome.message, "dice_roll");
    return outcome;
}

bool setObjectiveStatus(StoryObjectiveModel &objective, StoryObjectiveStatus status)
{
    if (objective.status == status) return false;
    objective.status = status;
    objective.metadata.touch();
    return true;
}

bool revealClue(StoryClueModel &clue, bool shareWithPlayers)
{
    const StoryClueStatus target = shareWithPlayers ? StoryClueStatus::Shared : StoryClueStatus::Discovered;
    if (clue.status == target && clue.playerVisible == shareWithPlayers) return false;
    clue.status = target;
    clue.playerVisible = shareWithPlayers;
    if (clue.discoveredAtUtc.empty()) clue.discoveredAtUtc = currentUtcTimestamp();
    clue.metadata.touch();
    return true;
}

bool hideClue(StoryClueModel &clue)
{
    if (clue.status == StoryClueStatus::Hidden && !clue.playerVisible) return false;
    clue.status = StoryClueStatus::Hidden;
    clue.playerVisible = false;
    clue.discoveredAtUtc.clear();
    clue.metadata.touch();
    return true;
}

bool requestSafetyPause(Campaign &campaign)
{
    CampaignSafetyProfileModel &profile = campaign.safetyProfile;
    if (!profile.enabled || profile.pauseActive) return false;
    profile.pauseActive = true;
    profile.pauseRequestedAtUtc = currentUtcTimestamp();
    appendNeutralSessionNote(campaign, "Safety pause requested.", "safety_pause");
    return true;
}

bool clearSafetyPause(Campaign &campaign)
{
    CampaignSafetyProfileModel &profile = campaign.safetyProfile;
    if (!profile.pauseActive) return false;
    profile.pauseActive = false;
    appendNeutralSessionNote(campaign, "Safety pause cleared.", "safety_pause");
    return true;
}

bool startSafetyCheckIn(Campaign &campaign, const std::string &prompt)
{
    CampaignSafetyProfileModel &profile = campaign.safetyProfile;
    if (!profile.enabled || profile.checkInActive) return false;
    profile.checkInPrompt = prompt.empty() ? "Is everyone comfortable continuing?" : prompt;
    profile.checkInActive = true;
    profile.checkInStartedAtUtc = currentUtcTimestamp();
    appendNeutralSessionNote(campaign, "Table check-in started.", "safety_check_in");
    return true;
}

bool clearSafetyCheckIn(Campaign &campaign)
{
    CampaignSafetyProfileModel &profile = campaign.safetyProfile;
    if (!profile.checkInActive) return false;
    profile.checkInActive = false;
    appendNeutralSessionNote(campaign, "Table check-in completed.", "safety_check_in");
    return true;
}

std::vector<PlayerSafeObjective> playerSafeObjectives(const Campaign &campaign)
{
    std::vector<PlayerSafeObjective> result;
    for (const StoryObjectiveModel &objective : campaign.storyObjectives)
        if (objective.playerVisible)
            result.push_back({objective.metadata.id, objective.title, objective.playerSummary, objective.status});
    return result;
}

std::vector<PlayerSafeClue> playerSafeClues(const Campaign &campaign)
{
    std::vector<PlayerSafeClue> result;
    for (const StoryClueModel &clue : campaign.storyClues)
        if (clue.playerVisible && clue.status == StoryClueStatus::Shared)
            result.push_back({clue.metadata.id, clue.title, clue.playerText, clue.discoveredAtUtc});
    return result;
}

std::vector<PlayerSafeSafetyBoundary> playerSafeBoundaries(const Campaign &campaign)
{
    std::vector<PlayerSafeSafetyBoundary> result;
    for (const SafetyBoundaryModel &boundary : campaign.safetyProfile.boundaries)
        if (boundary.active && boundary.shareWithPlayers)
            result.push_back({boundary.kind, boundary.topic, boundary.guidance});
    return result;
}

std::vector<PlayerSafeContentWarning> playerSafeWarnings(const Campaign &campaign, const EntityId &targetEntityId)
{
    std::vector<PlayerSafeContentWarning> result;
    for (const SafetyContentWarningModel &warning : campaign.safetyProfile.warnings)
    {
        if (!warning.shareWithPlayers || (!targetEntityId.empty() && warning.targetEntityId != targetEntityId))
            continue;
        result.push_back({warning.targetEntityId, warning.targetEntityType, warning.targetLabel, warning.warning});
    }
    return result;
}
} // namespace TabletopToolkit
