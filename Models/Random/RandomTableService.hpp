#pragma once

#include "Models/Random/RandomTableModels.hpp"

#include <random>
#include <string>
#include <vector>

struct Campaign;

namespace RandomTables
{
struct DiceExpression
{
    int count = 0;
    int sides = 0;
    int modifier = 0;

    int minimum() const { return count + modifier; }
    int maximum() const { return count * sides + modifier; }
};

bool parseDiceExpression(const std::string &text, DiceExpression &expression, std::string &error);
std::vector<std::string> validate(const RandomTableModel &table);
RandomTableRollResult roll(const RandomTableModel &table, std::mt19937 &generator);
std::string formatRoll(const RandomTableRollResult &result);
bool appendToActiveSessionNotes(Campaign &campaign, const RandomTableRollResult &result, std::string &error);
bool createPlayerHandout(Campaign &campaign, const RandomTableRollResult &result, std::string &error);
}
