#pragma once

#include "Models/Rulesets/RulesetDefinitionModel.hpp"

#include <string>

namespace RulesetDefinitionSerialization
{
bool hasStructuredDefinitions(const RulesetDefinitionModel &definition);
std::string serializeDefinitionContent(const RulesetDefinitionModel &definition);
void hydrateDefinitionContent(RulesetDefinitionModel &definition);
}
