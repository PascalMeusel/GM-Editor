#pragma once

#include "Models/Core/EntityMetadata.hpp"

#include <string>
#include <vector>

enum class RandomTableSelectionMode
{
    Weighted = 0,
    DiceRange = 1
};

struct RandomTableEntryModel
{
    EntityMetadata metadata{"random_table_entry"};
    std::string result;
    std::string gmNotes;
    int weight = 1;
    int minimumRoll = 1;
    int maximumRoll = 1;
    bool playerSafe = false;
    EntityId linkedEntityId;
    std::string linkedEntityType;
    std::string linkedEntityLabel;
};

struct RandomTableModel
{
    EntityMetadata metadata{"random_table"};
    std::string name = "New random table";
    std::string description;
    RandomTableSelectionMode selectionMode = RandomTableSelectionMode::Weighted;
    std::string diceExpression = "1d100";
    std::vector<RandomTableEntryModel> entries;
};

struct RandomTableRollResult
{
    bool rolled = false;
    EntityId tableId;
    std::string tableName;
    EntityId entryId;
    int roll = 0;
    std::string expression;
    std::string result;
    std::string gmNotes;
    bool playerSafe = false;
    EntityId linkedEntityId;
    std::string linkedEntityType;
    std::string linkedEntityLabel;
    std::string timestampUtc;
    std::string error;
};
