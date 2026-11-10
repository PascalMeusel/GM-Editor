#pragma once
#include "NPCOverviewViewDirectorySupport.hpp"

namespace
{
std::string monsterDirectorySearchText(const Campaign &campaign, const MonsterDefinitionModel &monster)
{
    std::string text;
    appendSearchText(text, monster.name);
    appendSearchText(text, monster.type);
    appendSearchText(text, monster.size);
    appendSearchText(text, monster.challengeRating);
    appendSearchText(text, "ac " + std::to_string(monster.armorClass));
    appendSearchText(text, "hp " + std::to_string(monster.hitPoints));
    for (const std::string &tag : monster.encounterRoleTags)
        appendSearchText(text, tag);
    for (const std::string &tag : monster.metadata.tags)
        appendSearchText(text, tag);
    for (const MonsterActionModel &action : monster.actions)
    {
        appendSearchText(text, action.name);
        appendSearchText(text, action.damage);
        appendSearchText(text, action.notes);
    }
    if (npcStoryReferenceCount(campaign, monster.metadata.id, "Monster") > 0)
        appendSearchText(text, "story linked");
    if (npcEncounterReferenceCount(campaign, monster.metadata.id) > 0)
        appendSearchText(text, "combat linked");
    return text;
}

bool monsterMatchesDirectoryFilters(
    const Campaign &campaign,
    const MonsterDefinitionModel &monster,
    const std::string &query,
    bool showArchived,
    bool archived)
{
    if (!showArchived && archived)
        return false;

    const std::string normalizedQuery = trimLowerCopy(query);
    if (normalizedQuery.empty())
        return true;
    return trimLowerCopy(monsterDirectorySearchText(campaign, monster)).find(normalizedQuery) != std::string::npos;
}

std::string roleTagsLabel(const MonsterDefinitionModel &monster)
{
    if (monster.encounterRoleTags.empty())
        return localizedUiLabel("No role tags");

    std::string label;
    for (const std::string &tag : monster.encounterRoleTags)
    {
        if (!label.empty())
            label += ", ";
        label += tag;
    }
    return label;
}

std::string monsterActionDisplayName(const MonsterActionModel &action)
{
    return action.name.empty() ? localizedUiLabel("<Unnamed Action>") : action.name;
}

std::string monsterActionRangeLabel(const MonsterActionModel &action)
{
    if (action.reachFeet > 0 && action.rangeFeet > action.reachFeet)
        return std::to_string(action.reachFeet) + "/" + std::to_string(action.rangeFeet) + " " + localizedUiLabel("ft");
    if (action.rangeFeet > 0)
        return std::to_string(action.rangeFeet) + " " + localizedUiLabel("ft");
    if (action.reachFeet > 0)
        return std::to_string(action.reachFeet) + " " + localizedUiLabel("ft");
    return localizedUiLabel("Varies");
}

}
