#pragma once
#include "CharacterAdvancement.hpp"
#include "Models/Persistence/PersistenceSerialization.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include "Models/Rulesets/DefinitionBackedRuleSet.hpp"
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>


namespace CharacterAdvancement
{
namespace
{
std::string lower(std::string s)
{
    for (auto &c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
std::vector<std::string> lines(const std::string &s)
{
    std::vector<std::string> result;
    std::istringstream in(s);
    for (std::string line; std::getline(in, line);)
    {
        auto first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos)
            continue;
        line = line.substr(first, line.find_last_not_of(" \t\r") - first + 1);
        if (std::find(result.begin(), result.end(), line) == result.end())
            result.push_back(line);
    }
    return result;
}
std::string canonical(const std::string &name)
{
    for (const auto &c : DND5e{}.getClasses())
        if (lower(c) == lower(name))
            return c;
    return name;
}
std::string castingAbility(const std::string &name)
{
    auto c = canonical(name);
    if (c == "Wizard")
        return "Intelligence";
    if (c == "Cleric" || c == "Druid" || c == "Ranger")
        return "Wisdom";
    if (c == "Bard" || c == "Sorcerer" || c == "Paladin" || c == "Warlock")
        return "Charisma";
    return {};
}
int score(const CharacterModel &c, const std::string &name)
{
    auto i = c.attributes.find(name);
    return i == c.attributes.end() ? 10 : i->second;
}
bool qualifies(const CharacterModel &c, const std::string &name)
{
    auto has = [&](const char *s) { return score(c, s) >= 13; };
    if (name == "Fighter")
        return has("Strength") || has("Dexterity");
    if (name == "Monk" || name == "Ranger")
        return has("Dexterity") && has("Wisdom");
    if (name == "Paladin")
        return has("Strength") && has("Charisma");
    if (name == "Barbarian")
        return has("Strength");
    if (name == "Rogue")
        return has("Dexterity");
    auto ability = castingAbility(name);
    return !ability.empty() && score(c, ability) >= 13;
}
int classLevel(const CharacterModel &c, const std::string &name)
{
    auto levels = classLevels(c);
    auto i = levels.find(name);
    return i == levels.end() ? 0 : std::clamp(i->second, 0, 20);
}
std::string describeClasses(const std::map<std::string, int> &levels)
{
    std::string result;
    for (const auto &[name, n] : levels)
    {
        if (!result.empty())
            result += " / ";
        result += name + " " + std::to_string(n);
    }
    return result;
}
void appendUnique(std::vector<std::string> &target, const std::vector<std::string> &added)
{
    for (const auto &s : added)
        if (std::find(target.begin(), target.end(), s) == target.end())
            target.push_back(s);
}
void resource(CharacterModel &c, const std::string &id, const std::string &name, int maximum,
              const std::string &recharge)
{
    auto i = std::find_if(c.resourceTrackers.begin(), c.resourceTrackers.end(), [&](const auto &r)
                          { return r.definitionId == id || (r.definitionId.empty() && lower(r.name) == lower(name)); });
    if (i == c.resourceTrackers.end())
    {
        ResourceTrackerModel r;
        r.metadata.id = c.metadata.id + "/" + id;
        r.definitionId = id;
        r.name = name;
        r.maximum = maximum;
        r.rechargeRule = recharge;
        c.resourceTrackers.push_back(r);
    }
    else
    {
        i->definitionId = id;
        i->maximum = maximum;
        i->rechargeRule = recharge;
    }
}
} // namespace
} // namespace CharacterAdvancement
