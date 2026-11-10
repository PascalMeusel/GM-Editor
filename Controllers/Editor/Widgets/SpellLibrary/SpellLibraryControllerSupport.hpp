#pragma once
#include "Models/Project/CampaignEntityDraft.hpp"
#include "Models/UX/CampaignNavigationResolver.hpp"
#include "SpellLibraryController.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Project/ProjectEventBus.hpp"
#include "Models/Project/ProjectLogger.hpp"
#include "Models/Project/ProjectSaveService.hpp"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>
#include <vector>

namespace
{
std::string uiText(const char *english, const char *german)
{
    return gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German ? german : english;
}

std::string toLowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool containsText(std::string value, std::string needle)
{
    if (needle.empty())
        return true;
    return toLowerCopy(value).find(toLowerCopy(needle)) != std::string::npos;
}

std::string trimCopy(const std::string &value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) { return std::isspace(c) != 0; });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) { return std::isspace(c) != 0; }).base();
    return first >= last ? "" : std::string(first, last);
}

std::string joinLimitedNames(const std::vector<std::string> &names, size_t limit = 5)
{
    std::string result;
    const size_t visible = std::min(names.size(), limit);
    for (size_t i = 0; i < visible; ++i)
    {
        if (!result.empty())
            result += ", ";
        result += names[i];
    }
    if (names.size() > visible)
    {
        if (!result.empty())
            result += ", ";
        result += uiText("and ", "und ") + std::to_string(names.size() - visible) + uiText(" more", " weitere");
    }
    return result;
}

std::vector<std::string> splitCsvLine(const std::string &line)
{
    std::vector<std::string> fields;
    std::string field;
    bool quoted = false;
    for (char c : line)
    {
        if (c == '"')
        {
            quoted = !quoted;
            continue;
        }
        if (c == ',' && !quoted)
        {
            fields.push_back(trimCopy(field));
            field.clear();
            continue;
        }
        field.push_back(c);
    }
    fields.push_back(trimCopy(field));
    return fields;
}

std::string jsonStringField(const std::string &object, const std::string &field)
{
    const std::regex pattern("\"" + field + "\"\\s*:\\s*\"((?:\\\\.|[^\"])*)\"", std::regex::icase);
    std::smatch match;
    if (!std::regex_search(object, match, pattern))
        return "";
    std::string value = match[1].str();
    std::string unescaped;
    for (size_t i = 0; i < value.size(); ++i)
    {
        if (value[i] == '\\' && i + 1 < value.size())
            unescaped.push_back(value[++i]);
        else
            unescaped.push_back(value[i]);
    }
    return unescaped;
}

int jsonIntField(const std::string &object, const std::string &field, int fallback)
{
    const std::regex pattern("\"" + field + "\"\\s*:\\s*(-?\\d+)", std::regex::icase);
    std::smatch match;
    return std::regex_search(object, match, pattern) ? std::stoi(match[1].str()) : fallback;
}

bool jsonBoolField(const std::string &object, const std::string &field, bool fallback)
{
    const std::regex pattern("\"" + field + "\"\\s*:\\s*(true|false|1|0)", std::regex::icase);
    std::smatch match;
    if (!std::regex_search(object, match, pattern))
        return fallback;
    const std::string value = toLowerCopy(match[1].str());
    return value == "true" || value == "1";
}

SpellSchool parseSpellSchool(std::string value)
{
    value = toLowerCopy(trimCopy(value));
    for (int i = 0; i < 8; ++i)
    {
        const SpellSchool school = static_cast<SpellSchool>(i);
        if (toLowerCopy(spellSchoolLabel(school)) == value)
            return school;
    }
    return SpellSchool::Evocation;
}

SpellResolutionType parseSpellResolution(std::string value, const SpellModel &spell)
{
    value = toLowerCopy(trimCopy(value));
    if (value.find("attack") != std::string::npos)
        return SpellResolutionType::SpellAttack;
    if (value.find("save") != std::string::npos || value.find("saving") != std::string::npos)
        return SpellResolutionType::SavingThrow;
    if (value.find("heal") != std::string::npos)
        return SpellResolutionType::Healing;
    if (value.find("summon") != std::string::npos || value.find("zone") != std::string::npos || value.find("area") != std::string::npos)
        return SpellResolutionType::SummonOrZone;
    if (!spell.healing.empty())
        return SpellResolutionType::Healing;
    if (!spell.savingThrowAbility.empty())
        return SpellResolutionType::SavingThrow;
    if (!spell.attackBonus.empty())
        return SpellResolutionType::SpellAttack;
    return SpellResolutionType::Utility;
}

bool stringTruthy(const std::string &value)
{
    const std::string normalized = toLowerCopy(trimCopy(value));
    return normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "y";
}

bool hasDiceExpression(const std::string &value)
{
    static const std::regex dicePattern(R"(\b\d*d\d+\b)", std::regex::icase);
    return std::regex_search(value, dicePattern);
}

bool hasNumber(const std::string &value)
{
    static const std::regex numberPattern(R"(\b\d+\b)");
    return std::regex_search(value, numberPattern);
}

