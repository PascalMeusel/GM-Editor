#pragma once

#include "Models/Rulesets/RulesetDefinitionModel.hpp"

RulesetDefinitionModel createLayeredRulesetDefinition(const RulesetDefinitionModel &baseDefinition, const RulesetDefinitionModel &overrideDefinition);
