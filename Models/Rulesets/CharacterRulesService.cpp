#include "CharacterRulesService.hpp"

namespace CharacterRules
{
RuleArmorSummary evaluateArmor(const IRuleSet &ruleset, const CharacterModel &character, const std::vector<ItemModel> &items)
{
    RuleArmorInput input;
    input.attributes = character.attributes;
    input.inventory = &character.inventory;
    input.items = &items;
    input.className = character.selectedClass;
    return ruleset.evaluateArmor(input);
}
}