bool looksLikeMalformedDiceText(const std::string &value)
{
    const std::string trimmed = trimCopy(value);
    return !trimmed.empty() && hasNumber(trimmed) && !hasDiceExpression(trimmed);
}

SpellModel spellFromCsvFields(const std::vector<std::string> &header, const std::vector<std::string> &fields)
{
    auto field = [&](const std::string &name) {
        const auto found = std::find_if(header.begin(), header.end(), [&](const std::string &candidate) {
            return toLowerCopy(candidate) == toLowerCopy(name);
        });
        if (found == header.end())
            return std::string{};
        const size_t index = static_cast<size_t>(std::distance(header.begin(), found));
        return index < fields.size() ? fields[index] : std::string{};
    };

    SpellModel spell;
    spell.name = field("name");
    try
    {
        if (!field("level").empty())
            spell.level = std::clamp(std::stoi(field("level")), 0, 9);
    }
    catch (const std::exception &)
    {
        spell.level = 0;
    }
    spell.school = parseSpellSchool(field("school"));
    spell.castingTime = field("casting_time").empty() ? field("castingTime") : field("casting_time");
    spell.range = field("range");
    spell.duration = field("duration");
    spell.concentration = stringTruthy(field("concentration"));
    spell.ritual = stringTruthy(field("ritual"));
    spell.classes = field("classes");
    spell.attackBonus = field("attack_bonus").empty() ? field("attackBonus") : field("attack_bonus");
    spell.savingThrowAbility = field("save").empty() ? field("saving_throw") : field("save");
    spell.damage = field("damage");
    spell.healing = field("healing");
    spell.area = field("area").empty() ? field("target") : field("area");
    spell.effectSummary = field("effect").empty() ? field("effect_summary") : field("effect");
    spell.scaling = field("scaling");
    spell.description = field("description").empty() ? field("text") : field("description");
    spell.resolution = parseSpellResolution(field("resolution"), spell);
    return spell;
}

std::vector<SpellModel> parseImportedSpells(const std::string &text)
{
    std::vector<SpellModel> spells;
    if (text.find('{') != std::string::npos)
    {
        const std::regex objectPattern("\\{[^{}]*\"name\"\\s*:[^{}]*\\}");
        for (std::sregex_iterator it(text.begin(), text.end(), objectPattern), end; it != end; ++it)
        {
            const std::string object = it->str();
            SpellModel spell;
            spell.name = jsonStringField(object, "name");
            spell.level = std::clamp(jsonIntField(object, "level", 0), 0, 9);
            spell.school = parseSpellSchool(jsonStringField(object, "school"));
            spell.castingTime = jsonStringField(object, "castingTime");
            if (spell.castingTime.empty())
                spell.castingTime = jsonStringField(object, "casting_time");
            spell.range = jsonStringField(object, "range");
            spell.duration = jsonStringField(object, "duration");
            spell.concentration = jsonBoolField(object, "concentration", false);
            spell.ritual = jsonBoolField(object, "ritual", false);
            spell.classes = jsonStringField(object, "classes");
            spell.attackBonus = jsonStringField(object, "attackBonus");
            spell.savingThrowAbility = jsonStringField(object, "savingThrowAbility");
            if (spell.savingThrowAbility.empty())
                spell.savingThrowAbility = jsonStringField(object, "save");
            spell.damage = jsonStringField(object, "damage");
            spell.healing = jsonStringField(object, "healing");
            spell.area = jsonStringField(object, "area");
            spell.effectSummary = jsonStringField(object, "effectSummary");
            if (spell.effectSummary.empty())
                spell.effectSummary = jsonStringField(object, "effect");
            spell.scaling = jsonStringField(object, "scaling");
            spell.description = jsonStringField(object, "description");
            if (spell.description.empty())
                spell.description = jsonStringField(object, "text");
            spell.resolution = parseSpellResolution(jsonStringField(object, "resolution"), spell);
            if (!spell.name.empty())
                spells.push_back(spell);
        }
        return spells;
    }

    std::stringstream stream(text);
    std::string line;
    std::vector<std::string> header;
    while (std::getline(stream, line))
    {
        line = trimCopy(line);
        if (line.empty())
            continue;
        const std::vector<std::string> fields = splitCsvLine(line);
        if (header.empty())
        {
            header = fields;
            continue;
        }
        SpellModel spell = spellFromCsvFields(header, fields);
        if (!spell.name.empty())
            spells.push_back(spell);
    }
    return spells;
}

std::string importActionForSpell(const Campaign &campaign, const SpellModel &spell)
{
    const auto existing = std::find_if(campaign.customSpells.begin(), campaign.customSpells.end(), [&](const SpellModel &candidate) {
        return toLowerCopy(candidate.name) == toLowerCopy(spell.name) && candidate.level == spell.level;
    });
    return existing == campaign.customSpells.end()
               ? uiText("Add new campaign spell", "Neuen Kampagnenzauber hinzufügen")
               : uiText("Update matching spell", "Passenden Zauber aktualisieren");
}
}
