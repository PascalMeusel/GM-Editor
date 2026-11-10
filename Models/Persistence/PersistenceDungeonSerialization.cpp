#include "PersistenceSerialization.hpp"

#include <algorithm>
#include <charconv>
#include <exception>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace PersistenceSerialization
{
std::string serializeDungeonRoomConnections(const std::vector<DungeonRoomConnectionModel> &connections)
{
    constexpr char fieldSeparator = '\x1f';
    constexpr char entrySeparator = '\x1e';

    std::stringstream stream;
    for (const DungeonRoomConnectionModel &connection : connections)
    {
        stream << connection.metadata.id << fieldSeparator
               << connection.targetRoomId << fieldSeparator
               << connection.label << fieldSeparator
               << (connection.locked ? 1 : 0) << fieldSeparator
               << (connection.secret ? 1 : 0) << fieldSeparator
               << (connection.discovered ? 1 : 0) << fieldSeparator
               << connection.metadata.createdAtUtc << fieldSeparator
               << connection.metadata.updatedAtUtc << entrySeparator;
    }
    return stream.str();
}

std::vector<DungeonRoomConnectionModel> deserializeDungeonRoomConnections(const std::string &serializedConnections)
{
    constexpr char fieldSeparator = '\x1f';
    constexpr char entrySeparator = '\x1e';

    std::vector<DungeonRoomConnectionModel> connections;
    std::stringstream entryStream(serializedConnections);
    std::string value;
    while (std::getline(entryStream, value, entrySeparator))
    {
        if (value.empty())
            continue;

        std::stringstream fieldStream(value);
        std::vector<std::string> fields;
        std::string field;
        while (std::getline(fieldStream, field, fieldSeparator))
            fields.push_back(field);

        DungeonRoomConnectionModel connection;
        if (!fields.empty() && !fields[0].empty())
            connection.metadata.id = fields[0];
        if (fields.size() > 1)
            connection.targetRoomId = fields[1];
        if (fields.size() > 2)
            connection.label = fields[2];
        if (fields.size() > 3)
            connection.locked = fields[3] == "1";
        if (fields.size() > 4)
            connection.secret = fields[4] == "1";
        if (fields.size() > 5)
            connection.discovered = fields[5] == "1";
        if (fields.size() > 6 && !fields[6].empty())
            connection.metadata.createdAtUtc = fields[6];
        if (fields.size() > 7 && !fields[7].empty())
            connection.metadata.updatedAtUtc = fields[7];
        if (!connection.targetRoomId.empty())
            connections.push_back(connection);
    }
    return connections;
}

std::string serializeMonsterActions(const std::vector<MonsterActionModel> &actions)
{
    constexpr char fieldSeparator = '\x1f';
    constexpr char actionSeparator = '\x1e';

    std::stringstream stream;
    for (const MonsterActionModel &action : actions)
    {
        stream << action.name << fieldSeparator
               << action.reachFeet << fieldSeparator
               << action.rangeFeet << fieldSeparator
               << action.attackBonus << fieldSeparator
               << action.damage << fieldSeparator
               << action.notes << fieldSeparator
               << action.sourceSpellId << fieldSeparator
               << action.spellLevel << fieldSeparator
               << (action.consumesSpellSlot ? 1 : 0) << fieldSeparator
               << action.normalRangeFeet << fieldSeparator
               << action.actionKind << fieldSeparator
               << action.sourceLabel << fieldSeparator
               << action.sourceEntityId << fieldSeparator
               << action.linkedResourceId << actionSeparator;
    }
    return stream.str();
}

std::vector<MonsterActionModel> deserializeMonsterActions(const std::string &serializedActions)
{
    constexpr char fieldSeparator = '\x1f';
    constexpr char actionSeparator = '\x1e';

    std::vector<MonsterActionModel> actions;
    std::stringstream actionStream(serializedActions);
    std::string actionValue;
    while (std::getline(actionStream, actionValue, actionSeparator))
    {
        if (actionValue.empty())
            continue;

        std::stringstream fieldStream(actionValue);
        std::vector<std::string> fields;
        std::string field;
        while (std::getline(fieldStream, field, fieldSeparator))
            fields.push_back(field);

        MonsterActionModel action;
        if (!fields.empty())
            action.name = fields[0];
        try
        {
            if (fields.size() > 1)
                action.reachFeet = std::stoi(fields[1]);
            if (fields.size() > 2)
                action.rangeFeet = std::stoi(fields[2]);
            action.normalRangeFeet = action.rangeFeet;
        }
        catch (const std::exception &)
        {
            action.reachFeet = 5;
            action.rangeFeet = 5;
        }
        if (fields.size() > 3)
            action.attackBonus = fields[3];
        if (fields.size() > 4)
            action.damage = fields[4];
        if (fields.size() > 5)
            action.notes = fields[5];
        if (fields.size() > 6)
            action.sourceSpellId = fields[6];
        try
        {
            if (fields.size() > 7)
                action.spellLevel = std::clamp(std::stoi(fields[7]), 0, 9);
        }
        catch (const std::exception &)
        {
            action.spellLevel = 0;
        }
        if (fields.size() > 8)
            action.consumesSpellSlot = fields[8] == "1";
        try
        {
            if (fields.size() > 9)
                action.normalRangeFeet = std::stoi(fields[9]);
        }
        catch (const std::exception &)
        {
            action.normalRangeFeet = action.rangeFeet;
        }
        if (fields.size() > 10)
            action.actionKind = fields[10].empty() ? "Attack" : fields[10];
        if (fields.size() > 11)
            action.sourceLabel = fields[11];
        if (fields.size() > 12)
            action.sourceEntityId = fields[12];
        if (fields.size() > 13)
            action.linkedResourceId = fields[13];
        actions.push_back(action);
    }
    return actions;
}
}
