#pragma once

#include "Models/Character/CharacterModel.hpp"
#include "Models/Rulesets/IRuleSet.hpp"

#include <algorithm>

// Estimates only a new, uninitialized character's maximum HP. Existing
// character HP remains authoritative and is never recalculated from rules.
inline int estimateNewCharacterMaxHitPoints(const CharacterModel &character, const IRuleSet *ruleset)
{
    const int level = std::max(1, character.level);
    if (ruleset == nullptr)
        return calculateDefaultCharacterMaxHitPoints(character);

    const RuleAdvancementProfile profile = ruleset->advancementProfile(character.selectedClass);
    // A ruleset must explicitly provide a first-level value, and any later
    // level needs an explicit fixed progression. Do not infer dice values.
    if (profile.startingHitPoints <= 0 || (level > 1 && profile.fixedHitPointsPerLevel <= 0))
        return calculateDefaultCharacterMaxHitPoints(character);

    const int modifier = profile.hitPointAbility.empty()
        ? 0
        : ruleset->getAbilityModifier(character.attributes, profile.hitPointAbility);
    const int firstLevel = std::max(1, profile.startingHitPoints + modifier);
    const int laterLevels = level - 1;
    const int laterGain = laterLevels == 0
        ? 0
        : std::max(1, profile.fixedHitPointsPerLevel + modifier) * laterLevels;
    return std::max(1, firstLevel + laterGain);
}
