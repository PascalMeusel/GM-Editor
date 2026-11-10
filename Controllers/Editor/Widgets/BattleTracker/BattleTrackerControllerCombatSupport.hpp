#pragma once
#include "BattleTrackerController.hpp"
#include "Models/Combat/EncounterDraftPersistence.hpp"
#include "Models/Audio/AudioTriggerService.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Rulesets/CharacterRulesService.hpp"
#include "Models/Session/InWorldTimeService.hpp"
#include "Models/Session/SessionControlService.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <random>
#include <sstream>
#include <string_view>
#include <utility>


namespace
{
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

std::string localizedRuleReason(const std::string &reason)
{
    const auto between = [&](std::string_view prefix, std::string_view suffix) -> std::string {
        if (reason.size() < prefix.size() + suffix.size() || reason.compare(0, prefix.size(), prefix) != 0 ||
            reason.compare(reason.size() - suffix.size(), suffix.size(), suffix) != 0)
            return {};
        return reason.substr(prefix.size(), reason.size() - prefix.size() - suffix.size());
    };

    if (const std::string level = between("No level ", " slots are configured."); !level.empty())
        return combatFormat("No level {0} slots are configured.", {level});
    if (const std::string level = between("No unused level ", " slots remain."); !level.empty())
        return combatFormat("No unused level {0} slots remain.", {level});
    if (reason.rfind("Level ", 0) == 0)
    {
        const std::size_t divider = reason.find(" slots left: ");
        if (divider != std::string::npos && reason.back() == '.')
        {
            const std::string level = reason.substr(6, divider - 6);
            const std::string count = reason.substr(divider + 13, reason.size() - divider - 14);
            return combatFormat("Level {0} slots left: {1}.", {level, count});
        }
    }
    return combatText(reason.c_str());
}

std::string localizedConditionLabel(const std::string &condition)
{
    constexpr std::string_view spellEffectPrefix = "Spell Effect: ";
    constexpr std::string_view concentratingPrefix = "Concentrating: ";
    constexpr std::string_view reinforcementPrefix = "Reinforcement: Round ";
    if (condition.rfind(spellEffectPrefix, 0) == 0)
        return combatFormat("Spell Effect: {0}", {std::string_view(condition).substr(spellEffectPrefix.size())});
    if (condition.rfind(concentratingPrefix, 0) == 0)
        return combatFormat("Concentrating: {0}", {std::string_view(condition).substr(concentratingPrefix.size())});
    if (condition.rfind(reinforcementPrefix, 0) == 0)
        return combatFormat("Reinforcement: Round {0}", {std::string_view(condition).substr(reinforcementPrefix.size())});
    constexpr std::string_view systemConditions[] = {
        "Blinded", "Charmed", "Deafened", "Exhaustion", "Frightened", "Grappled", "Incapacitated", "Invisible",
        "Paralyzed", "Petrified", "Poisoned", "Prone", "Restrained", "Stunned", "Unconscious", "Hidden"};
    for (const std::string_view systemCondition : systemConditions)
    {
        if (condition == systemCondition)
            return combatText(condition.c_str());
    }
    return condition;
}

int extractFlatDamageModifier(const std::string &damageFormula)
{
    int modifier = 0;
    for (size_t i = 0; i < damageFormula.size(); ++i)
    {
        if (damageFormula[i] != '+' && damageFormula[i] != '-')
            continue;

        const int sign = damageFormula[i] == '+' ? 1 : -1;
        size_t cursor = i + 1;
        while (cursor < damageFormula.size() && std::isspace(static_cast<unsigned char>(damageFormula[cursor])))
            ++cursor;

        size_t numberStart = cursor;
        while (cursor < damageFormula.size() && std::isdigit(static_cast<unsigned char>(damageFormula[cursor])))
            ++cursor;

        if (numberStart == cursor)
            continue;

        size_t nextToken = cursor;
        while (nextToken < damageFormula.size() && std::isspace(static_cast<unsigned char>(damageFormula[nextToken])))
            ++nextToken;
        if (nextToken < damageFormula.size() && (damageFormula[nextToken] == 'd' || damageFormula[nextToken] == 'D'))
            continue;

        modifier += sign * std::stoi(damageFormula.substr(numberStart, cursor - numberStart));
        i = cursor;
    }
    return modifier;
}

int extractSignedModifier(const std::string &value)
{
    for (size_t i = 0; i < value.size(); ++i)
    {
        if (value[i] != '+' && value[i] != '-')
            continue;

        const int sign = value[i] == '+' ? 1 : -1;
        size_t cursor = i + 1;
        while (cursor < value.size() && std::isspace(static_cast<unsigned char>(value[cursor])))
            ++cursor;

        const size_t numberStart = cursor;
        while (cursor < value.size() && std::isdigit(static_cast<unsigned char>(value[cursor])))
            ++cursor;
        if (numberStart != cursor)
            return sign * std::stoi(value.substr(numberStart, cursor - numberStart));
    }
    return 0;
}

std::vector<int> rollDiceForExpression(const std::string &expression)
{
    std::vector<int> rolls;
    std::random_device randomDevice;
    std::mt19937 generator(randomDevice());

    for (size_t index = 0; index < expression.size(); ++index)
    {
        if (expression[index] != 'd' && expression[index] != 'D')
            continue;

        size_t countStart = index;
        while (countStart > 0 && std::isdigit(static_cast<unsigned char>(expression[countStart - 1])))
            --countStart;
        int count = 1;
        if (countStart < index)
            count = std::max(1, std::stoi(expression.substr(countStart, index - countStart)));

        size_t sidesStart = index + 1;
        size_t sidesEnd = sidesStart;
        while (sidesEnd < expression.size() && std::isdigit(static_cast<unsigned char>(expression[sidesEnd])))
            ++sidesEnd;
        if (sidesStart == sidesEnd)
            continue;

        const int sides = std::max(1, std::stoi(expression.substr(sidesStart, sidesEnd - sidesStart)));
        std::uniform_int_distribution<int> die(1, sides);
        count = std::min(count, 200);
        for (int rollIndex = 0; rollIndex < count; ++rollIndex)
            rolls.push_back(die(generator));
        index = sidesEnd;
    }

    return rolls;
}

std::string joinRolls(const std::vector<int> &rolls)
{
    std::stringstream stream;
    for (std::size_t index = 0; index < rolls.size(); ++index)
    {
        if (index > 0)
            stream << ", ";
        stream << rolls[index];
    }
    return stream.str();
}

std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string trimCopy(const std::string &value)
{
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])))
        ++start;
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])))
        --end;
    return value.substr(start, end - start);
}

std::vector<std::string> splitDamageTraits(const std::string &traits)
{
    std::vector<std::string> values;
    std::stringstream stream(traits);
    std::string value;
    while (std::getline(stream, value, ','))
    {
        value = lowerCopy(trimCopy(value));
        if (!value.empty())
            values.push_back(value);
    }
    return values;
}

std::string damageTypeFromText(const std::string &text)
{
    const std::string lower = lowerCopy(text);
    const std::vector<std::string> damageTypes{"acid", "bludgeoning", "cold", "fire", "force", "lightning", "necrotic", "piercing", "poison", "psychic", "radiant", "slashing", "thunder"};
    for (const std::string &damageType : damageTypes)
    {
        if (lower.find(damageType) != std::string::npos)
            return damageType;
    }
    return "";
}

std::string appendContext(std::string message, const std::string &context)
{
    if (context.empty())
        return message;
    return message + " " + context;
}
}
