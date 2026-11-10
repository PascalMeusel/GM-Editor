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
std::string serializeActiveEffects(const std::vector<ActiveEffectModel> &effects)
{
    constexpr char fieldSeparator = '\x1f';
    constexpr char entrySeparator = '\x1e';

    std::stringstream stream;
    for (const ActiveEffectModel &effect : effects)
    {
        stream << effect.metadata.id << fieldSeparator
               << static_cast<int>(effect.kind) << fieldSeparator
               << effect.name << fieldSeparator
               << effect.sourceName << fieldSeparator
               << effect.sourceEntityId << fieldSeparator
               << effect.sourceSpellId << fieldSeparator
               << effect.notes << fieldSeparator
               << effect.remainingRounds << fieldSeparator
               << (effect.concentration ? 1 : 0) << fieldSeparator
               << (effect.active ? 1 : 0) << fieldSeparator
               << effect.metadata.createdAtUtc << fieldSeparator
               << effect.metadata.updatedAtUtc << entrySeparator;
    }
    return stream.str();
}

std::vector<ActiveEffectModel> deserializeActiveEffects(const std::string &serializedEffects)
{
    constexpr char fieldSeparator = '\x1f';
    constexpr char entrySeparator = '\x1e';

    std::vector<ActiveEffectModel> effects;
    std::stringstream entryStream(serializedEffects);
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

        ActiveEffectModel effect;
        if (!fields.empty() && !fields[0].empty())
            effect.metadata.id = fields[0];
        try
        {
            if (fields.size() > 1)
                effect.kind = static_cast<ActiveEffectKind>(std::clamp(std::stoi(fields[1]), 0, static_cast<int>(ActiveEffectKind::Other)));
        }
        catch (const std::exception &)
        {
            effect.kind = ActiveEffectKind::Other;
        }
        if (fields.size() > 2)
            effect.name = fields[2];
        if (fields.size() > 3)
            effect.sourceName = fields[3];
        if (fields.size() > 4)
            effect.sourceEntityId = fields[4];
        if (fields.size() > 5)
            effect.sourceSpellId = fields[5];
        if (fields.size() > 6)
            effect.notes = fields[6];
        try
        {
            if (fields.size() > 7)
                effect.remainingRounds = std::stoi(fields[7]);
        }
        catch (const std::exception &)
        {
            effect.remainingRounds = -1;
        }
        if (fields.size() > 8)
            effect.concentration = fields[8] == "1";
        if (fields.size() > 9)
            effect.active = fields[9] != "0";
        if (fields.size() > 10 && !fields[10].empty())
            effect.metadata.createdAtUtc = fields[10];
        if (fields.size() > 11 && !fields[11].empty())
            effect.metadata.updatedAtUtc = fields[11];
        if (!effect.name.empty() && effect.active)
            effects.push_back(effect);
    }
    return effects;
}

std::string serializeResourceTrackers(const std::vector<ResourceTrackerModel> &trackers)
{
    constexpr char fieldSeparator = '\x1f';
    constexpr char entrySeparator = '\x1e';

    std::stringstream stream;
    for (ResourceTrackerModel tracker : trackers)
    {
        normalizeResourceTracker(tracker);
        stream << tracker.metadata.id << fieldSeparator
               << tracker.name << fieldSeparator
               << tracker.maximum << fieldSeparator
               << tracker.used << fieldSeparator
               << tracker.rechargeRule << fieldSeparator
               << tracker.notes << fieldSeparator
               << tracker.metadata.createdAtUtc << fieldSeparator
               << tracker.metadata.updatedAtUtc << fieldSeparator
               << tracker.definitionId << entrySeparator;
    }
    return stream.str();
}

std::vector<ResourceTrackerModel> deserializeResourceTrackers(const std::string &serializedTrackers)
{
    constexpr char fieldSeparator = '\x1f';
    constexpr char entrySeparator = '\x1e';

    std::vector<ResourceTrackerModel> trackers;
    std::stringstream entryStream(serializedTrackers);
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

        ResourceTrackerModel tracker;
        if (!fields.empty() && !fields[0].empty())
            tracker.metadata.id = fields[0];
        if (fields.size() > 1)
            tracker.name = fields[1];
        try
        {
            if (fields.size() > 2)
                tracker.maximum = std::stoi(fields[2]);
            if (fields.size() > 3)
                tracker.used = std::stoi(fields[3]);
        }
        catch (const std::exception &)
        {
            tracker.maximum = 1;
            tracker.used = 0;
        }
        if (fields.size() > 4)
            tracker.rechargeRule = fields[4];
        if (fields.size() > 5)
            tracker.notes = fields[5];
        if (fields.size() > 6 && !fields[6].empty())
            tracker.metadata.createdAtUtc = fields[6];
        if (fields.size() > 7 && !fields[7].empty())
            tracker.metadata.updatedAtUtc = fields[7];
        if (fields.size() > 8)
            tracker.definitionId = fields[8];
        normalizeResourceTracker(tracker);
        trackers.push_back(tracker);
    }
    return trackers;
}

