#pragma once

#include "Models/Character/CharacterModel.hpp"
#include "Models/Inventory/InventoryModels.hpp"
#include "Models/Rulesets/IRuleSet.hpp"

#include <vector>

namespace CharacterRules
{
RuleArmorSummary evaluateArmor(const IRuleSet &ruleset, const CharacterModel &character, const std::vector<ItemModel> &items);
}
