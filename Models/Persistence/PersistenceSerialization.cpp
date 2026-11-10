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
std::string serializeAttributes(const std::map<std::string, int> &attributes)
{
    std::stringstream stream;
    for (const auto &attribute : attributes)
        stream << attribute.first << ":" << attribute.second << ";";
    return stream.str();
}

std::map<std::string, int> deserializeAttributes(const std::string &serializedAttributes)
{
    std::map<std::string, int> attributes;
    std::stringstream stream(serializedAttributes);
    std::string item;
    while (std::getline(stream, item, ';'))
    {
        const size_t delimiter = item.find(':');
        if (delimiter != std::string::npos)
            attributes[item.substr(0, delimiter)] = std::stoi(item.substr(delimiter + 1));
    }
    return attributes;
}

std::string serializeStringList(const std::vector<std::string> &values)
{
    std::stringstream stream;
    for (const std::string &value : values)
        stream << value << ";";
    return stream.str();
}

std::vector<std::string> deserializeStringList(const std::string &serializedValues)
{
    std::vector<std::string> values;
    std::stringstream stream(serializedValues);
    std::string item;
    while (std::getline(stream, item, ';'))
    {
        if (!item.empty())
            values.push_back(item);
    }
    return values;
}

std::string serializeCharacterRelationships(const std::vector<CharacterRelationshipModel> &relationships)
{
    constexpr char fieldSeparator = '\x1f';
    constexpr char entrySeparator = '\x1e';

    std::stringstream stream;
    for (const CharacterRelationshipModel &relationship : relationships)
    {
        stream << relationship.metadata.id << fieldSeparator
               << relationship.targetEntityType << fieldSeparator
               << relationship.targetEntityId << fieldSeparator
               << relationship.label << fieldSeparator
               << relationship.notes << fieldSeparator
               << relationship.metadata.createdAtUtc << fieldSeparator
               << relationship.metadata.updatedAtUtc << entrySeparator;
    }
    return stream.str();
}

std::vector<CharacterRelationshipModel> deserializeCharacterRelationships(const std::string &serializedRelationships)
{
    constexpr char fieldSeparator = '\x1f';
    constexpr char entrySeparator = '\x1e';

    std::vector<CharacterRelationshipModel> relationships;
    std::stringstream entryStream(serializedRelationships);
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

        CharacterRelationshipModel relationship;
        if (!fields.empty() && !fields[0].empty())
            relationship.metadata.id = fields[0];
        if (fields.size() > 1)
            relationship.targetEntityType = fields[1];
        if (fields.size() > 2)
            relationship.targetEntityId = fields[2];
        if (fields.size() > 3)
            relationship.label = fields[3];
        if (fields.size() > 4)
            relationship.notes = fields[4];
        if (fields.size() > 5 && !fields[5].empty())
            relationship.metadata.createdAtUtc = fields[5];
        if (fields.size() > 6 && !fields[6].empty())
            relationship.metadata.updatedAtUtc = fields[6];
        if (!relationship.targetEntityId.empty())
            relationships.push_back(relationship);
    }
    return relationships;
}

std::string serializeInventoryEntries(const std::vector<InventoryEntryModel> &entries)
{
    std::stringstream stream;
    for (const InventoryEntryModel &entry : entries)
    {
        stream << entry.itemId << ":" << entry.quantity << ":" << (entry.equipped ? 1 : 0) << ":" << entry.slot
               << ":" << entry.metadata.id << ":" << entry.containerEntryId << ":" << (entry.attuned ? 1 : 0)
               << ":" << entry.charges << ":" << entry.ammunitionRemaining << ":" << entry.durability << ":" << entry.stateNotes << ";";
    }
    return stream.str();
}

