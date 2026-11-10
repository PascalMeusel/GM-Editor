#include "Models/Random/RandomTableService.hpp"
#include "Models/Campaign/Campaign.hpp"
#include "Models/Session/SessionControlService.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <sstream>

namespace
{
std::string compactExpression(const std::string &text)
{
    std::string compact;
    compact.reserve(text.size());
    for (const unsigned char character : text)
        if (!std::isspace(character))
            compact.push_back(static_cast<char>(std::tolower(character)));
    return compact;
}

bool parsePositiveInteger(const std::string &text, std::size_t begin, std::size_t end, int &value)
{
    if (begin >= end)
        return false;
    long long parsed = 0;
    for (std::size_t index = begin; index < end; ++index)
    {
        if (!std::isdigit(static_cast<unsigned char>(text[index])))
            return false;
        parsed = parsed * 10 + (text[index] - '0');
        if (parsed > std::numeric_limits<int>::max())
            return false;
    }
    value = static_cast<int>(parsed);
    return value > 0;
}
}

namespace RandomTables
{
bool parseDiceExpression(const std::string &text, DiceExpression &expression, std::string &error)
{
    expression = {};
    const std::string compact = compactExpression(text);
    const std::size_t d = compact.find('d');
    if (d == std::string::npos || compact.find('d', d + 1) != std::string::npos)
    {
        error = "Use dice notation such as 1d20, 2d6+3, or 1d100-10.";
        return false;
    }

    std::size_t modifierPosition = compact.find_first_of("+-", d + 1);
    const std::size_t sidesEnd = modifierPosition == std::string::npos ? compact.size() : modifierPosition;
    if (!parsePositiveInteger(compact, 0, d, expression.count) ||
        !parsePositiveInteger(compact, d + 1, sidesEnd, expression.sides))
    {
        error = "Dice count and sides must be positive whole numbers.";
        return false;
    }
    if (expression.count > 100 || expression.sides > 100000)
    {
        error = "Dice expressions are limited to 100 dice with at most 100000 sides each.";
        return false;
    }

    if (modifierPosition != std::string::npos)
    {
        int magnitude = 0;
        if (!parsePositiveInteger(compact, modifierPosition + 1, compact.size(), magnitude))
        {
            error = "The dice modifier must be a whole number.";
            return false;
        }
        expression.modifier = compact[modifierPosition] == '-' ? -magnitude : magnitude;
    }

    error.clear();
    return true;
}

std::vector<std::string> validate(const RandomTableModel &table)
{
    std::vector<std::string> errors;
    if (table.name.find_first_not_of(" \t\r\n") == std::string::npos)
        errors.push_back("Give the table a name.");
    if (table.entries.empty())
        errors.push_back("Add at least one table entry.");

    DiceExpression dice;
    if (table.selectionMode == RandomTableSelectionMode::DiceRange)
    {
        std::string diceError;
        if (!parseDiceExpression(table.diceExpression, dice, diceError))
            errors.push_back(diceError);
    }

    for (std::size_t index = 0; index < table.entries.size(); ++index)
    {
        const RandomTableEntryModel &entry = table.entries[index];
        const std::string prefix = "Entry " + std::to_string(index + 1) + ": ";
        if (entry.result.find_first_not_of(" \t\r\n") == std::string::npos)
            errors.push_back(prefix + "enter a result.");
        if (table.selectionMode == RandomTableSelectionMode::Weighted && entry.weight < 1)
            errors.push_back(prefix + "weight must be at least 1.");
        if (table.selectionMode == RandomTableSelectionMode::DiceRange && entry.minimumRoll > entry.maximumRoll)
            errors.push_back(prefix + "minimum roll cannot exceed maximum roll.");
        if (table.selectionMode == RandomTableSelectionMode::DiceRange && dice.count > 0 &&
            (entry.minimumRoll < dice.minimum() || entry.maximumRoll > dice.maximum()))
            errors.push_back(prefix + "range must fit the dice expression's possible results.");
    }

    if (table.selectionMode == RandomTableSelectionMode::DiceRange)
    {
        for (std::size_t left = 0; left < table.entries.size(); ++left)
            for (std::size_t right = left + 1; right < table.entries.size(); ++right)
                if (table.entries[left].minimumRoll <= table.entries[right].maximumRoll &&
                    table.entries[right].minimumRoll <= table.entries[left].maximumRoll)
                {
                    errors.push_back("Dice ranges overlap between entries " + std::to_string(left + 1) + " and " +
                                     std::to_string(right + 1) + ".");
                    left = table.entries.size();
                    break;
                }
    }
    return errors;
}

RandomTableRollResult roll(const RandomTableModel &table, std::mt19937 &generator)
{
    RandomTableRollResult rolled;
    rolled.tableId = table.metadata.id;
    rolled.tableName = table.name;
    rolled.timestampUtc = currentUtcTimestamp();
    const auto errors = validate(table);
    if (!errors.empty())
    {
        rolled.error = errors.front();
        return rolled;
    }

    const RandomTableEntryModel *selected = nullptr;
    if (table.selectionMode == RandomTableSelectionMode::Weighted)
    {
        long long total = 0;
        for (const RandomTableEntryModel &entry : table.entries)
            total += entry.weight;
        if (total > std::numeric_limits<int>::max())
        {
            rolled.error = "The combined entry weights are too large.";
            return rolled;
        }
        std::uniform_int_distribution<int> distribution(1, static_cast<int>(total));
        rolled.roll = distribution(generator);
        rolled.expression = "weighted d" + std::to_string(total);
        int cursor = 0;
        for (const RandomTableEntryModel &entry : table.entries)
        {
            cursor += entry.weight;
            if (rolled.roll <= cursor)
            {
                selected = &entry;
                break;
            }
        }
    }
    else
    {
        DiceExpression expression;
        std::string error;
        if (!parseDiceExpression(table.diceExpression, expression, error))
        {
            rolled.error = error;
            return rolled;
        }
        std::uniform_int_distribution<int> die(1, expression.sides);
        rolled.roll = expression.modifier;
        for (int index = 0; index < expression.count; ++index)
            rolled.roll += die(generator);
        rolled.expression = table.diceExpression;
        const auto found = std::find_if(table.entries.begin(), table.entries.end(), [&](const auto &entry) {
            return rolled.roll >= entry.minimumRoll && rolled.roll <= entry.maximumRoll;
        });
        if (found != table.entries.end())
            selected = &*found;
    }

    if (!selected)
    {
        rolled.error = "The roll did not match any table entry.";
        return rolled;
    }

    rolled.rolled = true;
    rolled.entryId = selected->metadata.id;
    rolled.result = selected->result;
    rolled.gmNotes = selected->gmNotes;
    rolled.playerSafe = selected->playerSafe;
    rolled.linkedEntityId = selected->linkedEntityId;
    rolled.linkedEntityType = selected->linkedEntityType;
    rolled.linkedEntityLabel = selected->linkedEntityLabel;
    return rolled;
}

std::string formatRoll(const RandomTableRollResult &result)
{
    if (!result.rolled)
        return result.error;
    std::ostringstream text;
    text << result.tableName << " (" << result.expression << " = " << result.roll << "): " << result.result;
    if (!result.linkedEntityLabel.empty())
        text << " [" << result.linkedEntityLabel << "]";
    return text.str();
}

bool appendToActiveSessionNotes(Campaign &campaign, const RandomTableRollResult &result, std::string &error)
{
    if (!result.rolled)
    {
        error = result.error.empty() ? "Roll the table before adding a note." : result.error;
        return false;
    }
    SessionModel *session = SessionControl::activeSession(campaign);
    if (!session)
    {
        error = "Start or resume a session before adding the result to session notes.";
        return false;
    }
    SessionNoteModel note;
    note.timestampUtc = result.timestampUtc.empty() ? currentUtcTimestamp() : result.timestampUtc;
    note.body = formatRoll(result);
    if (!result.gmNotes.empty())
        note.body += "\nGM: " + result.gmNotes;
    note.linkedEntityId = result.tableId;
    note.linkedEntityType = "random_table";
    note.linkedEntityLabel = result.tableName;
    note.metadata.tags = {"random-table"};
    session->notes.push_back(std::move(note));
    session->metadata.touch();
    campaign.metadata.touch();
    error.clear();
    return true;
}

bool createPlayerHandout(Campaign &campaign, const RandomTableRollResult &result, std::string &error)
{
    if (!result.rolled || !result.playerSafe)
    {
        error = "Only a result marked player-safe can become a handout.";
        return false;
    }
    HandoutModel handout;
    handout.title = result.tableName.empty() ? "Random table result" : result.tableName;
    handout.body = result.result;
    if (!result.linkedEntityLabel.empty())
        handout.body += "\n\n" + result.linkedEntityLabel;
    handout.gmNotes = "Created from " + formatRoll(result);
    handout.metadata.tags = {"random-table", "player-safe"};
    campaign.handouts.push_back(std::move(handout));
    campaign.metadata.touch();
    error.clear();
    return true;
}
}