std::string serializeCombatLog(const std::vector<CombatLogEntryModel> &entries)
{
    constexpr char fieldSeparator = '\x1f';
    constexpr char entrySeparator = '\x1e';

    std::stringstream stream;
    for (const CombatLogEntryModel &entry : entries)
    {
        stream << entry.metadata.id << fieldSeparator
               << entry.round << fieldSeparator
               << entry.actorName << fieldSeparator
               << entry.eventType << fieldSeparator
               << entry.summary << fieldSeparator
               << entry.metadata.createdAtUtc << fieldSeparator
               << entry.metadata.updatedAtUtc << entrySeparator;
    }
    return stream.str();
}

std::vector<CombatLogEntryModel> deserializeCombatLog(const std::string &serializedEntries)
{
    constexpr char fieldSeparator = '\x1f';
    constexpr char entrySeparator = '\x1e';

    std::vector<CombatLogEntryModel> entries;
    std::stringstream entryStream(serializedEntries);
    std::string entryValue;
    while (std::getline(entryStream, entryValue, entrySeparator))
    {
        if (entryValue.empty())
            continue;

        std::stringstream fieldStream(entryValue);
        std::vector<std::string> fields;
        std::string field;
        while (std::getline(fieldStream, field, fieldSeparator))
            fields.push_back(field);

        CombatLogEntryModel entry;
        if (!fields.empty() && !fields[0].empty())
            entry.metadata.id = fields[0];
        try
        {
            if (fields.size() > 1)
                entry.round = std::max(1, std::stoi(fields[1]));
        }
        catch (const std::exception &)
        {
            entry.round = 1;
        }
        if (fields.size() > 2)
            entry.actorName = fields[2];
        if (fields.size() > 3)
            entry.eventType = fields[3];
        if (fields.size() > 4)
            entry.summary = fields[4];
        if (fields.size() > 5 && !fields[5].empty())
            entry.metadata.createdAtUtc = fields[5];
        if (fields.size() > 6 && !fields[6].empty())
            entry.metadata.updatedAtUtc = fields[6];
        if (!entry.summary.empty())
            entries.push_back(entry);
    }
    return entries;
}

std::string serializeDiceRollPresets(const std::vector<DiceRollPresetModel> &presets)
{
    constexpr char fieldSeparator = '\x1f';
    constexpr char entrySeparator = '\x1e';

    std::stringstream stream;
    for (const DiceRollPresetModel &preset : presets)
    {
        stream << preset.metadata.id << fieldSeparator
               << preset.name << fieldSeparator
               << preset.expression << fieldSeparator
               << preset.notes << fieldSeparator
               << preset.metadata.createdAtUtc << fieldSeparator
               << preset.metadata.updatedAtUtc << entrySeparator;
    }
    return stream.str();
}

std::vector<DiceRollPresetModel> deserializeDiceRollPresets(const std::string &serializedPresets)
{
    constexpr char fieldSeparator = '\x1f';
    constexpr char entrySeparator = '\x1e';

    std::vector<DiceRollPresetModel> presets;
    std::stringstream entryStream(serializedPresets);
    std::string entryValue;
    while (std::getline(entryStream, entryValue, entrySeparator))
    {
        if (entryValue.empty())
            continue;

        std::stringstream fieldStream(entryValue);
        std::vector<std::string> fields;
        std::string field;
        while (std::getline(fieldStream, field, fieldSeparator))
            fields.push_back(field);

        DiceRollPresetModel preset;
        if (!fields.empty() && !fields[0].empty())
            preset.metadata.id = fields[0];
        if (fields.size() > 1)
            preset.name = fields[1];
        if (fields.size() > 2)
            preset.expression = fields[2];
        if (fields.size() > 3)
            preset.notes = fields[3];
        if (fields.size() > 4 && !fields[4].empty())
            preset.metadata.createdAtUtc = fields[4];
        if (fields.size() > 5 && !fields[5].empty())
            preset.metadata.updatedAtUtc = fields[5];
        if (!preset.name.empty() && !preset.expression.empty())
            presets.push_back(preset);
    }
    return presets;
}
}