std::vector<InventoryEntryModel> deserializeInventoryEntries(const std::string &serializedEntries)
{
    std::vector<InventoryEntryModel> entries;
    std::stringstream stream(serializedEntries);
    std::string item;
    while (std::getline(stream, item, ';'))
    {
        if (item.empty())
            continue;

        std::stringstream entryStream(item);
        std::string itemId;
        std::string quantity;
        std::string equipped;
        std::string slot;
        std::string entryId;
        std::string containerEntryId;
        std::string attuned;
        std::string charges;
        std::string ammunitionRemaining;
        std::string durability;
        std::string stateNotes;

        std::getline(entryStream, itemId, ':');
        std::getline(entryStream, quantity, ':');
        std::getline(entryStream, equipped, ':');
        std::getline(entryStream, slot, ':');
        std::getline(entryStream, entryId, ':');
        std::getline(entryStream, containerEntryId, ':');
        std::getline(entryStream, attuned, ':');
        std::getline(entryStream, charges, ':');
        std::getline(entryStream, ammunitionRemaining, ':');
        std::getline(entryStream, durability, ':');
        std::getline(entryStream, stateNotes, ':');

        if (!itemId.empty())
        {
            InventoryEntryModel entry;
            if (!entryId.empty())
                entry.metadata.id = entryId;
            entry.itemId = itemId;
            entry.quantity = quantity.empty() ? 1 : std::max(1, std::stoi(quantity));
            entry.equipped = equipped == "1";
            entry.slot = slot;
            entry.containerEntryId = containerEntryId;
            entry.attuned = attuned == "1";
            if (!charges.empty())
                entry.charges = std::stoi(charges);
            if (!ammunitionRemaining.empty())
                entry.ammunitionRemaining = std::max(0, std::stoi(ammunitionRemaining));
            if (!durability.empty())
                entry.durability = std::stoi(durability);
            entry.stateNotes = stateNotes;
            entries.push_back(entry);
        }
    }
    return entries;
}

std::string serializeSpellSlots(const std::vector<int> &slots)
{
    std::stringstream stream;
    for (std::size_t index = 0; index < slots.size(); ++index)
    {
        if (index > 0)
            stream << ",";
        stream << slots[index];
    }
    return stream.str();
}

std::vector<int> deserializeSpellSlots(const std::string &serializedSlots)
{
    std::vector<int> slots(10, 0);
    std::stringstream stream(serializedSlots);
    std::string value;
    int index = 0;
    while (std::getline(stream, value, ',') && index < static_cast<int>(slots.size()))
    {
        try
        {
            slots[static_cast<std::size_t>(index)] = std::max(0, std::stoi(value));
        }
        catch (const std::exception &)
        {
            slots[static_cast<std::size_t>(index)] = 0;
        }
        ++index;
    }
    return slots;
}

std::string serializeSpellbookEntries(const std::vector<CharacterSpellEntryModel> &entries)
{
    constexpr char fieldSeparator = '\x1f';
    constexpr char entrySeparator = '\x1e';

    std::stringstream stream;
    for (const CharacterSpellEntryModel &entry : entries)
    {
        stream << entry.spellId << fieldSeparator
               << (entry.prepared ? 1 : 0) << fieldSeparator
               << entry.source << fieldSeparator
               << entry.notes << entrySeparator;
    }
    return stream.str();
}

std::vector<CharacterSpellEntryModel> deserializeSpellbookEntries(const std::string &serializedEntries)
{
    constexpr char fieldSeparator = '\x1f';
    constexpr char entrySeparator = '\x1e';

    std::vector<CharacterSpellEntryModel> entries;
    std::stringstream entryStream(serializedEntries);
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

        CharacterSpellEntryModel entry;
        if (!fields.empty())
            entry.spellId = fields[0];
        if (fields.size() > 1)
            entry.prepared = fields[1] == "1";
        if (fields.size() > 2)
            entry.source = fields[2];
        if (fields.size() > 3)
            entry.notes = fields[3];
        if (!entry.spellId.empty())
            entries.push_back(entry);
    }
    return entries;
}
}
